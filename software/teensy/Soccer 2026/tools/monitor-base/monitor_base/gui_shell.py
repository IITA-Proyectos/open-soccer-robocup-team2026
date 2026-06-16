"""gui_shell.py — Monitor unificado de la placa TOP (consola industrial).

UNA sola ventana con: barra de estado (conexión, seq, Hz, perdidos, grabar/pausa),
NAVEGACIÓN lateral entre vistas (paneles), área de contenido, y un DOCK de logs.
Una sola fuente (conexión) y un solo loop alimentan a todos los paneles; se cambia
de vista sin reconectar. Cada vista es un Panel (panel.py).

Entradas:
  python -m monitor_base --monitor --sim          # consola completa, simulador
  python -m monitor_base --monitor --port auto     # consola completa, robot real
  python -m monitor_base --monitor --selftest      # smoke headless (CI)
Los flags por-vista (--field, --polar, …) siguen andando: abren la MISMA consola
arrancando en esa vista.
"""
from __future__ import annotations

import time
import tkinter as tk
from collections import deque
from tkinter import ttk
from typing import Deque, List, Optional, Type

from .panel import LogBuffer, Panel, PanelContext
from .protocol import Frame as DownFrame
from .protocol_top import TopFrame
from .robot_registry import identify, load_registry
from .safety import is_destructive_write
from .shell_theme import (ACCENT, BAD, BG, BG2, FG, LINE, MONO, MUTED, OK,
                          PANEL, TITLE, UI_B, WARN, apply_theme)

HEARTBEAT_S = 2.0       # cada cuánto el log registra un latido de datos
PHANTOM_MM = 500.0      # |camf.ball - camb.ball| para sospechar pelota fantasma

BOARD_LONG = {"top": "PLACA SUPERIOR (TOP)", "down": "PLACA BASE (DOWN)", "any": "GENERAL"}
BOARD_SHORT = {"top": "PLACA SUPERIOR", "down": "PLACA BASE"}


def board_of(f) -> str:
    """Qué placa produjo el frame (por su tipo)."""
    return "down" if isinstance(f, DownFrame) else "top"


def _registry() -> List[Type[Panel]]:
    """Todas las vistas del monitor (import perezoso para no cargar Tk en tests
    de lógica pura)."""
    panels: List[Type[Panel]] = []
    for modname, clsname in (
            ("panel_field", "FieldPanel"),
            ("panel_polar", "PolarPanel"),
            ("panel_tof_360", "Tof360Panel"),
            ("panel_health", "HealthPanel"),
            ("panel_cam_fusion", "CamFusionPanel"),
            ("panel_timeline", "TimelinePanel"),
            ("panel_tof_setup", "TofSetupPanel"),
            ("panel_base", "BasePanel"),          # placa BASE (DOWN)
            ("panel_arquero", "ArqueroPanel"),    # placa BASE (DOWN)
            ("panel_logs", "LogsPanel"),
            ("panel_help", "HelpPanel"),
    ):
        try:
            mod = __import__(f"monitor_base.{modname}", fromlist=[clsname])
            panels.append(getattr(mod, clsname))
        except Exception:  # noqa: BLE001 — un panel que falte no tumba el monitor
            pass
    return panels


