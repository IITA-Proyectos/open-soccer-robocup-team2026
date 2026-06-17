---
task: TASK-111
titulo: "Validar el lazo RT de detección de línea en banco (ambos robots) y promover a competencia"
fecha-creada: 2026-06-17
creada-por: "Claude (sesión coach — Opus 4.8 1M)"
asignado: "equipo (banco con robot + cancha)"
prioridad: P0
estado: ABIERTA (firmware listo; banco pendiente)
placas: DOWN (+ efecto en CENTRAL)
origen: "banco domingo: 'las líneas blancas del borde tardaban en detectarse'"
---

# TASK-111 — Validar lazo RT de línea y promover

**Síntoma del banco (domingo):** las líneas blancas del borde de la cancha tardaban en
detectarse. **Causa verificada (código):** el binario de partido de DOWN (`down` / `down_robot2`)
NO trae las mejoras RT del lazo de línea — corre el barrido lento (~717 µs) y, en R1, el
spike I²C del OTOS (3-4 ms) bloquea la lectura de luz (confirmado en `platformio.ini:1781`).

Las mejoras YA existen gateadas. Se completó la paridad: ahora **ambos robots tienen su env RT**.

## Envs a probar

- **R2:** `down_robot2_rt` (ya existía) = ADC_FAST + ADC_DUAL + EARLY_EVIDENCE + RELIABLE_GATE + RX_HARDEN.
- **R1:** `down_rt` (NUEVO 2026-06-17) = lo de R2 **+ OTOS_FAST_I2C** (R1 tiene OTOS → necesita destrabar el spike).
- Variantes _bench (`down_robot2_rt_bench` / `down_rt_bench`) = + loop-monitor + debug serial para MEDIR.

## Qué mejora (medible)

- Barrido de luz: ~717 µs → ~126 µs (ADC fast + dual).
- R1: spike OTOS 3-4 ms → <0,6 ms (OTOS fast I²C).
- F3 (early evidence): el aviso de línea (`line_present`/`sensors_on_line`/`escape_angle`)
  sale ANTES, sin esperar los 6 sensores del "imminent". Aditivo (el freno duro nunca llega más tarde).

## Checklist de banco

- [ ] Flashear el _bench primero (`down_rt_bench` R1 / `down_robot2_rt_bench` R2) y leer
      `loop_us`/`scan_us` por serie: confirmar barrido ~126 µs y (R1) spike OTOS <0,6 ms.
- [ ] ADC averaging=1 (F1): confirmar que NO flickea el umbral de línea (carpet vs blanco estable).
- [ ] F3 detección temprana: robot a velocidad conocida cruzando la línea → medir penetración_mm
      al aviso temprano vs el inminente, y **0 falsos positivos** en marcha normal sobre carpet.
- [ ] Confirmar que el robot detecta el borde **notablemente más rápido** (el síntoma del domingo).
- [ ] **Si pasa:** promover el flag set al binario de partido (`down` y `down_robot2`).
      **Si algo falla:** flashear el respaldo (`down` / `down_robot2`).

## Relación con la oscilación

Detectar la línea más tarde obliga a frenar más tarde → contribuye a comportamiento errático
cerca del borde. Bajar la latencia de detección ayuda a la estabilidad además de la seguridad.
Combinar con la calibración de potencias de rueda (TASK separada) y el tuning del PID de rumbo.

Journal: `journal/2026-06-17-ruta-latencia-y-lazo-rt.md`.
