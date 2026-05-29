---
title: "Diagnóstico de motores de la placa CENTRAL (Zircon Rev v15)"
date: 2026-05-28
status: vivo
audiencia: "Virginia / Elías / Enzo — operativos en el banco"
firmware-source: software/teensy/Soccer 2026/src/diag/diag_central_motors.cpp
environment: "pio run -e diag_central_motors"
author: "Claude Opus 4.7 (Anthropic)"
requested-by: "Gustavo Viollaz (@gviollaz)"
---

# `diag_central_motors` — Test individual de los 3 motores del Zircon

## Para qué sirve

Sketch standalone para validar **en banco** que los 3 H-bridges del Zircon
Rev v15 funcionan, y para **mapear empíricamente** qué número de motor del
firmware (1/2/3) corresponde a qué rueda física del robot. Cada motor se
prueba uno a la vez con una onda PWM senoidal (sentido horario, 0 → 128 → 0
repetido), avanzando al siguiente cuando se aprieta el botón de la placa.

**Bonus crítico**: el mismo sketch resuelve empíricamente el conflicto P0
pines 7/8 documentado en
[`hardware/electronics/central-board-pack/01-pinout-y-hardware.md §8`](../../hardware/electronics/central-board-pack/01-pinout-y-hardware.md).
Si el motor 2 (driver U17, pines 7/8/6) **NO se mueve** cuando le toca el
turno, eso confirma que los pines 7/8 están cableados como `Serial2` hacia
DOWN y NO como motor — entonces no hay conflicto. Si el motor 2 **sí se
mueve**, los pines 7/8 son del motor y hay que migrar `Serial2` a otro UART
libre (ej. `Serial7` en pines 28/29) en el firmware CENTRAL de producción.

## Lo que NO es

- ❌ NO es firmware de competencia. Es diagnóstico de banco.
- ❌ NO depende de `config_central.h` ni del rol — los pines están
  hardcodeados en el sketch según `mapa-pines-teensy-ambos-robots.md` del
  2026-03-20. Si el cableado físico difiere, **editar la tabla `MOTORS[]`**
  arriba del sketch.
- ❌ NO prueba dirección antihoraria por default. Para invertir, compilar
  con `-DDIAG_MOTORS_REVERSE`.

## Procedimiento operativo

### Pre-requisitos

- Zircon Rev v15 con Teensy 4.1 enchufado.
- Cable USB al PC.
- Los 3 motores cableados a sus drivers del Zircon (U5, U17, U7).
- **Robot SUJETO al banco** o con **las ruedas al aire**. El sketch hace
  girar el motor a 50% de PWM máximo — el robot puede salir corriendo si
  está apoyado sobre las ruedas en el piso.
- Excepción Avast aplicada (TASK-025) si PlatformIO se traba al bajar
  paquetes. El sketch en sí no usa libs externas, sólo Arduino core.

### Paso 1 — Compilar y flashear

```bash
cd "software/teensy/Soccer 2026"
pio run -t clean -e diag_central_motors
pio run -e diag_central_motors -t upload
```

### Paso 2 — Abrir Serial Monitor

```bash
pio device monitor -b 115200
```

Tenés que ver el banner inicial:

```
==============================================
 diag_central_motors  -  Zircon Rev v15
==============================================
 Apreta el BOTON (pin 9) para arrancar el test.
 (Tambien se acepta cualquier input por Serial.)
...
```

Si el banner no aparece: revisar que el `-b 115200` esté bien, que el
Teensy esté reconocido (`pio device list` debería mostrarlo), y que el
LED del Teensy parpadee a 2 Hz (heartbeat de `WAITING_START`).

### Paso 3 — Correr el test

Apretás el botón físico (pin 9, "Botón 1" del Zircon legacy). Si el botón
no está cableado físicamente, mandar **cualquier línea por Serial Monitor**
también avanza (backup).

| Apretón # | Acción |
|---|---|
| 1 | Motor 1 (driver U5) arranca con onda creciente/decreciente |
| 2 | Motor 1 para, Motor 2 (driver U17) arranca |
| 3 | Motor 2 para, Motor 3 (driver U7) arranca |
| 4 | Motor 3 para, FIN. Reset del Teensy para repetir. |

Cada motor gira durante el tiempo que tarde la persona en apretar de nuevo
— no hay timeout. La onda completa un ciclo cada 2 segundos.

### Paso 4 — Anotar mediciones

Completar esta tabla a mano (impresa o en libreta):

| Motor del firmware | Pines | Driver | Rueda física observada | ¿Gira? | Sentido (horario / antihorario / no aplica) |
|---|---|---|---|---|---|
| Motor 1 | INA=2, INB=5, PWM=3 | U5 | _____________________ | ☐ Sí ☐ No | _____ |
| Motor 2 | INA=8, INB=7, PWM=6 | U17 | _____________________ | ☐ Sí ☐ No | _____ |
| Motor 3 | INA=11, INB=12, PWM=4 | U7 | _____________________ | ☐ Sí ☐ No | _____ |

Subir foto de la tabla a la entrada del journal de la sesión (ver Paso 6).

