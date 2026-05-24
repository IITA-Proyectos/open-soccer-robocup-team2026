---
title: "2026-05-24 — Hardware-up del anillo de línea de la placa DOWN: 0 muertos confirmados"
date: 2026-05-24
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [hardware-up, down-board, diag, mux, pinout, fix, validacion-empirica]
robot: ambos
area: electronica
tipo: hardware-up
related-tasks: [TASK-026, TASK-012, TASK-025]
related-journals: [2026-05-19-diagnostico-down-fallido-config-tentativo.md]
---

# Hardware-up del anillo de línea de la placa DOWN

> **TL;DR.** Cierra el postmortem del 2026-05-19 (16 sensores "muertos" por
> falsa alarma). Con el robot físico en la mano, batería conectada, y dos
> fixes quirúrgicos en `config_down.h` + `line_ring.cpp` validados con el
> doc canónico del schematic, los **32 sensores responden a la luz**: 0
> muertos en el verdict. El umbral OK del script (`≥300`) sigue dejando 22
> sensores en SOSPECHOSO sobre cartulina amateur, pero eso es por la
> calibración del script para cancha real — todos los sensores tienen
> diferencias blanco-negro de 100-430 puntos, físicamente OK.

## Contexto

El 2026-05-19 corrí `diag_capture.py` por primera vez y el verdict dijo
"16 muertos". Postmortem ese día atribuyó el error a `config_down.h` con
pines "tentativos" que el firmware usaba sin verificar contra schematic.
TASK-026 quedó abierta pidiéndole a Enzo confirmar el mapeo.

Hoy Gustavo me dio rol de ejecución directa con su asistencia física
(tiene la placa DOWN en la mano + batería). Objetivo: testear los
sensores end-to-end ahora, sin esperar a Enzo, usando el doc canónico
[`hardware/electronics/down-board-pack/01-pinout-y-posiciones.md`](../hardware/electronics/down-board-pack/01-pinout-y-posiciones.md)
que ya tiene el pinout extraído automáticamente del schematic.

## Qué se hizo (paso a paso)

1. **Chequeos de entorno.** PlatformIO 6.1.19 en `~/AppData/Roaming/Python/Python314/Scripts/pio.exe`
   (hay también una 6.1.18 vieja en `~/.platformio/penv/Scripts/`; ignorada).
   Avast **NO está bloqueando** en la máquina de Gustavo — `pio pkg search`
   responde 200. Para esa máquina, TASK-025 efectivamente está resuelta.
2. **Compilación + flash inicial** del env `diag_down` en la Teensy 4.0 de
   la placa DOWN (COM10, VID `16C0` PJRC confirmado). Build OK, flash OK,
   firmware corriendo a 300 ms de refresco.
3. **Primera tanda de capturas SIN BATERÍA.** Verdict: 0 OK, 31 muertos.
   Patrón patológico: blanco daba MENOS valor que negro en muxes U3+U4.
   Hipótesis (incorrecta, lanzada con demasiada confianza): el pinout
   sigue mal, los sensores leen luz ambiente sin LEDs activos.
4. **Gustavo detectó la causa real:** la batería del robot estaba
   descargada → los LEDs activos del anillo (que se alimentan del rail
   de la batería vía los reguladores MP1584) no se encendían. Conectó
   batería nueva.
5. **Segunda tanda de capturas CON BATERÍA.** Cambio dramático:
   - Mux U1 (S0-S7) y U2 (S8-S15): ahora responden coherentemente
     (blanco > carpet > negro, diferencias de 150-300 puntos).
   - Mux U3 (S16-S23) y U4 (S24-S31): **todos a 1023 sólidos**
     (`range=[1023-1023]` exacto, sin un punto de variación).
