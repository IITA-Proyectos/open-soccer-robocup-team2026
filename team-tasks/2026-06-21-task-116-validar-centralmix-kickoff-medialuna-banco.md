# TASK-116 — Validar en banco el estado de arranque (kickoff medialuna) del centralmix

- **Placa:** CENTRAL (R1, delantero) + depende de TOP (R1) con el BNO andando.
- **Asignado:** equipo (banco) — Gustavo / Elías
- **Prioridad:** P2 (prueba; el centralmix es experimental, no es binario de competencia)
- **Estado:** abierta
- **Build:** `pio run -e central_robot1_mix_bno -t upload` (⚠️ **sin compilar por Claude** —
  shell de la máquina rota; **compilar primero**). El kickoff es el primer estado (sin flag, va
  incluido). Escape: cualquier env de competencia (`central_robot1` / `central_robot1_delantero_practica`).

## Por qué
Estado de arranque `KICKOFF_SEEK` (PRIMER estado del FSM, sin flag; reemplaza a AVANCE_INICIO): al empezar, si ve la pelota
va hacia ella; si no, da un impulso fuerte y corto de medialuna hacia el centro y después busca
por giro. Ver journal `2026-06-21-centralmix-kickoff-medialuna.md`. Depende de que el heading del
BNO ande (TASK-115).

## Pre-requisito
- TOP de R1 con `top_robot1_pri_rt` (BNO andando). CENTRAL con `central_robot1_mix_bno`.

## Cómo validar (en orden)
1. **Compila**: `pio run -e central_robot1_mix_bno` → SUCCESS. Confirmar que
   `central_robot1_mix` y `central_robot1_mix_bno` SIGUEN compilando (no se rompió nada).
2. **Ruedas al aire, robot en soporte, dar GO del árbitro SIN pelota a la vista:** debe ejecutar
   `kickoff_medialuna` — un arco corto (~250 ms) — y después pasar a girar (GIRANDO). Ver el
   sentido: ¿la medialuna **curva hacia el lado del centro** de la cancha?
   - Si curva al revés → invertir los signos de **`MIX_KICKOFF_M1/M2/M3`** en `mix_config.h`.
   - Muy cerrada/abierta o torcida → ajustar cada **`MIX_KICKOFF_M1/M2/M3`** (PWM por rueda).
   - Muy corta / muy larga → **`MIX_KICKOFF_ARC_MS`**.
3. **Con pelota a la vista al arrancar:** debe ir directo a apuntar/perseguir (APUNTAR_PELOTA), NO
   hacer la medialuna.
4. **Sobre la línea:** si DOWN ve línea durante el arranque, debe priorizar el escape
   `DETECTA_LINEA_*` (no seguir la medialuna hacia afuera).
5. **En piso, kickoff real:** dar GO sin pelota cerca → ¿la medialuna lo deja mejor parado / más
   central que el arranque viejo (avanzar + girar)? Comparar A/B contra `central_robot1_mix_bno`.

## Criterio de cierre
- Compila.
- La medialuna curva para el lado correcto (hacia el centro para tu lado de saque) y es "fuerte y corta".
- Ve pelota → va a la pelota; no la ve → medialuna → búsqueda; línea → escape.
- **Decisión:** si suma → queda como arranque del delantero "mix"; si no → `central_robot1_mix_bno`
  (sin kickoff) o seguir con `src/central/`.

## Escape / rollback
Cualquier env de competencia (`central_robot1` / `central_robot1_delantero_practica`). El kickoff
ya NO es un flag (es el primer estado del centralmix); para sacarlo hay que revertir el commit del
kickoff. El centralmix sigue siendo un build aislado (no toca `src/central/`).

## Relación
- **TASK-115** (BNO por snapshot): es pre-requisito (el kickoff usa el mismo heading).
- Mejoras futuras anotadas (no en esta task): sesgar el lado de la medialuna por el arco rival /
  la línea; re-disparar el kickoff en cada STOP→GO (kickoff tras gol).
