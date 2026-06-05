#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""actualizar-cifra.py — Refresca la CIFRA VIVA de tests en los deliverables de competencia.

POR QUE EXISTE (la cifra es VIVA por diseno)
--------------------------------------------
Los deliverables (`docs/competencia/**/*.md`) publican el conteo de tests como un
NUMERO VIVO con timestamp, p.ej.:

    "652 tests / 47 suites / 0 fallos|failures (medido|measured 2026-06-05 18:39 ART
     via|con|via scripts/run-host-tests.sh)"

La suite host-native crece sesion a sesion; por eso la cifra lleva fecha y hora y NO
debe quedar desfasada. Este script automatiza el refresco en un solo comando: corre
el gate, parsea la cifra real del dia, y la propaga a TODOS los .md de competencia
(ES + en/) + a la Fig.8 (gen_figuras.py). NO inventa nada: si el gate no esta verde,
ABORTA sin tocar archivos.

QUE HACE
--------
1. Corre `bash scripts/run-host-tests.sh` y parsea N (tests) y M (envs/suites) de las
   lineas finales "Envs: M | OK: M | FAIL: 0 ..." y "Tests: N | Failures: 0".
   Si FAIL!=0 o Failures!=0 -> ABORTA (no se actualiza nada con el gate roto).
2. Detecta la cifra PUBLICADA (old_N/old_M) por regex r"(\\d+) tests / (\\d+) suites"
   sobre los .md. Si hay mas de un par distinto -> ABORTA (inconsistencia previa a
   resolver a mano).
3. Calcula el timestamp actual: datetime.now() -> "YYYY-MM-DD HH:MM ART".
4. Reemplaza en TODOS los .md de docs/competencia (ES + en/):
     - "{old_N} tests / {old_M} suites" -> "{N} tests / {M} suites"
     - "{old_N}/{old_M}/0"              -> "{N}/{M}/0"
     - "{old_N} / {old_M} / 0"          -> "{N} / {M} / 0"
     - timestamp de medicion (mismo verbo/preposicion) -> nuevo timestamp
     - el final de la cadena de crecimiento "...545 -> {old_N}" -> "-> {N}"
       (soporta flecha ASCII "->" y Unicode "->" / "→")
5. Actualiza gen_figuras.py: cambia el ULTIMO par de TEST_MILESTONES (label con la
   fecha/hora nueva, valor = N) y re-ejecuta gen_figuras.py -> regenera Fig.8.
6. Imprime un CHECKLIST de follow-ups MANUALES (lo que el script NO toca por seguridad).

USO
---
    python docs/competencia/assets/actualizar-cifra.py --dry-run   # default: NO escribe
    python docs/competencia/assets/actualizar-cifra.py --apply     # escribe de verdad

Sin flags = --dry-run (seguro). En --dry-run el script NO escribe NADA: solo corre el
gate (read-only) y reporta cuantos reemplazos haria por archivo.