class MonitorShell:
    def __init__(self, root: tk.Tk, source, panel_classes: Optional[List[Type[Panel]]] = None,
                 recorder=None, config_path: Optional[str] = None,
                 start_key: Optional[str] = None):
        self.root = root
        self.source = source
        self.recorder = recorder
        self.is_sim = getattr(source, "is_sim", False)
        self.logbuf = LogBuffer()
        self.frame_hist: Deque = deque(maxlen=600)          # combinada (para Logs)
        self.ctx = PanelContext(send=self._send, log=self.logbuf.add,
                                is_sim=self.is_sim, config_path=config_path)
        self.paused = False
        # Identidad del robot (R1/R2) por N° de serie USB del Teensy (anti-cruce de
        # placas): la app sabe a QUÉ robot está conectada y nombra al robot antes de
        # escribirle a la EEPROM. La config ya está keyed por serial (config_path).
        self._reg_extra = load_registry()
        self._current_robot = None
        self._last_robot_serial = None
        # estado MULTI-PLACA: métricas, último frame e historia por placa.
        self.current_board: Optional[str] = None
        self.last_by_board = {"top": None, "down": None}
        self.metrics_by = {b: {"seq": -1, "rate": 0.0, "frames": 0, "dropped": 0, "last_t": None}
                           for b in ("top", "down")}
        self._last_beat = 0.0
        self._anom: dict = {}        # estados previos para anomalías (por clave)

        apply_theme(root)
        root.title("IITA Soccer — Monitor del robot")
        root.configure(bg=BG)
        self._build_layout(panel_classes or _registry())
        self.logbuf.add("ok", f"conectado a {self.source.describe()}"
                              + ("  (SIMULADOR)" if self.is_sim else ""))
        self.source.start()
        self._select(start_key or self.panels[0].key)
        self.root.after(60, self._tick)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── Layout ──────────────────────────────────────────────────────────────
    def _build_layout(self, panel_classes: List[Type[Panel]]) -> None:
        r = self.root
        r.columnconfigure(1, weight=1)
        r.rowconfigure(1, weight=1)

        # Barra superior.
        top = tk.Frame(r, bg=BG2)
        top.grid(row=0, column=0, columnspan=2, sticky="we")
        tk.Label(top, text="◆ MONITOR ROBOT", bg=BG2, fg=ACCENT, font=TITLE,
                 padx=12, pady=8).pack(side="left")
        # Chip de IDENTIDAD del robot (R1/R2 por N° de serie) — para no cruzar placas.
        self.robot_chip = tk.Label(top, text="…", bg=PANEL, fg=MUTED,
                                   font=("Segoe UI Semibold", 10), padx=10, pady=3)
        self.robot_chip.pack(side="left", padx=(8, 0))
        self.conn_dot = tk.Label(top, text="●", bg=BG2, fg=MUTED, font=("Segoe UI", 14))
        self.conn_dot.pack(side="left", padx=(8, 2))
        self.conn_lbl = tk.Label(top, text="…", bg=BG2, fg=FG, font=MONO)
        self.conn_lbl.pack(side="left")
        self.metrics = tk.Label(top, text="", bg=BG2, fg=MUTED, font=MONO)
        self.metrics.pack(side="left", padx=16)
        tk.Button(top, text="?  ayuda de esta vista", command=self._show_help,
                  bg=PANEL, fg=ACCENT, relief="flat", padx=8).pack(side="right", padx=4, pady=4)
        self.rec_btn = tk.Button(top, text="⏺ grabar", command=self._toggle_record,
                                 bg=PANEL, fg=FG, relief="flat", padx=8)
        self.rec_btn.pack(side="right", padx=4, pady=4)
        self.pause_btn = tk.Button(top, text="⏸ pausa", command=self._toggle_pause,
                                   bg=PANEL, fg=FG, relief="flat", padx=8)
        self.pause_btn.pack(side="right", padx=4, pady=4)

        # Navegación lateral (con scroll por si hay muchas vistas).
        self.nav = tk.Frame(r, bg=BG2, width=160)
        self.nav.grid(row=1, column=0, sticky="ns")
        self.nav.grid_propagate(False)

        # Contenido (paneles apilados).
        content = tk.Frame(r, bg=BG)
        content.grid(row=1, column=1, sticky="nsew")
        content.rowconfigure(0, weight=1); content.columnconfigure(0, weight=1)

        # 1) Instanciar todos los paneles.
        self.panels: List[Panel] = []
        self._nav_btns = {}
        self.active: Optional[Panel] = None
        for cls in panel_classes:
            try:
                p = cls(content, self.ctx)
            except Exception as e:  # noqa: BLE001
                self.logbuf.add("bad", f"panel '{getattr(cls,'key','?')}' no cargó: {e}")
                continue
            p.container.grid(row=0, column=0, sticky="nsew")
            if hasattr(p, "attach"):     # Logs: buffer + historial
                try:
                    p.attach(self.logbuf, self.frame_hist)
                except Exception:  # noqa: BLE001
                    pass
            self.panels.append(p)

        # 2) Nav agrupada por placa (TOP / BASE / GENERAL), con encabezados.
        last_board = None
        for p in self.panels:
            if p.board != last_board:
                last_board = p.board
                tk.Label(self.nav, text=BOARD_LONG.get(p.board, p.board), bg=BG2, fg=MUTED,
                         font=("Segoe UI", 8, "bold"), anchor="w",
                         padx=12, pady=2).pack(fill="x", pady=(8, 0))
            b = tk.Button(self.nav, text=f" {p.icon}  {p.title}", anchor="w", bg=BG2, fg=FG,
                          relief="flat", font=UI_B, padx=12, pady=6,
                          activebackground="#1d2a36", activeforeground=ACCENT,
                          command=lambda k=p.key: self._select(k))
            b.pack(fill="x")
            self._nav_btns[p.key] = b

        self.status = tk.Label(r, text="iniciando…", bg=BG2, fg=MUTED, font=MONO,
                               anchor="w", padx=8, pady=3)
        self.status.grid(row=2, column=0, columnspan=2, sticky="we")

    # ── Navegación ──────────────────────────────────────────────────────────
    def _select(self, key: str) -> None:
        for p in self.panels:
            self._nav_btns[p.key].configure(
                bg=("#1d2a36" if p.key == key else BG2),
                fg=(ACCENT if p.key == key else FG))
        new = next((p for p in self.panels if p.key == key), None)
        if new is None:
            return
        if self.active is not None and self.active is not new:
            try:
                self.active.on_hide()
            except Exception:  # noqa: BLE001
                pass
        self.active = new
        new.container.tkraise()
        try:
            new.on_show()
            f = (self.last_by_board.get(new.board) if new.board in ("top", "down")
                 else (self.frame_hist[-1] if self.frame_hist else None))
            if f is not None:
                new.render(f)
        except Exception as e:  # noqa: BLE001
            self.logbuf.add("bad", f"render '{key}': {e}")
        extra = "" if new.board == "any" else f"  ·  {BOARD_LONG.get(new.board, '')}"
        self._set_status(f"vista: {new.title}{extra}")

    # ── Comandos ────────────────────────────────────────────────────────────
    def _send(self, cmd: str) -> None:
        if self.is_sim:
            self.logbuf.add("warn", f"SIM: comando NO enviado ({cmd})")
            return
        # Opción 2: gatear las ESCRITURAS a EEPROM con confirmación que nombra el
        # robot conectado → no escribirle config al robot equivocado.
        if is_destructive_write(cmd) and not self._confirm_write(cmd):
            self.logbuf.add("warn", f"escritura a EEPROM CANCELADA por el operador ({cmd})")
            return
        try:
            self.source.send(cmd)
            self.logbuf.add("cmd", f"→ {cmd}")
        except Exception as e:  # noqa: BLE001
            self.logbuf.add("bad", f"fallo enviando '{cmd}': {e}")

    def _confirm_write(self, cmd: str) -> bool:
        """Confirma una escritura a EEPROM nombrando el robot conectado (Opción 2).
        Headless/sin display (tests) → no bloquea: deja pasar (el guard de sim ya
        cubre el smoke; los tests no mandan comandos destructivos)."""
        rid = self._current_robot
        who = rid.name if rid else "robot SIN IDENTIFICAR"
        warn = ("" if (rid and rid.known)
                else "\n\n⚠ El robot NO está identificado por su N° de serie — "
                     "verificá que es el correcto ANTES de escribir.")
        try:
            from tkinter import messagebox
            return bool(messagebox.askyesno(
                "Confirmar escritura a EEPROM del robot",
                f"Vas a ESCRIBIR config persistente en:\n\n    {who}\n\n"
                f"Comando:  {cmd}{warn}\n\n¿Confirmás?",
                icon="warning", parent=self.root))
        except Exception:  # noqa: BLE001 — sin display/headless: no bloquear
            return True

    # ── Loop ────────────────────────────────────────────────────────────────
    def _tick(self) -> None:
        frames = self.source.poll() if not self.paused else []
        for f in frames:
            self._pump(f)
        self._drain_errors()
        self._update_bar(bool(frames))
        self.root.after(60, self._tick)

    def _pump(self, f) -> None:
        """Procesa un frame (TOP o BASE): métricas por placa, historial, anomalías,
        hot-swap y render del panel activo si corresponde a esa placa."""
        b = board_of(f)
        m = self.metrics_by[b]
        m["frames"] += 1
        if m["seq"] >= 0:
            gap = f.seq - m["seq"] - 1
            if gap > 0:
                m["dropped"] += gap
        m["seq"] = f.seq
        if m["last_t"] is not None and f.t_ms > m["last_t"]:
            inst = 1000.0 / (f.t_ms - m["last_t"])
            m["rate"] = inst if m["rate"] == 0 else 0.85 * m["rate"] + 0.15 * inst
        m["last_t"] = f.t_ms
        self.last_by_board[b] = f
        self.frame_hist.append(f)
        if self.recorder is not None:
            self.recorder.write(f)
        if b != self.current_board:
            self._on_board_change(b)
        self._log_anomalies(f, b)
        if self.active is not None and self.active.board in (b, "any"):
            try:
                self.active.render(f)
            except Exception as e:  # noqa: BLE001
                self.logbuf.add("bad", f"render '{self.active.key}': {e}")

    def _on_board_change(self, b: str) -> None:
        """Detectó datos de OTRA placa (hot-swap del USB) o la primera placa."""
        prev = self.current_board
        self.current_board = b
        name = BOARD_LONG.get(b, b)
        self.logbuf.add("ok", f"placa detectada: {name}" if prev is None
                        else f"HOT-SWAP → {name}  (la otra placa queda guardada)")
        # Si la vista activa no es de esta placa (ni 'any'), saltar a su vista por defecto.
        if self.active is None or self.active.board not in (b, "any"):
            first = next((p for p in self.panels if p.board == b), None)
            if first is not None:
                self._select(first.key)

    def _log_anomalies(self, f, b: str) -> None:
        if b == "top":
            hv = f.imu.heading_valid
            if self._anom.get("hv") is not None and hv != self._anom["hv"]:
                self.logbuf.add("ok" if hv else "warn",
                                "heading recuperado" if hv else "heading INVÁLIDO (sin rumbo fiable)")
            self._anom["hv"] = hv
            sv = f.snap.valid
            if self._anom.get("sv") is not None and sv != self._anom["sv"]:
                self.logbuf.add("ok" if sv else "warn",
                                "snapshot VÁLIDO" if sv else "snapshot inválido (TOP no manda pose)")
            self._anom["sv"] = sv
            ref = f.snap.referee_cmd
            if self._anom.get("ref") is not None and ref != self._anom["ref"]:
                self.logbuf.add("info", f"árbitro → {f.snap.referee_name}")
            self._anom["ref"] = ref
            cf, cb = f.camf, f.camb
            if cf.ball_visible and cb.ball_visible:
                d = ((cf.ball_x_mm - cb.ball_x_mm) ** 2 + (cf.ball_y_mm - cb.ball_y_mm) ** 2) ** 0.5
                if d > PHANTOM_MM:
                    self.logbuf.add("warn", f"posible PELOTA FANTASMA (Δfront/back={d:.0f}mm)")
        else:  # DOWN (placa base)
            lv = f.line.valid
            if self._anom.get("lv") is not None and lv != self._anom["lv"]:
                self.logbuf.add("ok" if lv else "warn",
                                "línea válida" if lv else "línea inválida (base)")
            self._anom["lv"] = lv
            if f.lifted and not self._anom.get("lifted"):
                self.logbuf.add("warn", "robot LEVANTADO (base)")
            self._anom["lifted"] = f.lifted
        now = time.time()
        if now - self._last_beat >= HEARTBEAT_S:
            self._last_beat = now
            self.logbuf.add("info", f"datos [{BOARD_SHORT.get(b, b)}] seq={f.seq} "
                                    f"{self.metrics_by[b]['rate']:.0f}Hz")

    def _drain_errors(self) -> None:
        errs = getattr(self.source, "errors", None)
        if errs is None:
            return
        while True:
            try:
                self.logbuf.add("bad", errs.get_nowait())
            except Exception:  # noqa: BLE001
                break

    def _update_robot_chip(self) -> None:
        """Refleja a QUÉ robot (R1/R2) está conectada la app, por N° de serie USB
        del Teensy. Avisa en el log si cambia (hot-swap a OTRO robot)."""
        if self.is_sim:
            self._current_robot = None
            self.robot_chip.configure(text="◇ SIMULADOR", bg=PANEL, fg=MUTED)
            return
        rid = identify(self.source.serial_number(), self._reg_extra)
        self._current_robot = rid
        if rid.serial != self._last_robot_serial:
            self._last_robot_serial = rid.serial
            if rid.serial:
                self.logbuf.add("ok" if rid.known else "warn",
                                f"robot conectado: {rid.name}")
        if rid.known:
            self.robot_chip.configure(text=f"▣ {rid.name}", bg=OK, fg=BG)
        elif rid.serial:
            self.robot_chip.configure(text=f"⚠ {rid.name}", bg=WARN, fg=BG)
        else:
            self.robot_chip.configure(text="⚠ identificando…", bg=PANEL, fg=WARN)

    def _update_bar(self, got: bool) -> None:
        self._update_robot_chip()
        live = got and not self.paused
        self.conn_dot.configure(fg=(OK if live else (WARN if self.paused else MUTED)))
        b = self.current_board
        name = BOARD_SHORT.get(b, "esperando placa")
        self.conn_lbl.configure(text=f"{name} · {self.source.describe()}"
                                + (" · SIM" if self.is_sim else ""))
        m = self.metrics_by.get(b) if b else None
        if m:
            self.metrics.configure(
                text=f"seq {m['seq']}   {m['rate']:.0f} Hz   frames {m['frames']}   "
                     f"perdidos {m['dropped']}" + ("   ⏸ PAUSA" if self.paused else ""))
        else:
            self.metrics.configure(text="esperando datos…")

    # ── Barra: acciones ─────────────────────────────────────────────────────
    def _toggle_pause(self) -> None:
        self.paused = not self.paused
        self.pause_btn.configure(text=("▶ reanudar" if self.paused else "⏸ pausa"))
        self.logbuf.add("info", "pausado" if self.paused else "reanudado")

    def _toggle_record(self) -> None:
        if self.recorder is None:
            from .recorder import Recorder
            fn = time.strftime("monitor-%Y%m%d-%H%M%S.jsonl")
            try:
                self.recorder = Recorder(fn)
                self.rec_btn.configure(text="⏹ detener", fg=BAD)
                self.logbuf.add("ok", f"grabando en {fn}")
            except Exception as e:  # noqa: BLE001
                self.logbuf.add("bad", f"no pude grabar: {e}")
        else:
            try:
                self.recorder.close()
            except Exception:  # noqa: BLE001
                pass
            self.recorder = None
            self.rec_btn.configure(text="⏺ grabar", fg=FG)
            self.logbuf.add("info", "grabación detenida")

    def _show_help(self) -> None:
        """Ventana de ayuda CONTEXTUAL de la vista activa."""
        from .help_text import panel_help
        key = self.active.key if self.active else "ayuda"
        title = self.active.title if self.active else "Ayuda"
        win = tk.Toplevel(self.root)
        win.title(f"Ayuda — {title}")
        win.configure(bg=BG2)
        win.geometry("520x420")
        tk.Label(win, text=f"?  {title}", bg=BG2, fg=ACCENT,
                 font=("Segoe UI Semibold", 13), anchor="w", padx=12, pady=8).pack(fill="x")
        t = tk.Text(win, bg=PANEL, fg=FG, font=MONO, relief="flat", wrap="word",
                    padx=14, pady=12, highlightthickness=0)
        t.pack(fill="both", expand=True, padx=10, pady=(0, 6))
        t.insert("1.0", panel_help(key))
        t.configure(state="disabled")
        tk.Button(win, text="cerrar", command=win.destroy, bg=PANEL, fg=FG,
                  relief="flat", padx=12, pady=4).pack(pady=(0, 10))
        win.transient(self.root)

    def _set_status(self, text: str) -> None:
        self.status.configure(text=text)

    def _on_close(self) -> None:
        try:
            self.source.stop()
            if self.recorder is not None:
                self.recorder.close()
        finally:
            self.root.destroy()


