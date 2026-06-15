# gen_zonas_tof.py — Genera fig_zonas_tof_4x4.png (draft) para TDP/POSTER.
#
# Pregunta que responde la figura
# -------------------------------
#   "¿Qué ve REALMENTE cada ToF? — 16 zonas crudas 4×4 por sensor, no un solo
#    promedio."
#
# El firmware HOY promedia las 16 zonas a UNA distancia por sensor
# (`mean_valid_zones`, sensors_tof.cpp). Desde 2026-06-14 el campo `z` ADITIVO de
# la telemetría (schema sigue 2) expone las 16 zonas crudas para verlas en el
# monitor. Esta figura muestra esa grilla 4×4 de UN ToF.
#
# FUENTES (no se inventan datos):
#   - Resolución 4×4 = 16 y sensor VL53L7CX:
#       tools/monitor-base/monitor_base/zones.py (docstring + ZoneGrid)
#       journal/2026-06-14-monitor-top-salud-y-zonas-telemetria.md
#   - Colormap (cerca=cálido, lejos=frío; near=100 mm, far=2000 mm):
#       zones.py::zone_color  (se IMPORTA y reusa acá → color idéntico al monitor)
#   - Orientación / convención de ejes:
#       docs/CONVENCION-EJES-ROBOT.md (+X=DERECHA, +Y=FRENTE; TOF2=DERECHA,
#       TOF3=IZQUIERDA) y src/shared/tof_zone_orient.h (el marco "canónico" SOLO
#       iguala los 4 sensores entre sí; el mapeo zona→azimut físico es A2.2/roadmap).
#   - `z` aditivo + zonas de SOLO LECTURA (A2.2 = roadmap):
#       journal 2026-06-14 (commit del campo z) y zones.py docstring.
#
# Los VALORES en mm son ILUSTRATIVOS/ESQUEMÁTICOS (rotulado en la figura). NO son
# una captura de banco: a 2026-06-14 las zonas se validaron como stream real en
# placa (TASK-209), pero no se publicó un snapshot numérico de las 16 celdas.
#
# Uso:
#   python docs/competencia/assets/gen_zonas_tof.py
# Salida: docs/competencia/assets/drafts/fig_zonas_tof_4x4.png  @300 dpi
#
# IDIOMA: español (figura de TRABAJO; la versión final del poster va en inglés).

import os
import sys

import matplotlib
matplotlib.use("Agg")  # sin display, solo guardar a archivo
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))

# Reusar el zone_color REAL del monitor (misma rampa de color que ve el equipo).
sys.path.insert(0, os.path.join(REPO, "software", "teensy", "Soccer 2026",
                                "tools", "monitor-base"))
try:
    from monitor_base.zones import zone_color, NO_READING_COLOR
except Exception:  # pragma: no cover — fallback si no está el paquete en el path
    NO_READING_COLOR = "#3a3f44"

    def zone_color(dist_mm, near=100, far=2000):
        d = max(near, min(far, dist_mm))
        t = (d - near) / (far - near)
        r = int(round(220 + (60 - 220) * t))
        g = int(round(60 + (200 - 60) * t))
        b = int(round(40 + (90 - 40) * t))
        return f"#{r:02x}{g:02x}{b:02x}"


# ── Datos ILUSTRATIVOS de las 16 zonas (mm), grilla 4×4 row-major ───────────
# Sensor de ejemplo: TOF2 = DERECHO (mira +X = la derecha del robot).
# Patrón pedagógico: una pared cercana en oblicuo + UNA zona sin lectura
# (sentinela 65535 del firmware → None) para mostrar "qué zona miente".
# fila 0 = arriba del cono del sensor, fila 3 = abajo; col 0..3 = ancho del cono.
ZONES_MM = [
    [ 410,  430,  520,  None],   # None = zona sin lectura (sentinela)
    [ 360,  380,  470,  900],
    [ 300,  330,  640, 1500],
    [ 280,  310, 1100, 1950],
]

NEAR_MM, FAR_MM = 100, 2000   # mismos límites de clamp que zone_color


