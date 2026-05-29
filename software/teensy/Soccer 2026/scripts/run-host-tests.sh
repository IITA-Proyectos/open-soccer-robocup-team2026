#!/usr/bin/env bash
# run-host-tests.sh — Corre la suite de tests host-native SIN PlatformIO ni red.
#
# Por que existe
# --------------
# `pio test -e test_native` siempre intenta descargar throwtheswitch/Unity del
# registry (platformio.ini lo documenta en el env test_native). En las maquinas
# del equipo, Avast + la red del taller bloquean esa descarga (TASK-025), asi
# que `pio test` da [ERRORED] en los 19 envs sin compilar una sola linea de
# test. Eso dejaba la suite efectivamente "no corrible" y el ESTADO-ACTUAL.md
# decia que los tests nunca se habian corrido punta a punta.
#
# Este script saltea PlatformIO: compila cada test directo con g++ contra
# Unity vendoreado (lib/Unity/src/unity.c) y los modulos de src/shared. Es
# 100% offline y no toca el registry. Replica exactamente lo que hace el env
# test_native: build_src_filter = +<shared/> => cada test linkea SOLO contra
# src/shared (verificado: ningun .cpp de shared incluye Arduino.h).
#
# Uso
# ---
#   bash scripts/run-host-tests.sh            # corre todos los tests
#   bash scripts/run-host-tests.sh test_proto # corre uno solo (prefijo opcional)
#
# Requisitos: g++ (MinGW-W64 sirve), bash. Nada mas. Sin red.
#
# Salida: tabla por test + resumen. Exit 0 si todo verde, 1 si algun fallo.

set -u

# --- Ubicarse en la raiz del firmware (este script vive en <fw>/scripts/) ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$FW_DIR"

UNITY="lib/Unity/src/unity.c"
if [ ! -f "$UNITY" ]; then
    echo "ERROR: no encuentro Unity vendoreado en $UNITY" >&2
    echo "       (deberia estar commiteado como respaldo offline)" >&2
    exit 2
fi

# Flags espejo del env test_native de platformio.ini (+ include de Unity).
CXXFLAGS="-std=gnu++17 -Wall -Wextra -I src/shared -I lib/Unity/src"

# Todos los modulos host-safe que cualquier test puede necesitar linkear.
SHARED_SRCS=$(ls src/shared/*.cpp 2>/dev/null)

# Filtro opcional: si paso un argumento, solo corro tests cuyo dir lo contenga.
FILTER="${1:-}"

OUT_DIR="$(mktemp -d 2>/dev/null || echo "${TEMP:-/tmp}/top_host_tests")"
mkdir -p "$OUT_DIR"

total_envs=0
ok_envs=0
fail_envs=0
build_errs=0
total_tests=0
total_fails=0
declare -a FAILED_LIST=()

printf '%-28s %8s %8s %8s   %s\n' "TEST ENV" "TESTS" "FAILS" "IGN" "RESULT"
printf '%s\n' "--------------------------------------------------------------------------"

for dir in test/test_*; do
    [ -d "$dir" ] || continue
    name="$(basename "$dir")"
    main="$dir/test_main.cpp"
    [ -f "$main" ] || continue
    if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then
        continue
    fi

    total_envs=$((total_envs + 1))
    exe="$OUT_DIR/$name.exe"
    log="$OUT_DIR/$name.log"

    # Compilar: test_main + todos los shared + unity. -I al dir del test por si
    # trae headers locales (algunos tests definen helpers en su propio dir).
    if ! g++ $CXXFLAGS -I "$dir" "$main" $SHARED_SRCS "$UNITY" -o "$exe" 2> "$OUT_DIR/$name.build.log"; then
        build_errs=$((build_errs + 1))
        fail_envs=$((fail_envs + 1))
        FAILED_LIST+=("$name (BUILD)")
        printf '%-28s %8s %8s %8s   %s\n' "$name" "-" "-" "-" "BUILD-ERROR"
        continue
    fi

    # Correr. Unity imprime al final: "N Tests M Failures K Ignored".
    "$exe" > "$log" 2>&1
    run_rc=$?
    summary="$(grep -E '^[0-9]+ Tests [0-9]+ Failures [0-9]+ Ignored' "$log" | tail -1)"

    if [ -z "$summary" ]; then
        # No imprimio el resumen Unity => crash o salida anormal.
        fail_envs=$((fail_envs + 1))
        FAILED_LIST+=("$name (NO-SUMMARY rc=$run_rc)")
        printf '%-28s %8s %8s %8s   %s\n' "$name" "?" "?" "?" "CRASH/NO-SUMMARY"
        continue
    fi

    n_tests="$(echo "$summary" | awk '{print $1}')"
    n_fails="$(echo "$summary" | awk '{print $3}')"
    n_ign="$(echo "$summary"   | awk '{print $5}')"
    total_tests=$((total_tests + n_tests))
    total_fails=$((total_fails + n_fails))

    if [ "$n_fails" -eq 0 ] && [ "$run_rc" -eq 0 ]; then
        ok_envs=$((ok_envs + 1))
        printf '%-28s %8s %8s %8s   %s\n' "$name" "$n_tests" "$n_fails" "$n_ign" "OK"
    else
        fail_envs=$((fail_envs + 1))
        FAILED_LIST+=("$name ($n_fails fails)")
        printf '%-28s %8s %8s %8s   %s\n' "$name" "$n_tests" "$n_fails" "$n_ign" "FAIL"
    fi
done

printf '%s\n' "--------------------------------------------------------------------------"
printf 'Envs: %d  |  OK: %d  |  FAIL: %d  |  build-errors: %d\n' \
    "$total_envs" "$ok_envs" "$fail_envs" "$build_errs"
printf 'Tests: %d  |  Failures: %d\n' "$total_tests" "$total_fails"

if [ "$fail_envs" -ne 0 ]; then
    echo ""
    echo "ENVS CON PROBLEMAS:"
    for f in "${FAILED_LIST[@]}"; do echo "  - $f"; done
    echo ""
    echo "Logs en: $OUT_DIR"
    exit 1
fi

echo ""
echo "TODO VERDE. Logs en: $OUT_DIR"
exit 0
