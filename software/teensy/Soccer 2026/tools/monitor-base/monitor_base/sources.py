"""sources.py — Fuentes de frames de telemetría para la app.

Tres orígenes, misma interfaz:
  • SerialSource  — Teensy real por USB (pyserial). Permite mandar comandos.
  • ReplaySource  — un archivo .jsonl grabado, reproducido a una tasa.
  • SimSource     — el simulador (sin robot).

Cada fuente corre un hilo que parsea líneas y empuja Frames a una cola
thread-safe; la GUI la drena con poll(). Además hay helpers PUROS (sin hilos)
para tests: parse_lines() y read_replay_file().
"""
from __future__ import annotations

import queue
import threading
import time
from typing import Callable, Iterable, Iterator, List, Optional

from .protocol import Frame, ProtocolError, is_telemetry_line, parse_line
from .protocol_top import parse_line_top
from .simulator import Simulator
from .simulator_top import SimulatorTop

# Una "fuente" empuja objetos parseados (Frame de la base o TopFrame del TOP).
# El parser concreto se pasa por inyección (parse_line / parse_line_top).
Parser = Callable[[str], object]


# ── Helpers puros (testeables sin hilos) ─────────────────────────────────────

def parse_lines(lines: Iterable[str],
                on_error: Optional[Callable[[str, ProtocolError], None]] = None
                ) -> Iterator[Frame]:
    """Parsea un iterable de líneas a Frames, salteando las que no son
    telemetría (p.ej. prints de boot '[DOWN] ...') o están corruptas. Si se pasa
    on_error, se la llama con (línea, error) para las líneas de telemetría que
    fallan al parsear."""
    for line in lines:
        if not is_telemetry_line(line):
            continue
        try:
            yield parse_line(line)
        except ProtocolError as e:
            if on_error:
                on_error(line, e)


def read_replay_file(path: str) -> List[Frame]:
    """Lee un archivo .jsonl entero y devuelve la lista de Frames válidos."""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return list(parse_lines(f))


# ── Autodetección de puerto serie (Teensy) ───────────────────────────────────

TEENSY_VID = 0x16C0   # PJRC (Teensy)


def list_serial_ports() -> List[dict]:
    """Lista los puertos serie disponibles con metadatos. [] si no hay pyserial.
    Cada item: {device, description, vid, pid, is_teensy}."""
    try:
        from serial.tools import list_ports  # type: ignore
    except ImportError:
        return []
    out = []
    for p in list_ports.comports():
        out.append({
            "device": p.device,
            "description": p.description or "",
            "vid": p.vid, "pid": p.pid,
            "is_teensy": (p.vid == TEENSY_VID),
        })
    return out


def autodetect_port() -> Optional[str]:
    """Devuelve el puerto que MÁS probablemente es el Teensy, o None.
    Preferencia: VID PJRC (0x16C0) > descripción 'serial/teensy/usb' > único puerto."""
    ports = list_serial_ports()
    if not ports:
        return None
    teensy = [p for p in ports if p["is_teensy"]]
    if len(teensy) == 1:
        return teensy[0]["device"]
    if teensy:
        return teensy[0]["device"]   # varios Teensy: el primero
    likely = [p for p in ports
              if any(k in p["description"].lower()
                     for k in ("serial", "teensy", "usb"))]
    if len(likely) == 1:
        return likely[0]["device"]
    if len(ports) == 1:
        return ports[0]["device"]
    return None   # ambiguo: que el caller liste y elija


# ── Fuentes con hilo + cola ──────────────────────────────────────────────────

