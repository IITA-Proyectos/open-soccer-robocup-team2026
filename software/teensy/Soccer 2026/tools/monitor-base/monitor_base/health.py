"""health.py — Veredicto de SALUD por sensor de la placa TOP (modelo PURO).

Toma UN TopFrame (protocol_top) y dice, sensor por sensor, si anda
(OK / REVISAR / FALLA / SIN DATO) y por qué. Es lo que hace al monitor
"operativo": ver de un vistazo qué sensor miente y poder apagarlo.

Sin tkinter ni serial → testeable host-native con frames sintéticos.

Reglas (derivadas del contrato telemetry_top.h, verificado en código):
  • Cámaras: fok/bok (watchdog del enlace) → FALLA si caído. Pelota fantasma =
    front y back ven la pelota en posiciones muy distintas (imposible físico).
  • IMU: lok/rok (FALLA si no responde); heading_valid=0 → rumbo FALLA
    (congelado/no confiable); desacuerdo L↔R alto → REVISAR.
  • ToF / ultrasonido: 65535 (None) = SIN DATO.
  • OTOS (base): *_fresh=False = dato viejo (SIN DATO); slip alto = REVISAR.
  • Línea (base): MUX_DEAD = FALLA; data_valid=0 = REVISAR (geometría no usable);
    NOISY/SUSPECT = REVISAR.
  • Snapshot→CENTRAL: valid=0 = SIN DATO (todavía no produce).
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from typing import List

from .protocol_top import TopFrame


class Status:
    OK = "ok"          # verde
    WARN = "warn"      # amarillo — revisar
    DEAD = "dead"      # rojo — falla
    NODATA = "nodata"  # gris — sin dato / no fresco


STATUS_COLOR = {
    Status.OK: "#1f8b4c",
    Status.WARN: "#c8911a",
    Status.DEAD: "#b32d2d",
    Status.NODATA: "#4a4f55",
}
STATUS_LABEL = {
    Status.OK: "OK",
    Status.WARN: "REVISAR",
    Status.DEAD: "FALLA",
    Status.NODATA: "SIN DATO",
}
# Orden de severidad para resúmenes ("¿lo peor que hay?").
_SEVERITY = {Status.OK: 0, Status.NODATA: 1, Status.WARN: 2, Status.DEAD: 3}


@dataclass
class SensorHealth:
    key: str       # id estable (para tests / ubicar el tile)
    label: str     # texto para el humano
    status: str    # Status.*
    detail: str    # explicación corta del porqué


def evaluate(f: TopFrame, phantom_threshold_mm: float = 200.0,
             disagree_warn_deg: float = 5.0, slip_warn: int = 50) -> List[SensorHealth]:
    """Devuelve la lista de veredictos por sensor, en orden de tablero."""
    cam, camf, camb = f.cam, f.camf, f.camb
    imu, tof, base, line, snap = f.imu, f.tof, f.base, f.line, f.snap
    out: List[SensorHealth] = []

    # ── Cámaras (watchdog del enlace) ──
    out.append(SensorHealth(
        "cam_front", "Cámara frontal",
        Status.OK if cam.front_ok else Status.DEAD,
        ("ve pelota" if camf.ball_visible else "enlace ok, sin pelota")
        if cam.front_ok else "enlace caído (watchdog)"))
    out.append(SensorHealth(
        "cam_back", "Cámara trasera",
        Status.OK if cam.back_ok else Status.DEAD,
        ("ve pelota" if camb.ball_visible else "enlace ok, sin pelota")
        if cam.back_ok else "enlace caído (watchdog)"))

    # ── Pelota fantasma (delta front↔back) ──
    if camf.ball_visible and camb.ball_visible:
        d = math.hypot(camf.ball_x_mm - camb.ball_x_mm,
                       camf.ball_y_mm - camb.ball_y_mm)
        if d > phantom_threshold_mm:
            out.append(SensorHealth(
                "ball_fusion", "Pelota (fusión)", Status.WARN,
                f"PELOTA FANTASMA: front↔back Δ={d:.0f}mm (>{phantom_threshold_mm:.0f}) "
                f"→ apagá una cámara"))
        else:
            out.append(SensorHealth("ball_fusion", "Pelota (fusión)", Status.OK,
                                    f"front≈back Δ={d:.0f}mm"))
    elif not camf.ball_visible and not camb.ball_visible:
        out.append(SensorHealth("ball_fusion", "Pelota (fusión)", Status.NODATA,
                                "ninguna cámara ve la pelota"))
    else:
        who = "frontal" if camf.ball_visible else "trasera"
        out.append(SensorHealth("ball_fusion", "Pelota (fusión)", Status.OK,
                                f"la ve la cámara {who}"))

    # ── IMU: BNO primario (idx0) / secundario (idx1) + rumbo fusionado ──
    # Nombrados por su ROL en la fusión (idx0=primario que prioriza el árbitro,
    # idx1=secundario/respaldo), NO por "izq/der" (los chips no están a los lados).
    # El bus I²C y la dirección (0x28/0x29) son constantes de BUILD y no viajan en
    # la telemetría → no se rotulan acá (mentirían según el firmware flasheado).
    out.append(SensorHealth("bno_left", "BNO primario",
                            Status.OK if imu.left_ok else Status.DEAD,
                            f"{imu.left_deg:+.1f}°" if imu.left_ok else "no responde"))
    out.append(SensorHealth("bno_right", "BNO secundario",
                            Status.OK if imu.right_ok else Status.DEAD,
                            f"{imu.right_deg:+.1f}°" if imu.right_ok else "no responde"))
    if not imu.heading_valid:
        out.append(SensorHealth("heading", "Rumbo (fusión)", Status.DEAD,
                                "no confiable / congelado (heading_valid=0)"))
    elif imu.disagreement_deg > disagree_warn_deg:
        out.append(SensorHealth("heading", "Rumbo (fusión)", Status.WARN,
                                f"desacuerdo prim↔sec Δ={imu.disagreement_deg:.1f}° "
                                f"(>{disagree_warn_deg:.0f}°)"))
    else:
        out.append(SensorHealth("heading", "Rumbo (fusión)", Status.OK,
                                f"{imu.heading_deg:+.1f}° (Δ={imu.disagreement_deg:.1f}°)"))

    # ── ToF por sensor ──
    for i in range(tof.n):
        d = tof.distances_mm[i] if i < len(tof.distances_mm) else None
        if d is None:
            out.append(SensorHealth(f"tof_{i}", f"ToF {i}", Status.NODATA,
                                    "sin lectura (65535)"))
        else:
            out.append(SensorHealth(f"tof_{i}", f"ToF {i}", Status.OK, f"{d} mm"))

    # ── Ultrasonido HC-SR04 ──
    if tof.hcsr04_mm is None:
        out.append(SensorHealth("ultrasonic", "Ultrasonido", Status.NODATA, "sin lectura"))
    else:
        out.append(SensorHealth("ultrasonic", "Ultrasonido", Status.OK, f"{tof.hcsr04_mm} mm"))

    # ── OTOS (pose + velocidad) que llega de la base ──
    if not base.pose_fresh:
        out.append(SensorHealth("otos_pose", "OTOS pose", Status.NODATA,
                                "dato viejo (no fresco)"))
    else:
        out.append(SensorHealth("otos_pose", "OTOS pose", Status.OK,
                                f"x={base.pose_x_mm} y={base.pose_y_mm} "
                                f"hdg={base.pose_heading_deg:+.1f}°"))
    if not base.vel_fresh:
        out.append(SensorHealth("otos_vel", "OTOS vel", Status.NODATA,
                                "dato viejo (no fresco)"))
    elif base.vel_slip > slip_warn:
        out.append(SensorHealth("otos_vel", "OTOS vel", Status.WARN,
                                f"patina (slip={base.vel_slip})"))
    else:
        out.append(SensorHealth("otos_vel", "OTOS vel", Status.OK, f"slip={base.vel_slip}"))

    # ── Línea (de la base) ──
    events = line.event_names
    if not line.fresh:
        out.append(SensorHealth("line", "Línea (base)", Status.NODATA,
                                "no fresca (enlace DOWN→TOP)"))
    elif "MUX_DEAD" in events:
        out.append(SensorHealth("line", "Línea (base)", Status.DEAD,
                                "MUX_DEAD (multiplexor de sensores caído)"))
    elif not line.data_valid:
        out.append(SensorHealth("line", "Línea (base)", Status.WARN,
                                "INVALID (geometría no usable)"))
    elif any(e in events for e in ("SENSOR_NOISY", "CALIB_SUSPECT", "DEGRADED_GEOMETRY")):
        out.append(SensorHealth("line", "Línea (base)", Status.WARN, ", ".join(events)))
    else:
        out.append(SensorHealth("line", "Línea (base)", Status.OK,
                                f"present={line.present} quality={line.quality}"))

    # ── Snapshot → CENTRAL ──
    if snap.valid:
        out.append(SensorHealth("snapshot", "Snapshot→CENTRAL", Status.OK,
                                f"frames={f.frames_sent}"))
    else:
        out.append(SensorHealth("snapshot", "Snapshot→CENTRAL", Status.NODATA,
                                "todavía sin envío"))

    return out


def worst_status(items: List[SensorHealth]) -> str:
    """Lo PEOR que hay en la lista (para un indicador global)."""
    return max((i.status for i in items), key=lambda s: _SEVERITY.get(s, 0),
               default=Status.NODATA)


def counts(items: List[SensorHealth]) -> dict:
    """Cuántos sensores hay en cada estado (para el resumen del header)."""
    c = {Status.OK: 0, Status.WARN: 0, Status.DEAD: 0, Status.NODATA: 0}
    for i in items:
        c[i.status] = c.get(i.status, 0) + 1
    return c