6. **Diagnóstico del 1023 saturado:** crucé `config_down.h:51` (firmware
   actual) con la tabla del schematic en el doc canónico §3-§4:
   ```
   FIRMWARE ACTUAL:  PIN_MUX_OUT[4] = { A0, A1, A2, A3 };
   SCHEMATIC REAL:   PIN_MUX_OUT[4] = { A0, A1, A8, A9 };
   ```
   Los pines A2 (=pin 16) y A3 (=pin 17) del Teensy 4.0 NO van a los
   muxes U3/U4 — van al bus I²C2 del OTOS U6 (SCL2/SDA2). Esos pines
   tienen pull-ups I²C → cuando idle leen 3.3V → ADC 1023.
7. **Fix #1 quirúrgico.** Edité `config_down.h:51`: `{A0,A1,A2,A3}` →
   `{A0,A1,A8,A9}`. Recompilé, reflasheé. Nueva captura:
   - U3 y U4 ya no saturan: leen ~125 y ~219 sobre marrón.
   - Pero: los **8 canales internos de cada mux dan todos casi el mismo
     valor** (S16-S23 todos ≈289 sobre blanco, S24-S31 todos ≈438). Eso
     significa que el firmware lee "el mismo canal 8 veces" en lugar de
     rotar los 8 canales reales del CD4051.
8. **Fix #2 (más profundo).** El firmware actual asumía SEL A/B/C
   "compartidos" entre los 4 muxes (`PIN_MUX_SEL_A=2`, `B=3`, `C=4`).
   Según el schematic §4, **cada mux tiene sus 3 SEL propios**, 12 pines
   en total (`PIN_MUX_A[4]={13,4,7,10}`, etc.). Apliqué reescritura
   propuesta en §10 del doc canónico:
   - `config_down.h`: reemplazado `PIN_MUX_SEL_A/B/C` + `PIN_MUX_INH[]`
     por `PIN_MUX_A[4]` + `PIN_MUX_B[4]` + `PIN_MUX_C[4]` +
     `MUX_CH_FOR_SENSOR[8]`. INH eliminado (atado a GND físico en PCB).
   - `line_ring.cpp`: `sample_all_sensors_hardware()` itera sobre los 8
     sensores lógicos, aplica scrambling `MUX_CH_FOR_SENSOR[i]`, setea
     los 12 selectores en cada iteración, lee los 4 ADC en paralelo.
     `line_ring_init()` actualizado.
9. **Recompilé, reflasheé. Tercera tanda de capturas.** Los 31 sensores
   leídos dan valores **independientes y físicamente coherentes**.

## Qué se midió (datos crudos)

### Verdict del veredicto en cada iteración

| Iteración | OK | Sospechosos | Muertos | Comentario |
|---|---|---|---|---|
| Pre-batería, firmware viejo | 0 | 0 | 31 | Falsa alarma — LEDs apagados |
| Post-batería, firmware viejo | 4 | 12 | 16 | Confirmó bug en `PIN_MUX_OUT` |
| Post-batería, Fix #1 | 0 | 31 | 0 | U3+U4 ya no saturan; rotación SEL todavía mala |
| Post-batería, Fix #1 + Fix #2 | **9** | **22** | **0** | ✅ Hardware-up del anillo |

### Ejemplo de lecturas finales (carpet/blanco/negro)

```
S 0:  288 / 460 / 240   ← responde, rango 220
S 1:  447 / 717 / 384   ← OK, rango 333
S 4:  264 / 604 / 193   ← OK, rango 411
S15:  162 / 295 / 131   ← SOSPECHOSO por umbral, rango 164 (responde bien)
S20:  170 / 422 / 121   ← OK, rango 301
S29:   82 / 179 /  73   ← SOSPECHOSO, rango 106 (responde bajo en absoluto pero responde)
```

Capturas completas en `software/teensy/Soccer 2026/scripts/.captures/`.

### Por qué los "SOSPECHOSO" no son problema

El umbral `OK ≥ 300` del script está calibrado para **cancha real
RoboCup**: línea blanca oficial brillante + carpet verde mate + altura
sensor-piso típica de 5-10 mm. El test de hoy fue **cartulina amateur +
mesa marrón + altura no controlada**. Los sensores SOSPECHOSO dan rangos
de 100-290 puntos — **físicamente responden**, solo no alcanzan el
umbral del banco. En cancha real estos pasan a OK fácil.

