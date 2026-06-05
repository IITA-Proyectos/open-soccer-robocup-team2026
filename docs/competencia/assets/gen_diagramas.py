# -*- coding: utf-8 -*-
"""
gen_diagramas.py  — Renderiza Fig.2 (flujo de datos 3 placas) y Fig.4 (FSM dual)
a PNG limpios @300dpi para el poster/TDP.

Fallback garantizado en matplotlib (no requiere mmdc ni graphviz ni red).
Fuente de verdad de los contratos: docs/competencia/diagramas.md + TDP.md.

Salida:
  fig2_dataflow.png
  fig4_fsm.png
"""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
from matplotlib.lines import Line2D

HERE = os.path.dirname(os.path.abspath(__file__))

# ── Paleta (sobria, alto contraste para impresión) ──
C_TOP    = "#1f4e79"   # azul oscuro
C_CEN    = "#7b241c"   # rojo oscuro
C_DOWN   = "#1e6b1e"   # verde oscuro
C_SENS   = "#5b6770"   # gris pizarra
C_ARB    = "#8a6d00"   # ocre
C_BG_TOP = "#dbe7f3"
C_BG_CEN = "#f3dada"
C_BG_DOWN= "#dcefdc"
EDGE     = "#222222"
LBL_BG   = "#ffffff"

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 9,
})


def box(ax, x, y, w, h, text, facecolor, edgecolor=EDGE,
        textcolor="white", fontsize=9, weight="bold", rounding=0.04):
    """Caja redondeada centrada en (x+w/2, y+h/2)."""
    p = FancyBboxPatch(
        (x, y), w, h,
        boxstyle=f"round,pad=0.0,rounding_size={rounding}",
        linewidth=1.4, edgecolor=edgecolor, facecolor=facecolor,
        mutation_aspect=1.0, zorder=3,
    )
    ax.add_patch(p)
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center",
            color=textcolor, fontsize=fontsize, weight=weight, zorder=4,
            linespacing=1.25)
    return (x, y, w, h)


def container(ax, x, y, w, h, title, facecolor, edgecolor):
    """Sub-grafo / contenedor con título arriba a la izquierda."""
    p = FancyBboxPatch(
        (x, y), w, h,
        boxstyle="round,pad=0.0,rounding_size=0.06",
        linewidth=1.6, edgecolor=edgecolor, facecolor=facecolor,
        linestyle="-", zorder=1,
    )
    ax.add_patch(p)
    ax.text(x + 0.12, y + h - 0.13, title, ha="left", va="top",
            color=edgecolor, fontsize=10, weight="bold", zorder=2)


def arrow(ax, p_from, p_to, label=None, color=EDGE, rad=0.0,
          label_fontsize=7.6, lx=None, ly=None, ls="-", lw=1.6):
    a = FancyArrowPatch(
        p_from, p_to,
        arrowstyle="-|>", mutation_scale=14,
        connectionstyle=f"arc3,rad={rad}",
        linewidth=lw, color=color, zorder=5, linestyle=ls,
        shrinkA=2, shrinkB=2,
    )
    ax.add_patch(a)
    if label:
        if lx is None:
            lx = (p_from[0] + p_to[0]) / 2
        if ly is None:
            ly = (p_from[1] + p_to[1]) / 2
        ax.text(lx, ly, label, ha="center", va="center",
                fontsize=label_fontsize, color="#111111", zorder=6,
                bbox=dict(boxstyle="round,pad=0.28", fc=LBL_BG,
                          ec=color, lw=0.9, alpha=0.96),
                linespacing=1.15)