NOTAS
-----
- Idempotente: re-correrlo no rompe nada (si old==new no hay reemplazos).
- Encoding utf-8, lectura/escritura en BYTES para preservar saltos de linea exactos.
- Solo toca docs/competencia/**/*.md y docs/competencia/assets/gen_figuras.py.
"""

import argparse
import datetime
import os
import re
import subprocess
import sys

# --- Rutas (todas absolutas derivadas de la ubicacion del script) -------------
HERE = os.path.dirname(os.path.abspath(__file__))            # .../docs/competencia/assets
COMPETENCIA_DIR = os.path.dirname(HERE)                      # .../docs/competencia
# Repo root: subir docs/competencia/assets -> docs/competencia -> docs -> <root>
REPO_ROOT = os.path.dirname(os.path.dirname(COMPETENCIA_DIR))
FIRMWARE_DIR = os.path.join(REPO_ROOT, "software", "teensy", "Soccer 2026")
GEN_FIGURAS = os.path.join(HERE, "gen_figuras.py")

PUBLISHED_RE = re.compile(r"(\d+) tests / (\d+) suites")


# ============================================================================
# 1) Gate
# ============================================================================
def run_gate():
    """Corre el gate host y devuelve (N_tests, M_envs). Aborta si no esta verde."""
    script_rel = os.path.join("scripts", "run-host-tests.sh")
    if not os.path.isfile(os.path.join(FIRMWARE_DIR, script_rel)):
        abort("no encuentro el runner: %s (en %s)" % (script_rel, FIRMWARE_DIR))

    print("[gate] corriendo: bash %s" % script_rel)
    print("[gate]      en cwd: %s" % FIRMWARE_DIR)
    try:
        proc = subprocess.run(
            ["bash", script_rel],
            cwd=FIRMWARE_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        )
    except FileNotFoundError:
        abort("no encuentro 'bash' en el PATH; instala git-bash/WSL o agregalo al PATH.")

    out = proc.stdout or ""
    # Mostrar las ultimas lineas para contexto (resumen del runner).
    tail = "\n".join(out.strip().splitlines()[-12:])
    print("[gate] --- ultimas lineas del runner ---")
    print(tail)
    print("[gate] --- fin runner (exit=%d) ---" % proc.returncode)

    # Parsear "Envs: M  |  OK: M  |  FAIL: X  |  build-errors: Y"
    m_envs = re.search(
        r"Envs:\s*(\d+)\s*\|\s*OK:\s*(\d+)\s*\|\s*FAIL:\s*(\d+)", out
    )
    # Parsear "Tests: N  |  Failures: Z"
    m_tests = re.search(r"Tests:\s*(\d+)\s*\|\s*Failures:\s*(\d+)", out)

    if not m_envs or not m_tests:
        abort("no pude parsear el resumen del gate "
              "(faltan lineas 'Envs:...' y/o 'Tests:...'). El gate pudo fallar/crashear.")

    n_envs = int(m_envs.group(1))
    n_fail_envs = int(m_envs.group(3))
    n_tests = int(m_tests.group(1))
    n_failures = int(m_tests.group(2))

    if proc.returncode != 0 or n_fail_envs != 0 or n_failures != 0:
        abort("GATE NO VERDE (exit=%d, FAIL=%d, Failures=%d). "
              "NO se actualiza nada con el gate roto."
              % (proc.returncode, n_fail_envs, n_failures))

    print("[gate] VERDE: %d tests / %d envs / 0 fallos." % (n_tests, n_envs))
    return n_tests, n_envs


# ============================================================================
# 2) Cifra publicada
# ============================================================================
def list_md_files():
    """Todos los .md bajo docs/competencia (recursivo: incluye en/)."""
    out = []
    for root, _dirs, files in os.walk(COMPETENCIA_DIR):
        for fn in files:
            if fn.endswith(".md"):
                out.append(os.path.join(root, fn))
    return sorted(out)


def read_text(path):
    """Lee bytes utf-8 -> str (preserva saltos de linea sin traduccion)."""
    with open(path, "rb") as f:
        return f.read().decode("utf-8")


def write_text(path, text):
    """Escribe str -> bytes utf-8 (preserva saltos de linea exactos)."""
    with open(path, "wb") as f:
        f.write(text.encode("utf-8"))


def detect_published(md_files):
    """Encuentra el par dominante (old_N, old_M). Aborta si hay >1 par distinto."""
    pairs = {}  # (N, M) -> count
    for path in md_files:
        text = read_text(path)
        for m in PUBLISHED_RE.finditer(text):
            key = (int(m.group(1)), int(m.group(2)))
            pairs[key] = pairs.get(key, 0) + 1

    if not pairs:
        abort("no encontre ninguna cifra publicada r'(\\d+) tests / (\\d+) suites' "
              "en los .md de competencia.")

    if len(pairs) > 1:
        detail = ", ".join("%d/%d (x%d)" % (n, mm, c)
                           for (n, mm), c in sorted(pairs.items()))
        abort("INCONSISTENCIA PREVIA: hay >1 cifra publicada distinta -> %s. "
              "Resolvela a mano antes de correr este script." % detail)

    (old_n, old_m), count = next(iter(pairs.items()))
    print("[publicada] cifra unica detectada: %d tests / %d suites (%d apariciones)"
          % (old_n, old_m, count))
    return old_n, old_m


# ============================================================================
# 3) Timestamp
# ============================================================================
def now_stamp():
    """datetime.now() -> 'YYYY-MM-DD HH:MM ART' (zona local del equipo, etiqueta fija ART)."""
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M") + " ART"


# ============================================================================
# 4) Reemplazos en .md
# ============================================================================
def build_md_replacers(old_n, old_m, new_n, new_m, stamp):
    """Devuelve lista de (descripcion, funcion(text)->(text, n_reemplazos))."""
    replacers = []

    # 4a) "{old_N} tests / {old_M} suites" -> "{N} tests / {M} suites"
    pat_pub = re.compile(r"%d tests / %d suites" % (old_n, old_m))
    rep_pub = "%d tests / %d suites" % (new_n, new_m)
    replacers.append(("'N tests / M suites'", _sub_fn(pat_pub, rep_pub)))

    # 4b) "{old_N}/{old_M}/0" -> "{N}/{M}/0"  (sin espacios)
    #     Cuidado: no comer el "{N} tests" ni un "/47/0" embebido en otra cifra.
    #     Anclamos con bordes que no sean digitos a izquierda/derecha del bloque.
    pat_slash = re.compile(r"(?<!\d)%d/%d/0(?!\d)" % (old_n, old_m))
    rep_slash = "%d/%d/0" % (new_n, new_m)
    replacers.append(("'N/M/0' (sin espacios)", _sub_fn(pat_slash, rep_slash)))

    # 4c) "{old_N} / {old_M} / 0" -> "{N} / {M} / 0"  (con espacios)
    pat_slash_sp = re.compile(r"(?<!\d)%d / %d / 0(?!\d)" % (old_n, old_m))
    rep_slash_sp = "%d / %d / 0" % (new_n, new_m)
    replacers.append(("'N / M / 0' (con espacios)", _sub_fn(pat_slash_sp, rep_slash_sp)))

    # 4d) timestamp de medicion: mismo verbo (medido|measured) + prep opcional (el|on)
    #     "medido 2026-06-05 18:39 ART" -> "medido <stamp>"  (conserva verbo/prep)
    pat_ts = re.compile(
        r"(medido|measured)( el| on)? \d{4}-\d{2}-\d{2} \d{2}:\d{2} ART"
    )

    def _ts_repl(m):
        verb = m.group(1)
        prep = m.group(2) or ""
        return "%s%s %s" % (verb, prep, stamp)

    replacers.append(("timestamp de medicion", _sub_fn_callable(pat_ts, _ts_repl)))

    # 4e) final de la cadena de crecimiento: "...545 -> {old_N}" -> "-> {N}"
    #     Soporta flecha ASCII '->' y Unicode '→' (→), con espacios alrededor.
    #     Anclamos en "545 <flecha> old_N" para no tocar otros "-> old_N".
    pat_chain = re.compile(
        r"(545\s*(?:->|→)\s*)%d(?!\d)" % old_n
    )

    def _chain_repl(m):
        return "%s%d" % (m.group(1), new_n)

    replacers.append(("final cadena crecimiento '...545 -> N'",
                      _sub_fn_callable(pat_chain, _chain_repl)))

    return replacers


def _sub_fn(pattern, repl_str):
    """Cierra sobre (pattern, repl_str) y devuelve f(text)->(new_text, count)."""
    def fn(text):
        return pattern.subn(repl_str, text)
    return fn


def _sub_fn_callable(pattern, repl_callable):
    """Igual que _sub_fn pero repl es una funcion (match)->str."""
    def fn(text):
        return pattern.subn(repl_callable, text)
    return fn


def apply_md(md_files, replacers, apply):
    """Aplica (o simula) los reemplazos por archivo. Devuelve total de cambios."""
    grand_total = 0
    for path in md_files:
        original = read_text(path)
        text = original
        per_file = []
        for desc, fn in replacers:
            text, n = fn(text)
            if n:
                per_file.append((desc, n))
        file_changes = sum(n for _d, n in per_file)
        if file_changes:
            grand_total += file_changes
            rel = os.path.relpath(path, REPO_ROOT)
            detail = ", ".join("%s x%d" % (d, n) for d, n in per_file)
            print("  %s%s  (%d): %s"
                  % ("[APPLY] " if apply else "[dry]   ", rel, file_changes, detail))
            if apply and text != original:
                write_text(path, text)
    return grand_total


# ============================================================================
# 5) gen_figuras.py + Fig.8
# ============================================================================
# Captura el ULTIMO par de TEST_MILESTONES: ("...label...", <valor>),
# justo antes del cierre "]" de la lista.
MILESTONE_LAST_RE = re.compile(
    r'(\(\s*")([^"]*)("\s*,\s*)(\d+)(\s*\),?\s*\n\s*\])'
)


def make_fig_label(stamp):
    """Construye el label corto de la ultima barra: 'Mon DD\\nHH:MM' (estilo gen_figuras)."""
    try:
        dt = datetime.datetime.strptime(stamp.replace(" ART", ""), "%Y-%m-%d %H:%M")
        return dt.strftime("%b %d") + "\\n" + dt.strftime("%H:%M")
    except ValueError:
        # Fallback defensivo: usar el stamp crudo (sin saltos).
        return stamp


def update_gen_figuras(new_n, stamp, apply):
    """Cambia el ultimo par de TEST_MILESTONES (label=fecha/hora, valor=N).
    Devuelve True si hubo cambio (o lo habria en dry-run)."""
    if not os.path.isfile(GEN_FIGURAS):
        print("  [warn] no encuentro gen_figuras.py; salteo Fig.8.")
        return False

    original = read_text(GEN_FIGURAS)
    new_label = make_fig_label(stamp)

    def _repl(m):
        return "%s%s%s%d%s" % (m.group(1), new_label, m.group(3), new_n, m.group(5))

    text, n = MILESTONE_LAST_RE.subn(_repl, original, count=1)
    if n == 0:
        print("  [warn] no pude ubicar el ultimo par de TEST_MILESTONES; "
              "salteo edicion de gen_figuras.py.")
        return False

    rel = os.path.relpath(GEN_FIGURAS, REPO_ROOT)
    if text == original:
        print("  [dry]   %s: ultimo milestone ya = ('%s', %d) (sin cambio)"
              % (rel, new_label.replace("\\n", " "), new_n))
        return False

    print("  %s%s: ultimo TEST_MILESTONES -> ('%s', %d)"
          % ("[APPLY] " if apply else "[dry]   ", rel,
             new_label.replace("\\n", " "), new_n))
    if apply:
        write_text(GEN_FIGURAS, text)
    return True


def regen_fig8(apply):
    """Re-ejecuta gen_figuras.py para regenerar Fig.8 (solo con --apply)."""
    if not apply:
        print("  [dry]   (omito ejecutar gen_figuras.py; correria: python gen_figuras.py)")
        return
    print("  [APPLY] ejecutando: python gen_figuras.py")
    try:
        proc = subprocess.run(
            [sys.executable, GEN_FIGURAS],
            cwd=HERE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        )
    except Exception as exc:  # noqa: BLE001
        print("  [warn] no pude ejecutar gen_figuras.py: %s" % exc)
        return
    print((proc.stdout or "").rstrip())
    if proc.returncode != 0:
        print("  [warn] gen_figuras.py salio con codigo %d (revisar matplotlib)."
              % proc.returncode)


# ============================================================================
# 6) Checklist manual
# ============================================================================
def print_checklist(new_n, new_m):
    print("")
    print("=" * 74)
    print("CHECKLIST MANUAL (el script NO toca esto por seguridad):")
    print("=" * 74)
    print("(a) Narracion DELETREADA del video — actualizar a mano al nuevo N/M en")
    print("    PALABRAS (numeros escritos en letras):")
    print("      - docs/competencia/VIDEO-GUION.md:")
    print('          "Cuarenta y siete suites... Seiscientos cincuenta y dos casos"')
    print("          -> reescribir %d suites / %d casos EN PALABRAS." % (new_m, new_n))
    print("      - docs/competencia/en/VIDEO-GUION.md:")
    print('          "Forty-seven test suites. Six hundred and fifty-two cases"')
    print("          -> reescribir %d suites / %d cases IN WORDS." % (new_m, new_n))
    print("(b) Revisar el OVERLAY / numero sobreimpreso del video (texto on-screen")
    print("    y la captura `Tests: %d | Failures: 0 (Envs: %d | OK: %d)`)."
          % (new_n, new_m, new_m))
    print("=" * 74)


# ============================================================================
# Helpers / main
# ============================================================================
def abort(msg):
    print("ABORT: %s" % msg, file=sys.stderr)
    sys.exit(2)


def main():
    parser = argparse.ArgumentParser(
        description="Refresca la cifra VIVA de tests en los deliverables de competencia.")
    g = parser.add_mutually_exclusive_group()
    g.add_argument("--dry-run", action="store_true",
                   help="(default) simula: corre el gate (read-only) y reporta sin escribir.")
    g.add_argument("--apply", action="store_true",
                   help="escribe los cambios en los .md, gen_figuras.py y regenera Fig.8.")
    args = parser.parse_args()

    apply = bool(args.apply)  # default => dry-run
    mode = "APPLY (se ESCRIBEN cambios)" if apply else "DRY-RUN (NO se escribe nada)"
    print("=" * 74)
    print("actualizar-cifra.py — modo: %s" % mode)
    print("  repo root : %s" % REPO_ROOT)
    print("  competencia: %s" % COMPETENCIA_DIR)
    print("=" * 74)

    # 1) Gate (read-only; OK correrlo tambien en dry-run).
    new_n, new_m = run_gate()

    # 2) Cifra publicada.
    md_files = list_md_files()
    old_n, old_m = detect_published(md_files)

    # 3) Timestamp.
    stamp = now_stamp()
    print("[timestamp] nuevo: %s" % stamp)

    if (old_n, old_m) == (new_n, new_m):
        print("[info] la cifra publicada (%d/%d) YA coincide con el gate; "
              "solo se refrescaria el timestamp." % (old_n, old_m))

    # 4) Reemplazos en .md.
    print("")
    print("--- Reemplazos en docs/competencia/**/*.md ---")
    replacers = build_md_replacers(old_n, old_m, new_n, new_m, stamp)
    total = apply_md(md_files, replacers, apply)
    print("  total reemplazos en .md: %d" % total)

    # 5) gen_figuras.py + Fig.8.
    print("")
    print("--- gen_figuras.py / Fig.8 ---")
    changed_fig = update_gen_figuras(new_n, stamp, apply)
    if changed_fig or apply:
        regen_fig8(apply)

    # 6) Checklist manual.
    print_checklist(new_n, new_m)

    print("")
    if apply:
        print("LISTO (--apply). Revisa el diff con git y completa el checklist manual.")
    else:
        print("DRY-RUN: no se escribio nada. Re-corre con --apply para aplicar.")
    print("Recordatorio: la cifra es VIVA por diseno — re-medila el dia de grabar/imprimir.")


if __name__ == "__main__":
    main()