def fig():
    fig, ax = plt.subplots(figsize=(11.5, 9.6))
    ax.set_xlim(-0.4, 4.9)
    ax.set_ylim(-1.55, 5.25)
    ax.set_aspect("equal")
    ax.axis("off")

    # ── Grilla 4×4 ──────────────────────────────────────────────────────────
    n = 4
    for r in range(n):
        for c in range(n):
            val = ZONES_MM[r][c]
            # y invertido: fila 0 arriba.
            y0 = (n - 1 - r)
            x0 = c
            if val is None:
                face = NO_READING_COLOR
                txt = "sin\nlectura"
                tcol = "#e8e8e8"
                tsize = 15
            else:
                face = zone_color(val)
                txt = str(val)
                # texto oscuro sobre celdas claras (lejos/verde), claro sobre rojas.
                tcol = "#101010" if val > 900 else "#ffffff"
                tsize = 21
            ax.add_patch(plt.Rectangle((x0, y0), 1, 1, facecolor=face,
                                       edgecolor="#10141a", linewidth=2.2))
            ax.text(x0 + 0.5, y0 + 0.5, txt, ha="center", va="center",
                    color=tcol, fontsize=tsize, fontweight="bold",
                    fontfamily="DejaVu Sans")

    # ── Etiquetas de eje (orientación canónica, sin inventar el azimut físico) ─
    # COLUMNAS = ancho del cono del sensor (azimut). TOF2 mira +X (derecha).
    ax.annotate("", xy=(4.0, 4.42), xytext=(0.0, 4.42),
                arrowprops=dict(arrowstyle="-|>", lw=2.6, color="#444"))
    ax.text(2.0, 4.66, "ancho del cono del sensor  (4 columnas)",
            ha="center", va="bottom", fontsize=15.5, color="#333")

    # FILAS = cerca→lejos (radial), de arriba abajo en el dibujo.
    arr = FancyArrowPatch((-0.28, 3.95), (-0.28, 0.05),
                          arrowstyle="-|>", mutation_scale=22,
                          lw=2.6, color="#444")
    ax.add_patch(arr)
    ax.text(-0.52, 2.0, "filas: cerca → lejos del robot  (radial)",
            ha="center", va="center", fontsize=15.5, color="#333",
            rotation=90)

    # ── Rótulo del sensor + lo que mira (de la CONVENCIÓN, no inventado) ──────
    ax.text(4.62, 3.5,
            "ToF 2 = DERECHO\nmira +X\n(derecha del\nrobot)",
            ha="left", va="center", fontsize=14.5, color="#b35900",
            fontweight="bold")

    # ── Leyenda de color (rampa cerca↔lejos) ─────────────────────────────────
    lx0, lx1 = 0.0, 4.0
    ly = -0.62
    lh = 0.32
    steps = 48
    for i in range(steps):
        t = i / (steps - 1)
        d = NEAR_MM + t * (FAR_MM - NEAR_MM)
        xx = lx0 + t * (lx1 - lx0)
        ax.add_patch(plt.Rectangle((xx, ly), (lx1 - lx0) / steps + 0.01, lh,
                                   facecolor=zone_color(d), edgecolor="none"))
    ax.add_patch(plt.Rectangle((lx0, ly), lx1 - lx0, lh, facecolor="none",
                               edgecolor="#10141a", linewidth=1.6))
    ax.text(lx0, ly - 0.12, "cerca  (100 mm)", ha="left", va="top",
            fontsize=14, color="#333")
    ax.text(lx1, ly - 0.12, "lejos  (2000 mm)", ha="right", va="top",
            fontsize=14, color="#333")
    # muestra de "sin lectura"
    ax.add_patch(plt.Rectangle((lx0, ly - 0.92), 0.34, 0.30,
                               facecolor=NO_READING_COLOR, edgecolor="#10141a",
                               linewidth=1.6))
    ax.text(lx0 + 0.46, ly - 0.77, "zona sin lectura (sentinela del firmware)",
            ha="left", va="center", fontsize=14, color="#333")

    # ── Título + bajada ──────────────────────────────────────────────────────
    fig.suptitle("¿Qué ve realmente un ToF? 16 zonas 4×4, no un solo promedio",
                 fontsize=21, fontweight="bold", y=0.975)
    ax.text(2.0, 5.18,
            "VL53L7CX · grilla 4×4 (16 zonas) por sensor · orientación canónica",
            ha="center", va="center", fontsize=15, color="#333")

    # ── Recuadro de honestidad (datos ilustrativos + estado real) ─────────────
    note = ("Valores en mm ILUSTRATIVOS (esquemáticos), no una captura de banco.\n"
            "Hoy el firmware PROMEDIA las 16 zonas a 1 distancia por sensor; el "
            "campo «z» de la telemetría\n"
            "(aditivo, schema 2 — banco 2026-06-14) expone las 16 crudas para "
            "VERLAS. Las zonas son de SOLO\n"
            "LECTURA: el enmascarado/rotación que cambia la navegación (A2.2) es "
            "roadmap. El marco canónico\n"
            "iguala los 4 ToF entre sí; el mapeo exacto zona→dirección física "
            "también es A2.2.")
    ax.text(2.25, -1.85, note, ha="center", va="top", fontsize=11.8,
            color="#222",
            bbox=dict(boxstyle="round,pad=0.6", facecolor="#f3f4f6",
                      edgecolor="#9aa0a6", linewidth=1.3))

    fig.subplots_adjust(left=0.06, right=0.97, top=0.91, bottom=0.15)
    out = os.path.join(HERE, "drafts", "fig_zonas_tof_4x4.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.savefig(out, dpi=300)
    plt.close(fig)
    print("escrito:", out)
    return out


if __name__ == "__main__":
    fig()
