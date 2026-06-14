# gen_arbol_salud.py — Genera fig_arbol_salud.png (draft) para TDP/POSTER.
#
# Pregunta que responde la figura
# -------------------------------
#   "¿Cómo decide el monitor que un sensor está SANO, DUDOSO o FALLADO,
#    SIN la cancha?"
#
# Es un diagrama de la LÓGICA del tablero de salud por sensor de la placa TOP
# (NO un screenshot). Muestra el semáforo de 4 estados y, para 5 sensores
# representativos, el criterio REAL que dispara cada veredicto.
#
# FUENTES (no se inventan criterios — todos salen del modelo PURO):
#   - software/teensy/Soccer 2026/tools/monitor-base/monitor_base/health.py
#       · evaluate(f, phantom_threshold_mm=200.0, disagree_warn_deg=5.0,
#                  slip_warn=50)
#       · Status.OK/WARN/DEAD/NODATA  +  STATUS_LABEL  +  STATUS_COLOR
#       · Cámara: front_ok/back_ok (watchdog del enlace) → FALLA si caído;
#                 pelota fantasma = |Δ front↔back| > 200 mm → REVISAR
#       · BNO/rumbo: left_ok/right_ok → FALLA si no responde;
#                    heading_valid=0 → FALLA; desacuerdo L↔R > 5° → REVISAR
#       · ToF: distancia None (sentinela 65535 del firmware) → SIN DATO
#       · OTOS: pose_fresh/vel_fresh=False → SIN DATO; vel_slip > 50 → REVISAR
#       · Línea: MUX_DEAD → FALLA; data_valid=0 → REVISAR;
#                NOISY/SUSPECT/DEGRADED → REVISAR; no fresca → SIN DATO
#   - Madurez: 16 tests host de health.py (test_health.py) — verificado;
#              tablero validado en banco 2026-06-14 (placa TOP real):
#              journal/2026-06-14-banco-monitor-top-validado-y-top-central-ok.md
#              journal/2026-06-14-monitor-top-salud-y-zonas-telemetria.md
#
# Los COLORES del semáforo se IMPORTAN de health.STATUS_COLOR → idénticos al
# tablero real que ve el equipo. Si el paquete no está en el path, hay fallback.
#
# Uso:
#   python docs/competencia/assets/gen_arbol_salud.py
# Salida: docs/competencia/assets/drafts/fig_arbol_salud.png  @300 dpi
#
# IDIOMA: español (figura de TRABAJO; la versión final del poster va en inglés).

import os
import sys

import matplotlib
matplotlib.use("Agg")  # sin display, solo guardar a archivo
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))

# ── Reusar los colores REALES del tablero (misma paleta que ve el equipo) ────
sys.path.insert(0, os.path.join(REPO, "software", "teensy", "Soccer 2026",
                                "tools", "monitor-base"))
try:
    from monitor_base.health import STATUS_COLOR, STATUS_LABEL, Status
    C_OK = STATUS_COLOR[Status.OK]
    C_WARN = STATUS_COLOR[Status.WARN]
    C_DEAD = STATUS_COLOR[Status.DEAD]
    C_NODATA = STATUS_COLOR[Status.NODATA]
    L_OK = STATUS_LABEL[Status.OK]        # "OK"
    L_WARN = STATUS_LABEL[Status.WARN]    # "REVISAR"
    L_DEAD = STATUS_LABEL[Status.DEAD]    # "FALLA"
    L_NODATA = STATUS_LABEL[Status.NODATA]  # "SIN DATO"
except Exception:  # pragma: no cover — fallback si no está el paquete
    C_OK, C_WARN, C_DEAD, C_NODATA = "#1f8b4c", "#c8911a", "#b32d2d", "#4a4f55"
    L_OK, L_WARN, L_DEAD, L_NODATA = "OK", "REVISAR", "FALLA", "SIN DATO"

INK = "#15191e"
GRID = "#10141a"

# ── Los 4 estados del semáforo (significado, de health.py) ───────────────────
STATES = [
    (C_OK,     L_OK,     "anda y el dato\nes confiable"),
    (C_WARN,   L_WARN,   "responde pero el\ndato es sospechoso"),
    (C_DEAD,   L_DEAD,   "no responde /\nno confiable"),
    (C_NODATA, L_NODATA, "todavía sin\nlectura"),
]

