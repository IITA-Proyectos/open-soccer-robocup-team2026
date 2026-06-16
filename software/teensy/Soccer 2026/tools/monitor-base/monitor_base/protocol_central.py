"""protocol_central.py — Parser del contrato de telemetría CENTRAL v1 (JSON Lines).

Espejo en Python del emisor de la placa CENTRAL (el "cerebro": FSM + comando +
PWM real por rueda + salud de los enlaces que RECIBE de TOP y DOWN + yaw OTOS +
eco del WorldSnapshot). Mismo estilo que protocol_top.py / protocol.py: núcleo
PURO (sin GUI, sin serial) → testeable contra un golden del firmware.

Clave de sniff EXCLUSIVA: top-level "central":1 (ni el frame TOP "cam" ni el DOWN
"ring" la tienen). is_telemetry_line ya filtra por '{' + '"v":'; el ruteo a este
parser se decide por la presencia de "central" en el objeto.

Patrón EXACTO de protocol_top.py:
  • parse_obj_central(obj, raw="") -> CentralFrame  (valida "v", rechaza v!=1)
  • parse_line_central(line) -> CentralFrame         (JSON Lines, una línea)
  • reusa ProtocolError de protocol.py (no se inventa otra excepción).

Campos ADITIVOS no bumpean v (igual que el resto del monitor).
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import List

from .protocol import ProtocolError  # reusa la misma excepción del monitor

SCHEMA_VERSION_CENTRAL = 1

# Árbitro (snap.referee) — mismo mapeo que protocol_top.REFEREE.
REFEREE = {0: "STOP", 1: "START", 2: "HALFTIME", 3: "RESET"}

# Flags del WorldSnapshot (snap.flags), espejo de protocol_top.SNAP_FLAGS.
SNAP_FLAGS = [
    (0x01, "IN_OWN_PENALTY"),
    (0x02, "PARTNER_ALIVE"),
    (0x04, "PARTNER_SEES_BALL"),
    (0x08, "MATCH_RUNNING"),
    (0x10, "HEADING_VALID"),
]


def decode_snap_flags(flags: int) -> List[str]:
    return [name for bit, name in SNAP_FLAGS if flags & bit]


@dataclass
class Fsm:
    """Estado de la máquina de estados del cerebro."""
    role: str          # "ATK" | "GK" (delantero / arquero)
    state: str         # nombre del estado FSM (texto libre del firmware)
    match: bool        # ¿partido corriendo? (árbitro en PLAY)


@dataclass
class Cmd:
    """Comando de movimiento APLICADO por la FSM (lo que pidió, antes de pisos)."""
    vx: int            # mm/s (lateral)
    vy: int            # mm/s (arco-a-arco)
    w: int             # cdeg/s (giro, centi-grados por segundo)
    spin: int          # pwm crudo de giro/impulso


@dataclass
class TopLink:
    """Salud del enlace TOP→CENTRAL (lo que la CENTRAL RECIBE del TOP).
    fresh=False → dato viejo (GRISEAR). age = ms desde el último frame bueno."""
    rxB: int           # bytes recibidos
    fr: int            # frames buenos
    crc: int           # frames con CRC malo
    rsy: int           # resyncs (re-enganches al stream)
    badsz: int         # frames de tamaño inválido
    fresh: bool
    age: int           # ms


@dataclass
class DownLink:
    """Salud del enlace DOWN→CENTRAL (línea + OTOS que la CENTRAL RECIBE del DOWN).
    OJO: lost y crc son CONTADORES SEPARADOS — NO sumarlos (lost = frames
    perdidos por SEQ; crc = frames corruptos). line_fresh/valid son distintos:
    fresh = llegó dato reciente; valid = la línea es confiable (compuerta)."""
    rx: int            # frames recibidos
    crc: int           # frames con CRC malo  (≠ lost)
    lost: int          # frames perdidos por hueco de SEQ  (≠ crc)
    rsy: int           # resyncs
    badsch: int        # frames con schema inesperado
    line_fresh: bool   # ¿llegó línea reciente?
    valid: bool        # ¿la línea es VÁLIDA? (data_valid)
    ev: int            # event_flags de la línea (EV_* OR-eados)
    age: int           # ms


@dataclass
class Otos:
    """Yaw de la odometría OTOS visto por la CENTRAL. SOLO heading (la pose x/y no
    viaja en este contrato — ver Snap.my_* = GAP del TOP). fresh=False → grisear."""
    fresh: bool
    hdg: float         # grados


@dataclass
class Snap:
    """Eco del WorldSnapshot que la CENTRAL consume (pelota relativa + arco propio
    en polar + árbitro + flags). NO trae my_x/my_y: la pose del robot NO viaja en
    este contrato (GAP del TOP); cualquier consumidor debe tratarla como 0/ausente."""
    ball_vis: bool
    ball_x: int        # mm, relativo al robot
    ball_y: int        # mm, relativo al robot
    goal_own_ang: float   # grados (arco propio)
    goal_own_dist: int    # mm
    referee: int
    flags: int

    @property
    def referee_name(self) -> str:
        return REFEREE.get(self.referee, f"?{self.referee}")

    @property
    def flag_names(self) -> List[str]:
        return decode_snap_flags(self.flags)


@dataclass
class Loop:
    """Carga del loop principal del cerebro (presupuesto de tiempo de ciclo)."""
    max_us: int        # peor caso del período (µs)
    ema_us: int        # período promedio suavizado (µs)


@dataclass
class CentralFrame:
    """Un frame de telemetría completo de la placa CENTRAL (una línea JSON)."""
    v: int
    seq: int
    t_ms: int
    fsm: Fsm
    cmd: Cmd
    pwm: List[int]     # PWM real por rueda [m0, m1, m2], signed -255..255
    top: TopLink
    down: DownLink
    otos: Otos
    snap: Snap
    loop: Loop
    raw_json: str = field(default="", repr=False)


def parse_obj_central(obj: dict, raw: str = "") -> CentralFrame:
    """Construye un CentralFrame desde un dict ya decodificado de JSON.

    Lanza ProtocolError si falta "central", el schema no se soporta (v!=1), o
    falta/está malformada una clave esperada.
    """
    try:
        if "central" not in obj:
            raise ProtocolError("no es un frame CENTRAL (falta la clave 'central')")
        v = int(obj["v"])
        if v != SCHEMA_VERSION_CENTRAL:
            raise ProtocolError(
                f"schema CENTRAL no soportado: v={v} "
                f"(esta app entiende v={SCHEMA_VERSION_CENTRAL})")

        fs = obj["fsm"]
        fsm = Fsm(
            role=str(fs["role"]),
            state=str(fs["state"]),
            match=bool(fs["match"]),
        )

        cm = obj["cmd"]
        cmd = Cmd(
            vx=int(cm["vx"]), vy=int(cm["vy"]),
            w=int(cm["w"]), spin=int(cm["spin"]),
        )

        pwm = [int(x) for x in obj["pwm"]]

        tp = obj["top"]
        top = TopLink(
            rxB=int(tp["rxB"]), fr=int(tp["fr"]), crc=int(tp["crc"]),
            rsy=int(tp["rsy"]), badsz=int(tp["badsz"]),
            fresh=bool(tp["fresh"]), age=int(tp["age"]),
        )

        dn = obj["down"]
        down = DownLink(
            rx=int(dn["rx"]), crc=int(dn["crc"]), lost=int(dn["lost"]),
            rsy=int(dn["rsy"]), badsch=int(dn["badsch"]),
            line_fresh=bool(dn["line_fresh"]), valid=bool(dn["valid"]),
            ev=int(dn["ev"]), age=int(dn["age"]),
        )

        ot = obj["otos"]
        otos = Otos(fresh=bool(ot["fresh"]), hdg=float(ot["hdg"]))

        sn = obj["snap"]
        snap = Snap(
            ball_vis=bool(sn["ball_vis"]),
            ball_x=int(sn["ball_x"]), ball_y=int(sn["ball_y"]),
            goal_own_ang=float(sn["goal_own_ang"]),
            goal_own_dist=int(sn["goal_own_dist"]),
            referee=int(sn["referee"]), flags=int(sn["flags"]),
        )

        lp = obj["loop"]
        loop = Loop(max_us=int(lp["max_us"]), ema_us=int(lp["ema_us"]))

        return CentralFrame(
            v=v, seq=int(obj["seq"]), t_ms=int(obj["t_ms"]),
            fsm=fsm, cmd=cmd, pwm=pwm, top=top, down=down,
            otos=otos, snap=snap, loop=loop, raw_json=raw,
        )
    except ProtocolError:
        raise
    except (KeyError, TypeError, ValueError) as e:
        raise ProtocolError(f"frame CENTRAL malformado: {e}") from e


def parse_line_central(line: str) -> CentralFrame:
    """Parsea UNA línea JSON a un CentralFrame. Ignora whitespace/CR/LF al borde.

    Lanza ProtocolError si la línea no es JSON válido, no es un objeto, o no
    cumple el contrato CENTRAL.
    """
    s = line.strip()
    if not s:
        raise ProtocolError("línea vacía")
    try:
        obj = json.loads(s)
    except json.JSONDecodeError as e:
        raise ProtocolError(f"JSON inválido: {e}") from e
    if not isinstance(obj, dict):
        raise ProtocolError("se esperaba un objeto JSON por línea")
    return parse_obj_central(obj, raw=s)


def is_central_line(line: str) -> bool:
    """Heurística barata: ¿parece un frame de la CENTRAL? (clave de sniff
    exclusiva "central"). Para rutear el JSON al parser correcto sin parsear
    de más cuando hay frames de varias placas en el mismo Serial."""
    s = line.lstrip()
    return s.startswith("{") and '"central"' in s and '"v":' in s
