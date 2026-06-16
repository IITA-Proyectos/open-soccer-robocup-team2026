"""tof_layout.py — Configuración de los 4 ToF de la placa TOP, lado APP.

Núcleo PURO (sin tkinter ni serial) → host-testeable. Esta capa NO toca firmware:
modela la configuración que el operador arma DESDE la app —dónde está cada ToF,
cómo está orientado, y qué zonas se vetan según la altura de la pared y el tamaño
de la cancha— la persiste a un .json, y genera los comandos para BAJARLA al
firmware cuando éste tenga los comandos (hoy solo `TOF n POS` existe; `ZONE/ROT/
FLIP` los está agregando otro proceso en paralelo).

Tres cosas que el operador necesita (pedido María 2026-06-15):
  1. ACOMODAR los ToF en su lugar: asignar a cada índice de sensor su posición
     física (FRONT/RIGHT/BACK/LEFT) y corregir su orientación (rotar 0/90/180/270,
     espejar) para que la grilla se interprete bien (el ToF izquierdo viene
     montado 180°; puede haber otros recableos).
  2. PRENDER/APAGAR zonas individuales (las filas superiores suelen "ver por
     encima de la pared" → detecciones falsas de afuera de la cancha).
  3. Hacerlo EN FUNCIÓN de la altura de pared + tamaño de cancha: el modelo
     geométrico sugiere qué filas vetar; el operador acepta o ajusta a mano.

La transformación (rotación/espejo/veto) se aplica acá, en la app, para el
DISPLAY — así las vistas muestran las zonas bien orientadas sin depender del
firmware. `to_firmware_commands()` deja lista la bajada para cuando exista.
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

GRID_W = 4
N_ZONES = GRID_W * GRID_W           # 16 (4×4)
N_TOF = 4
POSITIONS = ("FRONT", "RIGHT", "BACK", "LEFT")
FLIPS = ("none", "h", "v")
ROTATIONS = (0, 90, 180, 270)

# Posición física por DEFECTO de cada índice de sensor (espejo de zones.TOF_LABELS
# / tof_zone_orient.h: 0=FRONTAL 1=TRASERO 2=DERECHO 3=IZQUIERDO).
DEFAULT_POSITION = {0: "FRONT", 1: "BACK", 2: "RIGHT", 3: "LEFT"}
# El ToF izquierdo (idx 3) viene montado mirando abajo → su grilla está 180°.
# Lo dejamos como rotación POR DEFECTO (el operador puede cambiarla); es la misma
# corrección que zones.raw_zone_for_canonical aplica de fábrica.
DEFAULT_ROTATION = {0: 0, 1: 0, 2: 0, 3: 180}


# ── Permutación de zonas por rotación + espejo ──────────────────────────────
def _rc(idx: int, w: int) -> Tuple[int, int]:
    return divmod(idx, w)


def _idx(r: int, c: int, w: int) -> int:
    return r * w + c


def _rotate_rc(r: int, c: int, deg: int, w: int) -> Tuple[int, int]:
    """A dónde va la celda (r,c) tras rotar la grilla `deg` en sentido horario."""
    deg %= 360
    if deg == 0:
        return r, c
    if deg == 90:
        return c, w - 1 - r
    if deg == 180:
        return w - 1 - r, w - 1 - c
    if deg == 270:
        return w - 1 - c, r
    raise ValueError(f"rotación no soportada: {deg} (use 0/90/180/270)")


def _flip_rc(r: int, c: int, flip: str, w: int) -> Tuple[int, int]:
    if flip == "none":
        return r, c
    if flip == "h":            # espejo izquierda↔derecha
        return r, w - 1 - c
    if flip == "v":            # espejo arriba↔abajo
        return w - 1 - r, c
    raise ValueError(f"flip no soportado: {flip} (use none/h/v)")


def zone_source_map(rotation_deg: int, flip: str, w: int = GRID_W) -> List[int]:
    """Devuelve `src` tal que la celda de DISPLAY `i` se lee de la zona CRUDA
    `src[i]`. El display = flip(rotate(crudo)); invertimos esa composición.

    Es la generalización de zones.raw_zone_for_canonical (que sólo hace el 180°
    del ToF izquierdo) a cualquier rotación + espejo elegidos por el operador.
    """
    n = w * w
    src = [0] * n
    for raw in range(n):
        r, c = _rc(raw, w)
        rr, rc = _rotate_rc(r, c, rotation_deg, w)
        fr, fc = _flip_rc(rr, rc, flip, w)
        src[_idx(fr, fc, w)] = raw       # display(fr,fc) ← crudo raw
    return src


# ── Configuración ───────────────────────────────────────────────────────────
@dataclass
class WallField:
    """Parámetros geométricos para sugerir qué filas de zona vetar (ver por
    encima de la pared). Todo en mm/grados. Son ESTIMACIONES editables por el
    operador, NO verdades del firmware (el FOV real y el montaje están sin
    confirmar — TASK del IDE, parte B)."""
    wall_height_mm: float = 140.0       # alto de la pared de la cancha
    mount_height_mm: float = 90.0       # alto del ToF sobre el piso
    tilt_down_deg: float = 0.0          # inclinación del boresight hacia ABAJO (+)
    vfov_deg: float = 60.0              # FOV vertical del ToF (4 filas)
    field_width_mm: int = 1820          # corto (lateral)
    field_height_mm: int = 2430         # largo (arco-a-arco)

    @property
    def ref_dist_mm(self) -> float:
        """Distancia de referencia a la pared = la dimensión más larga (lo más
        lejos que un rayo viaja antes de pegar en una pared)."""
        return float(max(self.field_width_mm, self.field_height_mm))


@dataclass
class TofLayout:
    """Config completa de los 4 ToF. zone_enabled[idx] = lista de 16 bool
    (True = zona activa). position/rotation/flip/enabled por índice de sensor."""
    position: Dict[int, str] = field(default_factory=lambda: dict(DEFAULT_POSITION))
    rotation_deg: Dict[int, int] = field(default_factory=lambda: dict(DEFAULT_ROTATION))
    flip: Dict[int, str] = field(default_factory=lambda: {i: "none" for i in range(N_TOF)})
    sensor_enabled: Dict[int, bool] = field(default_factory=lambda: {i: True for i in range(N_TOF)})
    zone_enabled: Dict[int, List[bool]] = field(
        default_factory=lambda: {i: [True] * N_ZONES for i in range(N_TOF)})
    wall: WallField = field(default_factory=WallField)
    grid_w: int = GRID_W

    # ── Transformación para el DISPLAY ──────────────────────────────────────
    def oriented_cells(self, raw_cells: List[Optional[int]], idx: int) -> List[Optional[int]]:
        """Reordena las zonas CRUDAS del sensor `idx` a su orientación elegida
        (rotación + espejo). Mismo largo; no toca valores."""
        src = zone_source_map(self.rotation_deg.get(idx, 0), self.flip.get(idx, "none"), self.grid_w)
        out: List[Optional[int]] = []
        for i in range(len(src)):
            j = src[i]
            out.append(raw_cells[j] if 0 <= j < len(raw_cells) else None)
        return out

    def zone_is_enabled(self, idx: int, display_zone: int) -> bool:
        """¿La zona en posición de DISPLAY `display_zone` del sensor `idx` está
        activa? (el mask se guarda en marco de DISPLAY, que es lo que el operador
        ve y clickea)."""
        mask = self.zone_enabled.get(idx)
        if mask is None or not (0 <= display_zone < len(mask)):
            return True
        return bool(mask[display_zone])

    def toggle_zone(self, idx: int, display_zone: int) -> bool:
        mask = self.zone_enabled.setdefault(idx, [True] * N_ZONES)
        if 0 <= display_zone < len(mask):
            mask[display_zone] = not mask[display_zone]
            return mask[display_zone]
        return True

    # ── Sugerencia por altura de pared + cancha ─────────────────────────────
    def row_beam_height_mm(self, row: int) -> float:
        """Altura (mm sobre el piso) del centro del rayo de la FILA `row`
        (0 = fila superior) al llegar a la pared de referencia.
        elev(row) sobre la horizontal = -tilt + ( (W-1)/2 - row ) * (vfov/W).
        height = mount + ref_dist * tan(elev)."""
        import math
        w = self.grid_w
        per_row = self.wall.vfov_deg / w
        elev_deg = -self.wall.tilt_down_deg + ((w - 1) / 2.0 - row) * per_row
        return self.wall.mount_height_mm + self.wall.ref_dist_mm * math.tan(math.radians(elev_deg))

    def suggest_vetoed_rows(self) -> List[int]:
        """Filas cuyo rayo, al llegar a la pared de referencia, queda POR ENCIMA
        del borde de la pared → ven afuera de la cancha → sugeridas para vetar."""
        return [r for r in range(self.grid_w)
                if self.row_beam_height_mm(r) > self.wall.wall_height_mm]

    def apply_row_veto(self, rows: List[int]) -> None:
        """Veta (apaga) TODAS las zonas de las filas dadas en los 4 sensores
        (marco de DISPLAY: 'arriba' = filas chicas). No toca las demás."""
        rowset = set(rows)
        for idx in range(N_TOF):
            mask = self.zone_enabled.setdefault(idx, [True] * N_ZONES)
            for z in range(len(mask)):
                r, _ = _rc(z, self.grid_w)
                if r in rowset:
                    mask[z] = False

    def reset_zone_masks(self) -> None:
        self.zone_enabled = {i: [True] * N_ZONES for i in range(N_TOF)}

    # ── Persistencia (lado app) ─────────────────────────────────────────────
    def to_dict(self) -> dict:
        return {
            "position": {str(k): v for k, v in self.position.items()},
            "rotation_deg": {str(k): v for k, v in self.rotation_deg.items()},
            "flip": {str(k): v for k, v in self.flip.items()},
            "sensor_enabled": {str(k): v for k, v in self.sensor_enabled.items()},
            "zone_enabled": {str(k): list(v) for k, v in self.zone_enabled.items()},
            "wall": self.wall.__dict__,
            "grid_w": self.grid_w,
        }

    @classmethod
    def from_dict(cls, d: dict) -> "TofLayout":
        def _ints(m, default):
            out = dict(default)
            for k, v in (m or {}).items():
                out[int(k)] = v
            return out
        wall = WallField(**(d.get("wall") or {}))
        zen_raw = d.get("zone_enabled") or {}
        zen = {i: [True] * N_ZONES for i in range(N_TOF)}
        for k, v in zen_raw.items():
            zen[int(k)] = [bool(x) for x in v]
        return cls(
            position=_ints(d.get("position"), DEFAULT_POSITION),
            rotation_deg=_ints(d.get("rotation_deg"), DEFAULT_ROTATION),
            flip=_ints(d.get("flip"), {i: "none" for i in range(N_TOF)}),
            sensor_enabled=_ints(d.get("sensor_enabled"), {i: True for i in range(N_TOF)}),
            zone_enabled=zen,
            wall=wall,
            grid_w=int(d.get("grid_w", GRID_W)),
        )

    def save(self, path: str) -> None:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)

    @classmethod
    def load(cls, path: str) -> "TofLayout":
        with open(path, "r", encoding="utf-8") as f:
            return cls.from_dict(json.load(f))

    # ── Máscara en marco CRUDO (la que aplica el firmware) ──────────────────
    def raw_zone_mask(self, idx: int) -> int:
        """Máscara de zonas en MARCO CRUDO del sensor (lo que el firmware aplica a
        g_zones_mm vía TopConfig::zone_mask), convertida desde el veto en marco
        DISPLAY que ve y clickea el operador. Bit `raw` = 1 → zona CRUDA `raw` ACTIVA
        (1=usar, 0=anular; misma convención que el firmware). Usa zone_source_map
        (display→crudo) para que vetar la fila 'superior' en PANTALLA anule las zonas
        CRUDAS correctas aun con el sensor rotado/espejado (ej. el ToF izquierdo a 180°).
        Es la fuente-de-verdad de la composición orientación+veto del lado app; el
        firmware solo aplica el bit crudo → test de paridad en tests/."""
        src = zone_source_map(self.rotation_deg.get(idx, 0), self.flip.get(idx, "none"), self.grid_w)
        mask_disp = self.zone_enabled.get(idx, [True] * N_ZONES)
        bits = 0
        for disp in range(len(src)):
            raw = src[disp]
            on = mask_disp[disp] if disp < len(mask_disp) else True
            if on and 0 <= raw < 64:
                bits |= (1 << raw)
        return bits

    # ── Bajada a firmware ───────────────────────────────────────────────────
    def to_firmware_commands(self) -> List["FwCommand"]:
        """Genera los comandos para bajar esta config a la placa. POS, sensor ON/OFF
        y ZONEMASK ya los entiende el firmware HOY (A2.1 + A2.2 enmascarado). ROT/FLIP
        NO van al firmware: la orientación se PLIEGA dentro de la máscara cruda
        (raw_zone_mask) — el firmware solo aplica el bit crudo. Quedan como informativos
        (supported_now=False) para documentar el montaje elegido."""
        cmds: List[FwCommand] = []
        for idx in range(N_TOF):
            cmds.append(FwCommand(f"TOF {idx} POS {self.position.get(idx, 'FRONT')}", supported_now=True))
            cmds.append(FwCommand(f"TOF {idx} {'ON' if self.sensor_enabled.get(idx, True) else 'OFF'}",
                                  supported_now=True))
            rot = self.rotation_deg.get(idx, 0)
            if rot:
                cmds.append(FwCommand(f"TOF {idx} ROT {rot}  # informativo (plegado en ZONEMASK)",
                                      supported_now=False))
            fl = self.flip.get(idx, "none")
            if fl != "none":
                cmds.append(FwCommand(f"TOF {idx} FLIP {fl.upper()}  # informativo (plegado en ZONEMASK)",
                                      supported_now=False))
            # Máscara de zonas en MARCO CRUDO (la orientación display→crudo ya está
            # plegada) → atómica, un comando por sensor. 16 zonas = 16 bits = 4 hex.
            # Default todo-ON => FFFF => no-op en el firmware (fail-safe).
            raw_mask = self.raw_zone_mask(idx)
            cmds.append(FwCommand(f"TOF {idx} ZONEMASK {raw_mask:04X}", supported_now=True))
        cmds.append(FwCommand("CFG SAVE", supported_now=True))
        return cmds


@dataclass
class FwCommand:
    text: str
    supported_now: bool      # True = el firmware HOY lo entiende; False = pendiente


def default_config_path() -> str:
    """Ruta por defecto del .json de config (junto al paquete monitor_base)."""
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(here, "tof_layout.json")


def config_path_for_serial(serial: Optional[str]) -> str:
    """Ruta del .json de config POR PLACA, keyed por el N° de serie del Teensy →
    R1 y R2 NUNCA comparten archivo (imposible cruzar posiciones/zonas de ToF).
    Sin serial (sim/desconocido) cae al archivo legacy único. El N° de serie es el
    identificador hardware-único del Teensy; la app lo resuelve del puerto USB."""
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    if not serial:
        return os.path.join(here, "tof_layout.json")
    slug = "".join(ch if ch.isalnum() else "_" for ch in str(serial))
    return os.path.join(here, f"tof_layout_{slug}.json")


def load_or_default(path: Optional[str] = None) -> TofLayout:
    import os
    p = path or default_config_path()
    if os.path.exists(p):
        try:
            return TofLayout.load(p)
        except Exception:       # noqa: BLE001 — config corrupta → defaults
            pass
    return TofLayout()