def run_shell(source, panel_classes: Optional[List[Type[Panel]]] = None,
              recorder=None, config_path: Optional[str] = None,
              start_key: Optional[str] = None) -> None:
    root = tk.Tk()
    MonitorShell(root, source, panel_classes, recorder=recorder,
                 config_path=config_path, start_key=start_key)
    root.mainloop()


def smoke(frames: int = 60) -> int:
    """Smoke headless: construye el shell con TODOS los paneles, pasa frames del
    simulador por cada vista (build + render reales) y destruye. 0 si OK."""
    import sys
    from .protocol import parse_line
    from .protocol_top import parse_line_top
    from .simulator import Simulator
    from .simulator_top import SimulatorTop
    from .sources import SimTopSource
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        except Exception:  # noqa: BLE001
            pass
    root = tk.Tk()
    root.withdraw()
    try:
        shell = MonitorShell(root, SimTopSource(rate_hz=50.0), config_path=None)
    except Exception as e:  # noqa: BLE001
        print(f"[smoke-monitor] FALLO construyendo el shell: {e}")
        root.destroy()
        return 1
    top_sim, down_sim = SimulatorTop(rate_hz=50.0), Simulator(rate_hz=50.0)
    names = []
    rc = 0
    for p in shell.panels:
        names.append(f"{p.key}/{p.board}")
        shell._select(p.key)
        # Alimentar con frames de la placa que corresponde (ejercita el routing
        # multi-placa + el hot-swap al alternar TOP/BASE).
        down = (p.board == "down")
        for _ in range(8):
            try:
                line = down_sim.next_line() if down else top_sim.next_line()
                shell._pump(parse_line(line) if down else parse_line_top(line))
            except Exception as e:  # noqa: BLE001
                print(f"[smoke-monitor] FALLO en panel '{p.key}': {e}")
                rc = 1
                break
        root.update_idletasks()
    print(f"[smoke-monitor] paneles construidos+renderizados: {names}")
    print(f"[smoke-monitor] eventos de log generados: {len(shell.logbuf)}")
    try:
        shell.source.stop()
    except Exception:  # noqa: BLE001
        pass
    root.destroy()
    print("[smoke-monitor] " + ("OK" if rc == 0 else "FALLO"))
    return rc
