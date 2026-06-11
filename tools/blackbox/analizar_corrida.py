#!/usr/bin/env python3
"""Analizador de corridas de la CAJA NEGRA (CSV de la CENTRAL, envs *_bb).

Toma el corrida_*.csv (de leer_caja_negra.py o recortado del monitor) y produce:
  1. REPORTE en consola: duración, timeline de estados, detectores automáticos
     (pelota-teletransportada = falso naranja, flapping de estados, PWM vs cmd
     incoherente, heading congelado, frenos de emergencia).
  2. PNG con 4 paneles (si matplotlib está instalado): timeline de estados,
     pelota XY, comandos vs PWM, heading.

Uso:
    python analizar_corrida.py corrida_20260611_103000.csv
    python analizar_corrida.py corrida.csv --sin-plot

Requiere: Python 3.8+. matplotlib opcional (sin él, solo reporte de texto).
"""
import csv
import sys
from collections import Counter

# Consolas Windows (cp1252) no banca emojis → salida UTF-8 tolerante.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# ---------------- carga ----------------

def cargar(path: str):
    filas = []
    meta = ""
    # utf-8-sig: tolera el BOM que meten PowerShell/Notepad en Windows (sin esto,
    # la primera línea no matchea startswith('#') y el parseo entero se cae).
    with open(path, encoding="utf-8-sig", errors="replace") as f:
        for raw in f:
            raw = raw.strip()
            if not raw or raw.startswith("==="):
                continue
            if raw.startswith("#"):
                meta = raw
                continue
            filas.append(raw)
    if not filas:
        sys.exit("CSV vacío o sin filas de datos.")
    rd = csv.DictReader(filas)
    rows = []
    for r in rd:
        try:
            rows.append({
                "t": int(r["t_ms"]), "state": r["state"].strip(),
                "match": r["match"] == "1", "snap": r["snap"] == "1",
                "ball_vis": r["ball_vis"] == "1", "goal_vis": r["goal_vis"] == "1",
                "hdg_valid": r["hdg_valid"] == "1", "line": r["line"] == "1",
                "imminent": r["imminent"] == "1",
                "hdg": float(r["hdg_deg"]),
                "bx": int(r["ball_x"]), "by": int(r["ball_y"]),
                "vx": int(r["cmd_vx"]), "vy": int(r["cmd_vy"]), "w": int(r["cmd_w_dps"]),
                "pwm": (int(r["pwm1"]), int(r["pwm2"]), int(r["pwm3"])),
                "emerg": r.get("emerg", "0") == "1",
                # v1.2 (práctica 2026-06-12): odometría OTOS. Ausente en CSVs
                # viejos → defaults inertes (otos_fresh=False anula los detectores).
                "otos_fresh": r.get("otos_fresh", "0") == "1",
                "otos_x": int(r.get("otos_x", 0) or 0),
                "otos_y": int(r.get("otos_y", 0) or 0),
                "otos_hdg": float(r.get("otos_hdg", 0.0) or 0.0),
            })
        except (KeyError, ValueError):
            continue  # fila corrupta (corte de USB a mitad de línea)
    if not rows:
        sys.exit("No se pudo parsear ninguna fila — ¿es un CSV de la caja negra?")
    return meta, rows


# ---------------- detectores ----------------