## Interpretación de resultados

### Caso A — Los 3 motores giran

Cableado del firmware coincide con el hardware. **Pero ojo**: si el motor 2
giró, los pines 7/8 **son del motor** — entonces el conflicto pines 7/8 es
real y hay que **migrar `Serial2` a otro UART libre** (recomendado
`Serial7` en pines 28/29 — están libres según el pinout) en
`src/central/comm_down.h` antes de poder probar comm DOWN→CENTRAL.

→ Acción siguiente: TASK nueva "Migrar Serial2 CENTRAL a Serial7" + cambio
físico de cable (recablear conector DOWN↔CENTRAL en el lado CENTRAL para
que vaya a pines 28/29).

### Caso B — Motor 2 NO gira, motores 1 y 3 sí

Confirma que los pines 7/8 **no son del motor** — son sólo `Serial2` hacia
DOWN. El doc del 2026-03-20 estaba describiendo el firmware 2025 donde sí
eran motor. El conflicto pines 7/8 **no existe** en el Zircon Rev v15.

→ Acción siguiente: cerrar TASK del conflicto pines 7/8 como "no aplica al
v15". Actualizar `central-board-pack/01-pinout-y-hardware.md` borrando el
warning del §8. Investigar qué motor está realmente en el driver U17 (si
es que U17 existe físicamente en este PCB).

### Caso C — Ningún motor gira

Problema más serio. Posibles causas:
- Batería sin conectar o descargada (el USB alimenta sólo al Teensy, no a
  los H-bridges).
- Drivers no soldados o sin alimentación de potencia.
- Pinout del sketch no coincide con el Zircon (raro, está basado en doc del
  2026-03-20).

→ Acción siguiente: verificar con multímetro que llega tensión a los
drivers, que la batería está cargada, y que las salidas PWM del Teensy
están conmutando (sondear con osciloscopio el pin PWM del motor que
debería estar activo).

### Caso D — Otro patrón inesperado

Anotar literalmente lo que pasó y cualquier sorpresa (luz extraña, ruido,
olor a quemado, motor que tiembla pero no gira, etc). **Las sorpresas son
los datos más valiosos** — no las tirés, vienen acá y las analizamos.

## Pendientes que este test NO cubre

| # | Pendiente | Cómo se cierra |
|---|---|---|
| 1 | Dirección antihoraria (probar inverso) | Recompilar con `-DDIAG_MOTORS_REVERSE` y repetir |
| 2 | Confirmar `PIN_KICKER_SOL` (TASK-011) | Con multímetro entre pin 23 del Teensy y la compuerta del MOSFET del solenoide. Test separado, no en este sketch (kicker requiere cuidado: pulso corto, cooldown 1.5 s). |
| 3 | Cinemática real del robot (rueda → vector de velocidad) | Con los 3 motores identificados, mañana se puede usar un sketch de movimientos vectoriales para confirmar `WHEEL_ANGLES_DEG` y `WHEEL_RADIUS_MM` empíricamente. |
| 4 | Saturación proporcional con los 3 motores activos simultáneamente | Idem: sketch de movimientos vectoriales (planeado para mañana). |
| 5 | Encoders (si llegaran a sumarse) | Fuera de scope Incheon. |

## Documentación esperada (journal entry post-test)

El equipo que ejecute este test (Virginia / Elías / Enzo) debe dejar una
entrada en `journal/YYYY-MM-DD-diag-central-motors-<descripcion>.md` con:

- Foto del setup (Zircon + Teensy + batería + cable USB + lugar donde apoyó
  el robot).
- Tabla de Paso 4 completada.
- Veredicto: Caso A / B / C / D + razonamiento.
- Próxima TASK abierta (si corresponde): conflicto resuelto, motores re-rotulados,
  cableado a verificar, etc.
- Tiempo total de la sesión de test (estimación honesta).

## Referencias

- Sketch: [`software/teensy/Soccer 2026/src/diag/diag_central_motors.cpp`](../../software/teensy/Soccer%202026/src/diag/diag_central_motors.cpp)
- Environment: `[env:diag_central_motors]` en
  [`platformio.ini`](../../software/teensy/Soccer%202026/platformio.ini)
- Pinout fuente: [`hardware/electronics/central-board-pack/01-pinout-y-hardware.md`](../../hardware/electronics/central-board-pack/01-pinout-y-hardware.md)
- Doc del 2026-03-20: [`hardware/electronics/mapa-pines-teensy-ambos-robots.md`](../../hardware/electronics/mapa-pines-teensy-ambos-robots.md)
- Hermano (placa DOWN): `pio run -e diag_down -t upload` + `journal/2026-05-24-hardware-up-down-anillo-linea.md`
- Hermano (cámaras front/back): `cameraFront-pack/` y `cameraBack-pack/`

## Cambios

- 2026-05-28 — creación inicial. Sketch + environment + doc + actualización
  de `ESTADO-ACTUAL.md` y `FUENTES-DE-VERDAD.md`. Author: Claude Opus 4.7
  (Anthropic). Requested-by: Gustavo Viollaz (@gviollaz).
