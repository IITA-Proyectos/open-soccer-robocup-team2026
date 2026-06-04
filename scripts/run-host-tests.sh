#!/usr/bin/env bash
# run-host-tests.sh — corre la suite de tests host con g++ contra el Unity
# vendoreado en lib/Unity, esquivando `pio test` (que SIEMPRE resuelve
# throwtheswitch/Unity del registry, y Avast lo bloquea — TASK-025).
#
# Por qué existe: ESTADO-ACTUAL referenciaba este script pero no estaba creado.
# Creado 2026-06-03 al implementar las correcciones del audit del firmware DOWN
# (research/in-progress/2026-06-03-auditoria-multiagente-firmware-down.md).
#
# Uso:
#   bash scripts/run-host-tests.sh             # corre todos los test/test_*/
#   bash scripts/run-host-tests.sh test_proto  # corre uno solo
#
# Linkea src/shared (módulos PUROS host-testeables) + headers de src/down.
# Limitación conocida: los tests que dependen de src/central o src/top (que usan
# Arduino: Serial/Wire/analogWrite) no compilan host con este linker simple y se
# reportan como SKIP. Los de la cadena DOWN + shared sí corren.

set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FW="$ROOT/software/teensy/Soccer 2026"
cd "$FW" || { echo "no encuentro $FW"; exit 1; }
UNITY="lib/Unity/src"
FILTER="${1:-}"
TMP="$(mktemp -d)"
pass=0; fail=0; skip=0; tests=0; failed=""

for d in test/test_*/; do
  t="$(basename "$d")"
  [ -f "$d/test_main.cpp" ] || continue
  [ -n "$FILTER" ] && [ "$t" != "$FILTER" ] && continue
  if g++ -std=gnu++17 -I src/shared -I src/down -I "$UNITY" \
         "$UNITY/unity.c" src/shared/*.cpp "$d/test_main.cpp" \
         -o "$TMP/$t" 2>"$TMP/err_$t"; then
    if "$TMP/$t" >"$TMP/out_$t" 2>&1; then
      res="$(grep -oE '[0-9]+ Tests [0-9]+ Failures' "$TMP/out_$t" | head -1)"
      n="$(echo "$res" | grep -oE '^[0-9]+')"; tests=$((tests + ${n:-0}))
      echo "PASS  $t  ($res)"; pass=$((pass+1))
    else
      echo "FAIL  $t"; tail -4 "$TMP/out_$t"; fail=$((fail+1)); failed="$failed $t"
    fi
  else
    echo "SKIP  $t  (no compila host: deps de central/top/Arduino)"; skip=$((skip+1))
  fi
done

rm -rf "$TMP"
echo "===================================================="
echo "PASS=$pass  FAIL=$fail  SKIP=$skip  (tests corridos: $tests)"
[ -n "$failed" ] && echo "FALLARON:$failed"
[ "$fail" -eq 0 ]
