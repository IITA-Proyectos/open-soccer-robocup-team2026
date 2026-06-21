---
title: "arqueromix — el avance de arranque sale de la línea HASTA despegar (no por reloj fijo)"
date: 2026-06-21
author: "Claude (Opus 4.8, 1M context) — coach, pedido de Virginia (banco) + workflow de análisis"
status: COMPILA · NO validado en banco
scope: software/teensy/Soccer 2026/src/arqueromix/
tipo: feature-banco
---

# arqueromix — salida del área hasta despegar de la línea

## Pedido (Virginia)

"Funciona, pero está muy cerca de la línea de fondo, se está metiendo dentro del área chica. Hacé un
cambio chico manteniendo la misma lógica: al encender retroceder un poco, cuando detecta línea blanca,
avanzar para salirse de la línea con impulso corto, y recién comenzar a patrullar izq-der con el arco
centrado atrás. Analizá en profundidad, guardá la versión que funciona, y que sea confiable 100%."

## Versión guardada (punto de retorno)

Tag **`arqueromix-funciona-arco-fallback-2026-06-21`** (commit `2558427`, pusheado). Volver: ese tag.

## Análisis en profundidad (workflow paralelo: 3 lectores + síntesis)

Hallazgos verificados contra el código real:
1. **La línea NO se puede clasificar fondo-vs-lateral de forma confiable** en arqueromix. A `AmixIO`
   solo llegan `line_present`/`line_angle_deg`/`line_depth`; el ángulo colapsa a 0 si `data_valid=0`,
   y NO llegan `cross_track_mm`/`penetration_mm`/`event_flags` (el DOWN los calcula, el contrato plano
   los recorta). El contrato DOWN dice explícito: "la semántica de cancha la pone CENTRAL (con heading)".
   arqueromix no tiene heading-mapeo → no clasifica.
2. **Profundidad:** R2 NO tiene OTOS (sin odometría). La distancia al arco por cámara está sin calibrar
   (`CAMERA_UNIT_TO_MM=10` placeholder, ~50-60% confiable). La señal más confiable = la **línea local** de DOWN.
3. **Raíz del problema = el ARRANQUE, no la patrulla.** Hoy `inicio_avanzar` avanza un **tiempo FIJO
   (400 ms)** para salir de la línea. Si ese tiempo es corto para donde frenó, queda **pisando la línea
   / medio en el área**. Es exactamente lo que Virginia describe del arranque.

## Cambio (mínimo, 100% confiable, sin cámara)

UN solo cambio de lógica, en `inicio_avanzar`: el avance **deja de terminar por reloj** y termina
cuando el robot **realmente despegó de la línea**:
- Sale si: (impulso MÍNIMO cumplido **Y** `!linea()`)  **o**  (tope de seguridad).
- `avanzar_inicio()` (M1=+,M2=-,M3=0) avanza RECTO al frente = hacia el campo, lejos del fondo → es la
  dirección que SACA del área. Reusa primitivas existentes; NO se agrega estado; NO se toca la patrulla.

Perillas (reemplazan `AMIX_T_INICIO_AVANCE=400`):
- `AMIX_T_INICIO_AVANCE_MIN = 400` (impulso mínimo = comportamiento de antes → sin regresión).
- `AMIX_T_INICIO_AVANCE_SAFETY = 1200` (tope: si nunca deja de ver línea, patrulla igual; no se traba).

## Por qué es confiable 100%

- Cero dependencia de cámara o calibración (solo el sensor de línea local de DOWN).
- En el ARRANQUE, por geometría (retrocede recto al fondo), la línea que pisa ES la de fondo → `linea()`
  alcanza, no hace falta clasificar. La ambigüedad fondo-vs-lateral NO aplica en este contexto.
- Default = comportamiento actual (MIN=400) + seguro de despegue → no empeora lo que ya andaba.

## ⚠️ Lo que este cambio NO resuelve (honesto)

NO protege contra la deriva-hacia-atrás DURANTE la patrulla (si la cámara pierde el arco y no hay
rebote). Eso es un problema separado, depende de la cámara/arco, y NO se mezcla acá. Si en banco se ve
que deriva al fondo MIENTRAS patrulla (no solo al arrancar), es otra TASK.

## Verificación

- `pio run -e central_robot2_arqueromix` → **SUCCESS**.
- ⚠️ Compila ≠ anda. Lo cierra el equipo en banco (regla #1).

## Plan de prueba en banco (Virginia)

1. Robot adelantado (lejos del fondo), GO: retrocede→ve línea→avanza→patrulla, **igual que antes** (no-regresión).
2. Robot PEGADO a la línea de fondo / medio en el área, GO: el avance debe **despegarlo completo** de la
   línea antes de patrullar. Marcá con cinta dónde queda: debe estar **fuera del área**.
3. Repetí el paso 2 desde 5 posiciones. Objetivo 5/5 fuera del área.
4. Tuneo: si queda pisando → subí `AMIX_T_INICIO_AVANCE_SAFETY` (1200→1400). Si avanza de más → bajalo.
5. No-regresión: confirmá que la patrulla izq-der + rebote (arco/línea) siguen idénticos.

## Archivos

- `amix_fsm.cpp` (estado `inicio_avanzar`: nueva condición de salida).
- `amix_config.h` (`AMIX_T_INICIO_AVANCE` → `_MIN` + `_SAFETY`).
- `DOCUMENTACION.md` (§16) + `FSM-ARQUERO-MIX-EXPLICADA.md` (referencias actualizadas).
- NO se tocó: `amix_motors.cpp`, la patrulla, el despeje, ni nada de cámara.