class FrameSource:
    """Base: hilo productor → cola de Frames. poll() la drena."""

    def __init__(self, maxsize: int = 1000):
        self.queue: "queue.Queue[Frame]" = queue.Queue(maxsize=maxsize)
        self.errors: "queue.Queue[str]" = queue.Queue(maxsize=200)
        # Líneas de TEXTO de la placa (prints de boot/diag que NO son telemetría).
        # Sirven para mostrar qué está haciendo el firmware mientras arranca.
        self.text: "queue.Queue[str]" = queue.Queue(maxsize=200)
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    def start(self) -> None:
        if self._thread is not None:
            return
        self._stop.clear()
        self._thread = threading.Thread(target=self._safe_run, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        t = self._thread
        if t is not None:
            t.join(timeout=2.0)
        self._thread = None

    def send(self, cmd: str) -> None:
        """Manda un comando al firmware. Las fuentes sin robot lo ignoran."""

    def poll(self, max_items: int = 500) -> List[Frame]:
        out: List[Frame] = []
        for _ in range(max_items):
            try:
                out.append(self.queue.get_nowait())
            except queue.Empty:
                break
        return out

    def _push(self, frame: Frame) -> None:
        try:
            self.queue.put_nowait(frame)
        except queue.Full:
            # Descartar el más viejo para no quedar atrás de la realidad.
            try:
                self.queue.get_nowait()
                self.queue.put_nowait(frame)
            except queue.Empty:
                pass

    def _push_error(self, msg: str) -> None:
        try:
            self.errors.put_nowait(msg)
        except queue.Full:
            pass

    def _push_text(self, line: str) -> None:
        """Mensaje de texto de la placa (no telemetría). Descarta el más viejo
        si la cola está llena."""
        try:
            self.text.put_nowait(line)
        except queue.Full:
            try:
                self.text.get_nowait()
                self.text.put_nowait(line)
            except queue.Empty:
                pass

    def last_text(self) -> Optional[str]:
        """Última línea de texto de la placa (drena la cola). None si no hay."""
        out = None
        while True:
            try:
                out = self.text.get_nowait()
            except queue.Empty:
                break
        return out

    def _safe_run(self) -> None:
        try:
            self._run()
        except Exception as e:  # noqa: BLE001 — reportar, no crashear la GUI
            self._push_error(f"fuente terminó con error: {e!r}")

    def _run(self) -> None:  # override
        raise NotImplementedError


class SimSource(FrameSource):
    def __init__(self, rate_hz: float = 20.0, **sim_kwargs):
        super().__init__()
        self.rate_hz = rate_hz
        self._sim = Simulator(rate_hz=rate_hz, **sim_kwargs)

    def _run(self) -> None:
        period = 1.0 / self.rate_hz
        while not self._stop.is_set():
            line = self._sim.next_line()
            try:
                self._push(parse_line(line))
            except ProtocolError as e:
                self._push_error(str(e))
            time.sleep(period)


class SimTopSource(FrameSource):
    """Simulador de la placa TOP (sin robot) — empuja TopFrame."""

    def __init__(self, rate_hz: float = 20.0, **sim_kwargs):
        super().__init__()
        self.rate_hz = rate_hz
        self._sim = SimulatorTop(rate_hz=rate_hz, **sim_kwargs)

    def _run(self) -> None:
        period = 1.0 / self.rate_hz
        while not self._stop.is_set():
            line = self._sim.next_line()
            try:
                self._push(parse_line_top(line))
            except ProtocolError as e:
                self._push_error(str(e))
            time.sleep(period)


class ReplaySource(FrameSource):
    def __init__(self, path: str, rate_hz: float = 20.0, loop: bool = True,
                 parser: Parser = parse_line):
        super().__init__()
        self.path = path
        self.rate_hz = rate_hz
        self.loop = loop
        self.parser = parser

    def _run(self) -> None:
        period = 1.0 / self.rate_hz
        with open(self.path, "r", encoding="utf-8", errors="replace") as f:
            lines = [ln for ln in f if is_telemetry_line(ln)]
        if not lines:
            self._push_error(f"replay vacío o sin telemetría: {self.path}")
            return
        while not self._stop.is_set():
            for ln in lines:
                if self._stop.is_set():
                    return
                try:
                    self._push(self.parser(ln))
                except ProtocolError as e:
                    self._push_error(str(e))
                time.sleep(period)
            if not self.loop:
                return


class SerialSource(FrameSource):
    """Lee del Teensy real por USB. Requiere pyserial (`pip install pyserial`).

    Robusto al banco: si el puerto se cae (la placa se resetea al conectar la
    batería y el USB re-enumera) **se reconecta solo**, y las líneas que NO son
    telemetría (prints de arranque '[DOWN] ...') se publican como TEXTO para que
    la GUI muestre qué está haciendo el firmware mientras bootea.

    `port` puede ser un COM concreto (ej. "COM5") o "auto" (autodetecta el Teensy
    en cada (re)conexión)."""

    def __init__(self, port: str, baud: int = 115200, parser: Parser = parse_line,
                 reconnect_s: float = 1.5):
        super().__init__()
        self.port_spec = port
        self.baud = baud
        self.parser = parser
        self.reconnect_s = reconnect_s
        self._serial = None

    def _resolve_port(self) -> str:
        if str(self.port_spec).lower() == "auto":
            p = autodetect_port()
            if p is None:
                ports = list_serial_ports()
                hint = (", ".join(x["device"] for x in ports)
                        if ports else "ninguno")
                raise RuntimeError(
                    f"autodetección: no encontré un Teensy claro (puertos: {hint}). "
                    f"Pasá el COM a mano con --port COMx.")
            return p
        return self.port_spec

    def _open(self):
        try:
            import serial  # type: ignore
        except ImportError as e:
            raise RuntimeError(
                "pyserial no está instalado. Instalá con: pip install pyserial"
            ) from e
        port = self._resolve_port()
        ser = serial.Serial(port, self.baud, timeout=0.2)
        self._push_text(f"conectado a {port} — esperando datos de la placa…")
        return ser

    def send(self, cmd: str) -> None:
        ser = self._serial
        if ser is None:
            self._push_error("no se puede mandar comando: puerto cerrado (¿reconectando?)")
            return
        if not cmd.endswith("\n"):
            cmd += "\n"
        try:
            ser.write(cmd.encode("ascii", errors="ignore"))
        except Exception as e:  # noqa: BLE001
            self._push_error(f"error al mandar '{cmd.strip()}': {e!r}")

    def _sleep_reconnect(self) -> None:
        # Espera con chequeo de stop, para no quedar trabado al cerrar.
        end = self.reconnect_s
        step = 0.1
        while end > 0 and not self._stop.is_set():
            time.sleep(step)
            end -= step

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                self._serial = self._open()
            except Exception as e:  # noqa: BLE001 — sin puerto: avisar y reintentar
                self._push_error(f"no pude abrir el puerto: {e}")
                self._sleep_reconnect()
                continue
            try:
                while not self._stop.is_set():
                    raw = self._serial.readline()
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="replace").strip()
                    if not line:
                        continue
                    if is_telemetry_line(line):
                        try:
                            self._push(self.parser(line))
                        except ProtocolError as e:
                            self._push_error(str(e))
                    else:
                        # Print de boot/diag de la placa: mostrarlo, no tragarlo.
                        self._push_text(line)
            except Exception as e:  # noqa: BLE001 — enlace caído: reconectar
                self._push_error(f"enlace perdido ({e}); reintentando…")
            finally:
                try:
                    if self._serial is not None:
                        self._serial.close()
                except Exception:  # noqa: BLE001
                    pass
                self._serial = None
            self._sleep_reconnect()