# ── 5 sensores representativos: qué dispara cada veredicto (criterio REAL) ────
# Cada fila: (sensor, [(estado_color, etiqueta_estado, criterio_que_lo_dispara)])
# Se eligen los disparadores característicos de cada sensor (no todos los casos).
SENSORS = [
    ("Cámara\n(front / back)", "watchdog del enlace\n+ cruce front↔back", [
        (C_OK,   L_OK,   "enlace vivo\n(front_ok /\nback_ok = 1)"),
        (C_WARN, L_WARN, "PELOTA FANTASMA:\n|Δ front↔back|\n> 200 mm"),
        (C_DEAD, L_DEAD, "enlace caído\n(watchdog)"),
    ]),
    ("BNO + rumbo\n(2 IMU)", "L/R responden\n+ acuerdan", [
        (C_OK,   L_OK,   "ambos OK y\n|Δ L↔R| ≤ 5°"),
        (C_WARN, L_WARN, "desacuerdo\nL↔R > 5°"),
        (C_DEAD, L_DEAD, "no responde  /\nheading_valid = 0"),
    ]),
    ("ToF\n(4 láser)", "sentinela del\nfirmware", [
        (C_OK,     L_OK,     "distancia\nen rango"),
        (C_NODATA, L_NODATA, "lectura 65535\n→ None (sin eco)"),
    ]),
    ("OTOS\n(pose + vel)", "frescura del dato\n+ patinaje", [
        (C_OK,     L_OK,     "dato fresco\ny slip ≤ 50"),
        (C_WARN,   L_WARN,   "patina:\nslip > 50"),
        (C_NODATA, L_NODATA, "dato viejo\n(no fresco)"),
    ]),
    ("Línea\n(anillo base)", "eventos del\nseguidor de línea", [
        (C_OK,     L_OK,     "fresca y\ngeometría usable"),
        (C_WARN,   L_WARN,   "INVALID /\nNOISY / SUSPECT"),
        (C_DEAD,   L_DEAD,   "MUX_DEAD\n(multiplexor caído)"),
    ]),
]


def _chip(ax, x, y, w, h, color, label, fs=15, tcol="#ffffff", lw=2.0):
    """Pastilla de estado redondeada con etiqueta centrada."""
    ax.add_patch(FancyBboxPatch(
        (x, y), w, h, boxstyle="round,pad=0.012,rounding_size=0.05",
        facecolor=color, edgecolor=GRID, linewidth=lw, mutation_aspect=1.0))
    ax.text(x + w / 2, y + h / 2, label, ha="center", va="center",
            color=tcol, fontsize=fs, fontweight="bold")