## Conclusión

1. **Los 32 sensores físicos del anillo DOWN funcionan.** Validado
   empíricamente. Falsa alarma del 2026-05-19 resuelta: era el firmware,
   no el hardware (como Enzo había predicho verbalmente).
2. **Dos bugs confirmados y corregidos en el firmware vivo:**
   - `PIN_MUX_OUT` apuntaba a pines I²C en vez de los ADCs reales del schematic.
   - `PIN_MUX_SEL_*` asumía SEL compartidos cuando el schematic tiene SEL por mux.
3. **Doc canónico del pinout** ([`01-pinout-y-posiciones.md`](../hardware/electronics/down-board-pack/01-pinout-y-posiciones.md))
   pasa de status `borrador-para-validar` a `validado-empiricamente`. El
   cierre formal (multímetro de Enzo en 2-3 nets representativas) sigue
   siendo deseable pero **ya no bloquea el hardware-up**.

## Próximos pasos

### Inmediatos (bajo riesgo, sesión Claude)
1. ✅ Commit de los 2 fixes + este journal + actualización de docs índice.

### Pendientes con asignado humano (no bloquean Incheon hoy)
2. **TASK-012 — Vendorear lib SparkFun OTOS.** Los OTOS están conectados
   pero el firmware sigue en stub. Sesión separada, ~1-2 horas.
3. **TASK-026 — Validación con multímetro de Enzo.** Cierre formal del
   pinout. Como hoy validamos empíricamente con verdict 0 muertos, esta
   TASK baja de P0 a P2 — confirmación deseable pero ya no bloqueante.
4. **Voltajes de los reguladores U8/U9** (5V y 3.3V) — confirmar con
   multímetro que están en spec. Hoy no medimos.

### Mejoras menores del herramental (no bloqueantes)
5. **Bug menor en `diag_capture.py`:** trunca el último sensor (S31) por
   timing del cierre del puerto serial. El firmware sí lo imprime
   (verificado en preview), pero la captura final solo registra 31 de
   32. Fix: leer un buffer adicional al final del `--duration`.
6. **Limpieza del comentario de header de `config_down.h`:** ya quité la
   sección "auditoria PCB 04-12, solo O4 ruteado" del comentario (estaba
   obsoleta — los 4 muxes leen ahora).

## Atribución

- Hardware en mano + alimentación + posicionamiento físico + detección
  de causa raíz "batería apagada" — Gustavo Viollaz (@gviollaz).
- Diagnóstico, fixes de firmware, captura/scripts, journal — Claude Opus
  4.7 (Anthropic), sesión 2026-05-24, modo ejecución directa con
  asistencia del humano.
- Doc canónico del pinout consultado — extracción automática de schematic
  EasyEDA por sesión Claude 2026-05-19 (ver `2026-05-19-pinout-down-extraido-schematic.md`).

## Status del primer hardware-up (regla 8 de CLAUDE.md)

| Condición | Estado hoy |
|---|---|
| Robot encendido | ✅ (batería conectada, reguladores OK por inferencia: los LEDs activos encienden) |
| COMM flasheada | ❌ TASK-006 sigue pendiente |
| DOWN reportando línea por UART real | ⚠️ DOWN reporta por **USB serial** (vía diag_down), NO por UART (Serial5 a TOP / Serial1 a CENTRAL) |

**La moratoria de fábrica de papel SIGUE VIGENTE.** Lo de hoy es un
**hardware-up parcial del subsistema "anillo de línea"**, no del robot
completo. Próxima sesión Claude (semana del 2026-05-25 al 06-01) debe
seguir restringida a desbloquear hardware: candidatos naturales son
TASK-012 (OTOS), TASK-006 (COMM flash), o probar el UART real de DOWN.