def detectores(rows):
    hallazgos = []
    # 1. Pelota teletransportada (falso naranja): salto > 800 mm entre muestras (20 ms)
    saltos = 0
    for a, b in zip(rows, rows[1:]):
        if a["ball_vis"] and b["ball_vis"]:
            d = ((a["bx"] - b["bx"]) ** 2 + (a["by"] - b["by"]) ** 2) ** 0.5
            if d > 800:
                saltos += 1
    if saltos:
        hallazgos.append(f"FALSOS NARANJAS probables: {saltos} saltos de pelota >800mm entre "
                         f"muestras de 20ms (una pelota real no se teletransporta) -> recalibrar LAB / limpiar entorno.")
    # 2. Flapping de estados: cambios de estado con permanencia < 100 ms
    cortos = 0
    cambios = []
    for a, b in zip(rows, rows[1:]):
        if a["state"] != b["state"]:
            cambios.append((b["t"], b["state"]))
    for (t1, _), (t2, _) in zip(cambios, cambios[1:]):
        if t2 - t1 < 100:
            cortos += 1
    if cortos > 2:
        hallazgos.append(f"FLAPPING de estados: {cortos} permanencias <100ms — umbrales/histeresis a revisar.")
    # 3. PWM incoherente: comando ~0 pero PWM alto sostenido (o viceversa)
    raros = sum(1 for r in rows if abs(r["vx"]) + abs(r["vy"]) + abs(r["w"]) < 10
                and max(abs(p) for p in r["pwm"]) > 60)
    if raros > 5:
        hallazgos.append(f"PWM SIN COMANDO: {raros} muestras con cmd~0 pero PWM>60 (¿kickstart pegado? ¿estado raro?).")
    # 4. Heading congelado con robot comandado a girar
    frozen = 0
    for a, b in zip(rows, rows[1:]):
        if a["hdg_valid"] and abs(a["w"]) > 3000 and abs(a["hdg"] - b["hdg"]) < 0.01:
            frozen += 1
    if frozen > 25:  # >0.5 s acumulado girando sin que el heading se mueva
        hallazgos.append(f"HEADING SOSPECHOSO: {frozen} muestras girando (|w|>30 dps) con hdg congelado — ¿BNO frizado?")
    # 5. Frenos de emergencia
    emergs = sum(1 for r in rows if r["emerg"])
    if emergs:
        hallazgos.append(f"FRENO DE BORDE: {emergs} muestras en EMERGENCY_LINE (~{emergs * 20} ms total).")
    # 6. Snapshot stale
    stale = sum(1 for r in rows if not r["snap"])
    if stale > 10:
        hallazgos.append(f"SNAPSHOT STALE: {stale} muestras sin datos frescos del TOP ({stale * 100 // len(rows)}% de la corrida).")
    # 7. EMPUJE TORCIDO (v1.2, práctica delantero R1 sin gyro): dentro de cada
    #    tramo continuo de ATK_PUSH, si el yaw del OTOS derivó más de 10°, el
    #    empuje salió curvo (rumbo no sostenido / signo del yaw-hold a revisar).
    push_spans = []
    span = None
    for r in rows:
        if r["state"] == "ATK_PUSH" and r["otos_fresh"]:
            if span is None:
                span = []
            span.append(r)
        else:
            if span:
                push_spans.append(span)
            span = None
    if span:
        push_spans.append(span)
    torcidos = 0
    peor = 0.0
    for s in push_spans:
        if len(s) < 2:
            continue
        drift = abs(_wrap180(s[-1]["otos_hdg"] - s[0]["otos_hdg"]))
        peor = max(peor, drift)
        if drift > 10.0:
            torcidos += 1
    if torcidos:
        hallazgos.append(f"EMPUJE TORCIDO: {torcidos}/{len(push_spans)} empujes con deriva de yaw OTOS "
                         f">10 grados (peor: {peor:.1f}) — revisar signo/ganancia del yaw-hold o pisos asimétricos.")
    # 8. OTOS mudo en el delantero: si el rol depende del OTOS (R1 sin gyro) y la
    #    pose no fluye, el eje de ataque cae y el robot no orbita ni empuja.
    if any(r["state"].startswith("ATK_") for r in rows):
        atk_rows = [r for r in rows if r["state"].startswith("ATK_") and r["state"] != "ATK_WAIT_START"]
        if atk_rows:
            otos_ok = sum(1 for r in atk_rows if r["otos_fresh"])
            if 0 < otos_ok < len(atk_rows) // 2:
                hallazgos.append(f"OTOS INTERMITENTE: pose fresca solo {otos_ok * 100 // len(atk_rows)}% del "
                                 f"tiempo de ataque — revisar link DOWN→CENTRAL y los OTOS.")
    return hallazgos, cambios


def _wrap180(a: float) -> float:
    while a > 180.0:
        a -= 360.0
    while a < -180.0:
        a += 360.0
    return a


# ---------------- reporte ----------------