def fig():
    fig, ax = plt.subplots(figsize=(15.5, 11.2))
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis("off")

    # ── Título + bajada (la PREGUNTA) ────────────────────────────────────────
    fig.suptitle("¿Cómo sabe el monitor qué sensor miente, SIN la cancha?",
                 fontsize=26, fontweight="bold", x=0.5, y=0.975)
    ax.text(50, 95.0,
            "Tablero de salud de la placa TOP — un semáforo por sensor + el "
            "porqué (modelo puro, sin tocar el partido)",
            ha="center", va="center", fontsize=16, color="#444")

    # ── UNIDAD 1: la leyenda del semáforo de 4 estados ───────────────────────
    ax.text(3.0, 89.2, "El semáforo (4 estados):", ha="left", va="center",
            fontsize=17, fontweight="bold", color=INK)
    cw, ch, gap = 21.5, 6.6, 2.3
    x0 = 3.0
    for i, (col, lab, meaning) in enumerate(STATES):
        x = x0 + i * (cw + gap)
        _chip(ax, x, 80.6, cw * 0.40, ch, col, lab, fs=16,
              tcol=("#ffffff" if lab != L_WARN else "#1a1206"))
        ax.text(x + cw * 0.40 + 1.2, 80.6 + ch / 2, meaning,
                ha="left", va="center", fontsize=12.8, color="#333",
                linespacing=1.0)

    # ── UNIDADES 2-6: una fila por sensor con sus disparadores ───────────────
    # Geometría de filas.
    top = 75.5
    row_h = 13.2
    name_x, name_w = 3.0, 18.0         # columna del nombre del sensor
    rule_x = name_x + name_w + 1.0     # columna "criterio base"
    rule_w = 16.5
    chips_x0 = rule_x + rule_w + 1.5   # inicio de las pastillas de veredicto
    chips_right = 99.0
    chips_total = chips_right - chips_x0

    # placa = TOP → todo el tablero corre en TOP (azul como acento de tarjeta)
    TOP_BLUE = "#1D4E89"

    for r, (sname, base_rule, triggers) in enumerate(SENSORS):
        y = top - r * row_h
        yb = y - row_h + 1.4  # base de la tarjeta de fila
        # Tarjeta de la fila
        ax.add_patch(FancyBboxPatch(
            (name_x - 1.2, yb), chips_right - name_x + 1.2, row_h - 2.2,
            boxstyle="round,pad=0.2,rounding_size=0.6",
            facecolor=("#f5f7fa" if r % 2 == 0 else "#eef2f6"),
            edgecolor="#c4ccd4", linewidth=1.4))
        cy = yb + (row_h - 2.2) / 2

        # Nombre del sensor (acento azul TOP)
        ax.add_patch(plt.Rectangle((name_x - 1.2, yb), 0.9, row_h - 2.2,
                                   facecolor=TOP_BLUE, edgecolor="none"))
        ax.text(name_x + 0.4, cy, sname, ha="left", va="center",
                fontsize=15.5, fontweight="bold", color=TOP_BLUE,
                linespacing=1.0)
        # Criterio base (qué mira el monitor para ese sensor)
        ax.text(rule_x, cy, base_rule, ha="left", va="center",
                fontsize=11.5, color="#555", style="italic", linespacing=1.05)

        # Pastillas de veredicto con su disparador
        n = len(triggers)
        slot = chips_total / n
        chip_w = 7.6
        for j, (col, lab, crit) in enumerate(triggers):
            cx = chips_x0 + j * slot          # borde izq. del slot
            _chip(ax, cx, cy + 2.4, chip_w, 3.4, col, lab, fs=12.0,
                  tcol=("#ffffff" if lab != L_WARN else "#1a1206"), lw=1.6)
            # criterio: alineado a la IZQ. al borde del slot → no se solapa
            ax.text(cx, cy - 0.2, crit, ha="left", va="top",
                    fontsize=10.0, color="#222", linespacing=1.0)

    # encabezado de las dos sub-columnas
    ax.text(rule_x, top + 1.6, "qué mira el monitor",
            ha="left", va="bottom", fontsize=12.5, fontweight="bold",
            color="#6b7280")
    ax.text(chips_x0, top + 1.6, "veredicto  ←  criterio que lo dispara",
            ha="left", va="bottom", fontsize=12.5, fontweight="bold",
            color="#6b7280")

    # ── UNIDAD 7: recuadro de madurez (host vs banco, fechado) ───────────────
    note = ("MADUREZ — Reglas verificadas EN HOST: 16 tests de health.py con "
            "frames sintéticos (sin robot).\n"
            "Tablero VALIDADO EN BANCO 2026-06-14: corrió en la placa TOP real "
            "y mostró dato real (snapshot 66 Hz MEDIDO, crc=0).\n"
            "Umbrales (200 mm · 5° · slip 50) = valores por defecto de "
            "evaluate() en health.py.")
    ax.text(3.0, 9.4, note, ha="left", va="top", fontsize=11.6, color="#1a1a1a",
            bbox=dict(boxstyle="round,pad=0.7", facecolor="#eef6ef",
                      edgecolor=C_OK, linewidth=2.0))

    # Crédito / caption corta dentro de la figura (banda inferior, despejada)
    ax.text(50.0, 0.6,
            "Fig. — Lógica del tablero de salud por sensor (TOP) · "
            "Diagrama original del equipo · CC BY 4.0 · Fuente: health.py",
            ha="center", va="bottom", fontsize=9.5, color="#888")

    fig.subplots_adjust(left=0.015, right=0.985, top=0.93, bottom=0.035)
    out = os.path.join(HERE, "drafts", "fig_arbol_salud.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.savefig(out, dpi=300)
    plt.close(fig)
    print("escrito:", out)
    return out


if __name__ == "__main__":
    fig()