# ═══════════════════════════════════════════════════════════════════════════
#  FIG 2 — Flujo de datos entre las 3 placas
# ═══════════════════════════════════════════════════════════════════════════
def fig2_dataflow():
    fig, ax = plt.subplots(figsize=(13.5, 8.0))
    ax.set_xlim(0, 13.5)
    ax.set_ylim(0, 8.0)
    ax.axis("off")

    ax.text(6.75, 7.75, "Fig. 2 — Flujo de datos entre placas (TOP · CENTRAL · DOWN)",
            ha="center", va="center", fontsize=14, weight="bold", color="#111111")

    # ── Columna izquierda: sensores TOP + cámaras + árbitro ──
    container(ax, 0.25, 4.45, 3.15, 2.92, "Sensores TOP", "#eef1f3", C_SENS)
    camf = box(ax, 0.45, 6.38, 2.75, 0.58, "Camara frontal — OpenMV N6", C_SENS, fontsize=8.2)
    camr = box(ax, 0.45, 5.66, 2.75, 0.58, "Camara trasera — OpenMV N6", C_SENS, fontsize=8.2)
    bno  = box(ax, 0.45, 4.94, 2.75, 0.58, "2x BNO055 IMU + 4x ToF\n(I2C Wire 100 kHz, BNO @20 Hz)", C_SENS, fontsize=7.4)

    arb  = box(ax, 0.45, 3.45, 2.75, 0.66, "Arbitro RCJ\n(COMM ESP32-C6)", C_ARB, fontsize=8.4)

    # ── Centro: placa TOP ──
    container(ax, 4.05, 4.20, 3.55, 3.17, "TOP — Teensy 4.0 · percepcion", C_BG_TOP, C_TOP)
    parser = box(ax, 4.30, 6.28, 3.05, 0.66, "parser camaras\n(contrato v2, 11 B)", C_TOP, fontsize=8.2)
    fusion = box(ax, 4.30, 5.34, 3.05, 0.70, "fusion + build_snapshot", C_TOP, fontsize=9)
    arbin  = box(ax, 4.30, 4.42, 3.05, 0.70, "arbitro GPIO pines 5/6\nmatch_running = pin5 OR pin6", C_TOP, fontsize=7.6)

    # ── Derecha: placa CENTRAL ──
    container(ax, 9.95, 4.20, 3.35, 3.17, "CENTRAL — Teensy 4.1 · cerebro", C_BG_CEN, C_CEN)
    wm  = box(ax, 10.20, 6.28, 2.85, 0.66, "world_model\n(NO fusiona)", C_CEN, fontsize=8.4)
    fsm = box(ax, 10.20, 5.34, 2.85, 0.70, "strategy.cpp — FSM tactica", C_CEN, fontsize=8.6)
    mot = box(ax, 10.20, 4.42, 2.85, 0.70, "3 motores omni", C_CEN, fontsize=9)

    # ── Abajo centro: placa DOWN ──
    container(ax, 4.05, 0.85, 3.55, 2.62, "DOWN — Teensy 4.0 · piso", C_BG_DOWN, C_DOWN)
    line = box(ax, 4.30, 2.55, 3.05, 0.56, "anillo 32 sensores linea", C_DOWN, fontsize=8.4)
    otos = box(ax, 4.30, 1.84, 3.05, 0.56, "2x OTOS (odometria)", C_DOWN, fontsize=8.4)
    dtx  = box(ax, 4.30, 1.10, 3.05, 0.58, "down_tx (broadcast)", C_DOWN, fontsize=8.6)

    def cx(b): return b[0] + b[2] / 2
    def cy(b): return b[1] + b[3] / 2
    def right(b): return (b[0] + b[2], b[1] + b[3] / 2)
    def left(b):  return (b[0], b[1] + b[3] / 2)
    def top(b):   return (b[0] + b[2] / 2, b[1] + b[3])
    def bot(b):   return (b[0] + b[2] / 2, b[1])

    # Sensores -> parser/fusion
    arrow(ax, right(camf), (parser[0], parser[1] + parser[3] * 0.62),
          label="blobs 11 B · Serial3 · 19200 · ~30 Hz", color=C_SENS,
          rad=0.05, lx=3.72, ly=6.80, label_fontsize=6.8)
    arrow(ax, right(camr), (parser[0], parser[1] + parser[3] * 0.30),
          label="blobs 11 B · Serial5 · 19200", color=C_SENS,
          rad=-0.05, lx=3.72, ly=5.50, label_fontsize=6.8)
    arrow(ax, right(bno), left(fusion), color=C_SENS, rad=-0.04)
    arrow(ax, bot(parser), top(fusion), color=C_TOP, rad=0.0)

    # Árbitro -> arbin -> fusion
    arrow(ax, right(arb), left(arbin),
          label="nivel GPIO pines 5/6\n(no es un frame UART)", color=C_ARB,
          rad=0.0, lx=3.72, ly=4.05, label_fontsize=6.9)
    arrow(ax, top(arbin), (fusion[0] + fusion[2] * 0.5, fusion[1]), color=C_TOP, rad=0.0)

    # TOP -> CENTRAL : WorldSnapshot v3
    arrow(ax, right(fusion), left(wm),
          label="WorldSnapshot v3 · 0x60 · 31 B · 100 Hz · 230400\nTOP Serial4 pin17 → CEN Serial7 pin28",
          color=C_TOP, rad=0.10, lx=8.78, ly=6.55, label_fontsize=7.2)

    # CENTRAL: wm -> fsm -> motores
    arrow(ax, bot(wm), top(fsm), color=C_CEN)
    arrow(ax, bot(fsm), top(mot), label="MotorCommand\nvx/vy/omega",
          color=C_CEN, lx=11.95, ly=5.34, label_fontsize=6.9)

    # DOWN internals
    arrow(ax, bot(line), (dtx[0] + dtx[2] * 0.42, dtx[1] + dtx[3]), color=C_DOWN, rad=0.0)
    arrow(ax, bot(otos), (dtx[0] + dtx[2] * 0.58, dtx[1] + dtx[3]), color=C_DOWN, rad=0.0)

    # DOWN broadcast -> CENTRAL (Serial1)
    arrow(ax, right(dtx), (wm[0] + 0.15, wm[1] - 0.05),
          label="LineStatusV2 0x10 16 B · Pose2D 0x11 7 B · Velocity2D 0x12 7 B\nDOWN Serial1 pin1 → CEN Serial1 pin0 (230400, banco)",
          color=C_DOWN, rad=-0.22, lx=10.1, ly=2.05, label_fontsize=6.9)

    # DOWN broadcast -> TOP (Serial5, cacheado)
    arrow(ax, left(dtx), (fusion[0] + fusion[2] * 0.42, fusion[1]),
          label="mismo broadcast\nDOWN Serial5 pin20 → TOP Serial1 pin0\n(cacheado en TOP)",
          color=C_DOWN, rad=0.28, lx=2.55, ly=2.45, label_fontsize=6.8)

    # Leyenda
    leg = [
        Line2D([0], [0], color=C_SENS, lw=2.4, label="sensores / camaras"),
        Line2D([0], [0], color=C_ARB,  lw=2.4, label="arbitro (GPIO, no UART)"),
        Line2D([0], [0], color=C_TOP,  lw=2.4, label="TOP → CENTRAL (UART)"),
        Line2D([0], [0], color=C_DOWN, lw=2.4, label="DOWN broadcast (UART)"),
        Line2D([0], [0], color=C_CEN,  lw=2.4, label="CENTRAL interno"),
    ]
    ax.legend(handles=leg, loc="lower right", fontsize=7.6, frameon=True,
              framealpha=0.95, edgecolor="#888888", bbox_to_anchor=(13.32 / 13.5, 0.02))

    out = os.path.join(HERE, "fig2_dataflow.png")
    fig.savefig(out, dpi=300, bbox_inches="tight", facecolor="white", pad_inches=0.18)
    plt.close(fig)
    return out


