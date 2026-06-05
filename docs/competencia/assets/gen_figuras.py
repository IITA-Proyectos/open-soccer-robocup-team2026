# gen_figuras.py — Genera las 2 figuras de DATOS del poster/TDP (Fig.8 y Fig.9).
#
# Por que existe
# -------------
# El repo no tiene imagenes y dos figuras del poster son GRAFICOS de datos reales
# (no fotos): el crecimiento de la cobertura de tests y el error de odometria OTOS
# por superficie. Este script las produce de forma reproducible -> es, ademas,
# evidencia de "datos mostrados con graficos" (rubrica: Data/Results/Discussion)
# y de replicabilidad open-source.
#
# Uso
# ---
#   pip install matplotlib        # si falta
#   python docs/competencia/assets/gen_figuras.py
# Salida: fig8_test_growth.png y fig9_otos_error.png en esta misma carpeta.
#
# IDIOMA: las etiquetas estan en INGLES porque las figuras van al poster/TDP
# FINALES (que la rubrica exige en ingles). Cambiar a ES si se quiere para la
# version de trabajo.

import os
import matplotlib
matplotlib.use("Agg")  # sin display, solo guardar a archivo
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))

# ============================================================================
# Fig.8 — Crecimiento de la cobertura de tests host-native
# Datos DOCUMENTADOS en el repo (ESTADO-ACTUAL.md + MEMORY + run-host-tests.sh):
#   246 y 262 = 2026-05-29 ; 324/354/403 = 2026-06-03 ; 470 = merge 2026-06-03 ;
#   545 = 2026-06-04 (40 suites) ; 624 = 2026-06-05 ; 652 = 2026-06-05 18:39 ART
#   (47 suites / 0 fallos, vía scripts/run-host-tests.sh — CIFRA VIVA: crece cada
#   sesión, RE-MEDIR antes de imprimir/grabar y actualizar este punto + la fecha).
# ============================================================================
TEST_MILESTONES = [
    ("May 29\n(audit)", 246),
    ("May 29\n(merge)", 262),
    ("Jun 03", 324),
    ("Jun 03", 354),
    ("Jun 03", 403),
    ("Jun 03\n(merge)", 470),
    ("Jun 04", 545),
    ("Jun 05\n19:50", 658),
]

def fig8():
    labels = [m[0] for m in TEST_MILESTONES]
    vals = [m[1] for m in TEST_MILESTONES]
    fig, ax = plt.subplots(figsize=(8, 4.2))
    bars = ax.bar(range(len(vals)), vals, color="#2a7de1", edgecolor="#13315c")
    ax.set_xticks(range(len(vals)))
    ax.set_xticklabels(labels, fontsize=8)
    ax.set_ylabel("Host-native tests passing")
    ax.set_title("Host-native test coverage growth (0 failures throughout)")
    ax.set_ylim(0, max(vals) * 1.15)
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v + 6, str(v),
                ha="center", va="bottom", fontsize=8, fontweight="bold")
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    fig.tight_layout()
    out = os.path.join(HERE, "fig8_test_growth.png")
    fig.savefig(out, dpi=300)
    plt.close(fig)
    print("escrito:", out)

# ============================================================================
# Fig.9 — Error de odometria OTOS por superficie (ciclo testeo -> modificacion)
# ⚠️ VERIFICAR los valores de "lamina" y "directo" contra las notas de banco.
#   Solido: carton corrugado = 6.5% (280.4 mm reportados sobre 300 mm reales,
#   pasa la tolerancia de 8% de TASK-029, journal 2026-05-24). Tolerancia = 8%.
# ============================================================================
OTOS_ERROR = [
    ("A4 over\nprotective film", 9.5),   # VERIFICAR valor exacto en banco
    ("Film removed\n(direct)", 0.0),     # VERIFICAR
    ("Corrugated\ncardboard", 6.5),      # 300 mm -> 280.4 mm, TASK-029 (verificado)
]
OTOS_TOLERANCE_PCT = 8.0

def fig9():
    labels = [s[0] for s in OTOS_ERROR]
    vals = [s[1] for s in OTOS_ERROR]
    colors = ["#d1495b" if v > OTOS_TOLERANCE_PCT else "#2e933c" for v in vals]
    fig, ax = plt.subplots(figsize=(7, 4.2))
    bars = ax.bar(range(len(vals)), vals, color=colors, edgecolor="#222")
    ax.axhline(OTOS_TOLERANCE_PCT, color="#e08a00", linestyle="--", linewidth=1.5,
               label="Tolerance (8%, TASK-029)")
    ax.set_xticks(range(len(vals)))
    ax.set_xticklabels(labels, fontsize=9)
    ax.set_ylabel("Odometry error (%)")
    ax.set_title("OTOS odometry error by surface  (test -> evaluate -> modify)")
    ax.set_ylim(0, max(max(vals), OTOS_TOLERANCE_PCT) * 1.3)
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v + 0.15, f"{v:.1f}%",
                ha="center", va="bottom", fontsize=9, fontweight="bold")
    ax.legend(loc="upper right", fontsize=8)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    fig.tight_layout()
    out = os.path.join(HERE, "fig9_otos_error.png")
    fig.savefig(out, dpi=300)
    plt.close(fig)
    print("escrito:", out)

if __name__ == "__main__":
    fig8()
    fig9()
    print("OK — 2 figuras generadas en", HERE)