def reporte(meta, rows):
    t0, t1 = rows[0]["t"], rows[-1]["t"]
    dur = (t1 - t0) / 1000.0
    print("=" * 70)
    print("ANÁLISIS DE CORRIDA — caja negra CENTRAL")
    if meta:
        print(meta)
    print(f"Muestras: {len(rows)}  ·  Duración: {dur:.1f} s  ·  t0={t0} ms")
    print()
    print("TIEMPO POR ESTADO:")
    cnt = Counter(r["state"] for r in rows)
    for st, n in cnt.most_common():
        print(f"  {st:24s} {n * 20 / 1000.0:7.1f} s  ({100 * n // len(rows):3d}%)")
    print()
    vis = sum(1 for r in rows if r["ball_vis"])
    print(f"Pelota visible: {100 * vis // len(rows)}% de la corrida · "
          f"Arco visible: {100 * sum(1 for r in rows if r['goal_vis']) // len(rows)}% · "
          f"Heading válido: {100 * sum(1 for r in rows if r['hdg_valid']) // len(rows)}%")
    print()
    hallazgos, cambios = detectores(rows)
    print(f"TRANSICIONES DE ESTADO ({len(cambios)}):")
    for t, st in cambios[:40]:
        print(f"  t={(t - t0) / 1000.0:7.2f}s -> {st}")
    if len(cambios) > 40:
        print(f"  ... y {len(cambios) - 40} más")
    print()
    if hallazgos:
        print("🔎 DETECTORES AUTOMÁTICOS:")
        for h in hallazgos:
            print(f"  ⚠️  {h}")
    else:
        print("🔎 DETECTORES AUTOMÁTICOS: sin anomalías obvias.")
    print("=" * 70)


def plot(path, rows):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("(matplotlib no instalado — solo reporte de texto. pip install matplotlib)")
        return
    t0 = rows[0]["t"]
    ts = [(r["t"] - t0) / 1000.0 for r in rows]
    fig, axs = plt.subplots(4, 1, figsize=(14, 12), sharex=True)
    # Panel 1: estados como escalera
    estados = sorted({r["state"] for r in rows})
    idx = {s: i for i, s in enumerate(estados)}
    axs[0].step(ts, [idx[r["state"]] for r in rows], where="post")
    axs[0].set_yticks(range(len(estados)))
    axs[0].set_yticklabels(estados, fontsize=7)
    axs[0].set_title("Estados FSM")
    for i, r in enumerate(rows):
        if r["emerg"]:
            axs[0].axvline(ts[i], color="red", alpha=0.25)
    # Panel 2: pelota
    axs[1].plot(ts, [r["bx"] if r["ball_vis"] else None for r in rows], label="ball_x", lw=0.8)
    axs[1].plot(ts, [r["by"] if r["ball_vis"] else None for r in rows], label="ball_y", lw=0.8)
    axs[1].legend(fontsize=7); axs[1].set_title("Pelota (mm, frame robot)")
    # Panel 3: comandos vs PWM
    axs[2].plot(ts, [r["vx"] for r in rows], label="cmd_vx", lw=0.8)
    axs[2].plot(ts, [r["vy"] for r in rows], label="cmd_vy", lw=0.8)
    axs[2].plot(ts, [r["pwm"][0] for r in rows], label="pwm1", lw=0.6, alpha=0.7)
    axs[2].plot(ts, [r["pwm"][1] for r in rows], label="pwm2", lw=0.6, alpha=0.7)
    axs[2].plot(ts, [r["pwm"][2] for r in rows], label="pwm3", lw=0.6, alpha=0.7)
    axs[2].legend(fontsize=7, ncol=5); axs[2].set_title("Comando (mm/s) vs PWM aplicado")
    # Panel 4: heading + omega (+ yaw OTOS si la corrida lo trae, v1.2)
    axs[3].plot(ts, [r["hdg"] for r in rows], label="hdg (deg)", lw=0.8)
    axs[3].plot(ts, [r["w"] / 10.0 for r in rows], label="cmd_w (dps/10)", lw=0.6, alpha=0.7)
    if any(r["otos_fresh"] for r in rows):
        axs[3].plot(ts, [r["otos_hdg"] if r["otos_fresh"] else None for r in rows],
                    label="otos_hdg (deg)", lw=0.8, alpha=0.9)
    axs[3].legend(fontsize=7); axs[3].set_title("Heading y omega")
    axs[3].set_xlabel("t (s)")
    out = path.rsplit(".", 1)[0] + "_analisis.png"
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    print(f"📈 Gráfico: {out}")


def main():
    if len(sys.argv) < 2:
        sys.exit("Uso: python analizar_corrida.py corrida_XXXX.csv [--sin-plot]")
    path = sys.argv[1]
    meta, rows = cargar(path)
    reporte(meta, rows)
    if "--sin-plot" not in sys.argv[2:]:
        plot(path, rows)


if __name__ == "__main__":
    main()