# ═══════════════════════════════════════════════════════════════════════════
#  FIG 4 — FSM dual de strategy.cpp
# ═══════════════════════════════════════════════════════════════════════════
def fsm_state(ax, x, y, text, color, w=2.35, h=0.66, fontsize=8.6):
    b = box(ax, x, y, w, h, text, color, fontsize=fontsize, rounding=0.10)
    return b


def fig4_fsm():
    fig, ax = plt.subplots(figsize=(14.0, 8.6))
    ax.set_xlim(0, 14.0)
    ax.set_ylim(0, 8.6)
    ax.axis("off")

    ax.text(7.0, 8.32, "Fig. 4 — FSM tactica dual (strategy.cpp): DELANTERO / ARQUERO",
            ha="center", va="center", fontsize=14, weight="bold", color="#111111")

    def right(b): return (b[0] + b[2], b[1] + b[3] / 2)
    def left(b):  return (b[0], b[1] + b[3] / 2)
    def top(b):   return (b[0] + b[2] / 2, b[1] + b[3])
    def bot(b):   return (b[0] + b[2] / 2, b[1])

    # ── selección de rol ──
    rol = box(ax, 5.55, 7.30, 2.90, 0.60,
              "seleccion de rol\n(dipswitch al encender)", "#33393f", fontsize=8.4)

    # ─────────────── DELANTERO (izquierda) ───────────────
    container(ax, 0.25, 0.55, 6.55, 6.40, "DELANTERO (ATTACKER)", C_BG_TOP, C_TOP)
    a_wait = fsm_state(ax, 2.25, 5.92, "WAIT_START", C_TOP)
    a_kick = fsm_state(ax, 2.25, 5.05, "KICKOFF", C_TOP)
    a_srch = fsm_state(ax, 2.25, 4.15, "SEARCH", C_TOP)
    a_pos  = fsm_state(ax, 0.65, 2.95, "POSITION", C_TOP)
    a_app  = fsm_state(ax, 3.85, 2.95, "APPROACH\n(empuja por inercia)", C_TOP, fontsize=7.8)
    a_line = fsm_state(ax, 2.25, 1.55, "LINE_AVOID", C_ARB)

    # ─────────────── ARQUERO (derecha) ───────────────
    container(ax, 7.20, 0.55, 6.55, 6.40, "ARQUERO (GOALKEEPER)", C_BG_CEN, C_CEN)
    g_wait = fsm_state(ax, 9.20, 5.92, "WAIT_START", C_CEN)
    g_pat  = fsm_state(ax, 9.20, 5.05, "PATROL", C_CEN)
    g_int  = fsm_state(ax, 9.20, 4.05, "INTERCEPT\n(anticipa con ball_predict)", C_CEN, fontsize=7.6)
    g_clr  = fsm_state(ax, 9.20, 2.95, "CLEAR\n(despeja por inercia)", C_CEN, fontsize=7.8)
    g_line = fsm_state(ax, 9.20, 1.55, "LINE_AVOID", C_ARB)

    # rol -> ATK / GK
    arrow(ax, left(rol), (3.42, 6.85), label="HIGH = DELANTERO", color=C_TOP,
          rad=0.18, lx=3.6, ly=7.15, label_fontsize=7.6)
    arrow(ax, right(rol), (10.37, 6.85), label="LOW = ARQUERO", color=C_CEN,
          rad=-0.18, lx=10.5, ly=7.15, label_fontsize=7.6)

    # ── DELANTERO transiciones ──
    arrow(ax, bot(a_wait), top(a_kick), label="flanco STOP→RUN\n(set play)",
          color=C_TOP, lx=3.95, ly=5.62, label_fontsize=6.8)
    arrow(ax, bot(a_kick), top(a_srch), label="tras ~250 ms boost",
          color=C_TOP, lx=3.85, ly=4.72, label_fontsize=6.8)
    # SEARCH -> POSITION / APPROACH
    arrow(ax, left(a_srch), top(a_pos), label="ve pelota + ve arco\n+ NO alineada",
          color=C_TOP, rad=0.22, lx=1.0, ly=4.0, label_fontsize=6.6)
    arrow(ax, right(a_srch), top(a_app), label="ve pelota +\n(alineada o sin arco)",
          color=C_TOP, rad=-0.22, lx=5.15, ly=4.0, label_fontsize=6.6)
    # POSITION <-> APPROACH
    arrow(ax, (a_pos[0] + a_pos[2], a_pos[1] + a_pos[3] * 0.66),
          (a_app[0], a_app[1] + a_app[3] * 0.66),
          label="detras de la pelota\n+ alineado", color=C_TOP, rad=0.0,
          lx=3.25, ly=3.55, label_fontsize=6.6)
    arrow(ax, (a_app[0], a_app[1] + a_app[3] * 0.30),
          (a_pos[0] + a_pos[2], a_pos[1] + a_pos[3] * 0.30),
          label="desalineo\n(histeresis +10°)", color=C_TOP, rad=0.0,
          lx=3.25, ly=2.62, label_fontsize=6.6)
    # APPROACH/POSITION -> SEARCH (perdio pelota)
    arrow(ax, right(a_app), (a_srch[0] + a_srch[2], a_srch[1] + 0.12),
          label="perdio la pelota", color=C_TOP, rad=-0.30, lx=5.7, ly=3.65,
          label_fontsize=6.6)

    # ── ARQUERO transiciones ──
    arrow(ax, bot(g_wait), top(g_pat), label="match_running",
          color=C_CEN, lx=10.75, ly=5.62, label_fontsize=6.9)
    arrow(ax, bot(g_pat), top(g_int), label="ve la pelota",
          color=C_CEN, lx=10.7, ly=4.78, label_fontsize=6.9)
    arrow(ax, (g_int[0], g_int[1] + g_int[3] * 0.30),
          (g_clr[0], g_clr[1] + g_clr[3] * 0.66),
          label="pelota cerca\n(<250 mm)", color=C_CEN, rad=0.28, lx=8.35, ly=3.5,
          label_fontsize=6.6)
    arrow(ax, (g_clr[0] + g_clr[2], g_clr[1] + g_clr[3] * 0.66),
          (g_int[0] + g_int[2], g_int[1] + g_int[3] * 0.30),
          label="se alejo\n(>400 mm, hist.)", color=C_CEN, rad=0.28, lx=12.4, ly=3.5,
          label_fontsize=6.6)
    arrow(ax, left(g_int), (g_pat[0], g_pat[1] + 0.10),
          label="perdio la pelota", color=C_CEN, rad=0.30, lx=8.0, ly=4.7,
          label_fontsize=6.6)
    arrow(ax, right(g_clr), (g_pat[0] + g_pat[2], g_pat[1] + 0.10),
          label="perdio la pelota", color=C_CEN, rad=-0.32, lx=12.5, ly=4.55,
          label_fontsize=6.6)

    # LINE_AVOID (bypass desde cualquier estado) — flecha discontinua entrante
    arrow(ax, bot(a_srch), top(a_line), color=C_ARB, ls="--", lw=1.3)
    arrow(ax, left(a_line), (a_pos[0] + a_pos[2] * 0.5, a_pos[1]), color=C_ARB,
          ls="--", lw=1.3, rad=0.2)
    ax.text(2.25 + 1.175, 1.30, "entra desde CUALQUIER estado si\nimminent_exit + linea fresca; sale a SEARCH",
            ha="center", va="top", fontsize=6.6, color=C_ARB, style="italic")

    arrow(ax, bot(g_pat), top(g_line), color=C_ARB, ls="--", lw=1.3, rad=-0.45)
    ax.text(9.20 + 1.175, 1.30, "igual que el delantero;\nsale a PATROL",
            ha="center", va="top", fontsize=6.6, color=C_ARB, style="italic")

    # ── Nota global ──
    ax.text(7.0, 0.30,
            "GLOBAL (prioridad, ambos roles):  arbitro STOP (no match_running) → WAIT_START   ·   "
            "imminent_exit + linea fresca → LINE_AVOID   ·   omega clamp ≤ 327 (anti sign-flip)",
            ha="center", va="center", fontsize=7.8, color="#111111", weight="bold",
            bbox=dict(boxstyle="round,pad=0.4", fc="#fff6d6", ec="#8a6d00", lw=1.1))

    out = os.path.join(HERE, "fig4_fsm.png")
    fig.savefig(out, dpi=300, bbox_inches="tight", facecolor="white", pad_inches=0.18)
    plt.close(fig)
    return out


if __name__ == "__main__":
    o2 = fig2_dataflow()
    o4 = fig4_fsm()
    for o in (o2, o4):
        sz = os.path.getsize(o)
        print(f"OK  {o}  ({sz} bytes)")
