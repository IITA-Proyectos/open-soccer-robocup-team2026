---
id: TASK-028
title: "Documentar regla operativa: hardware-up de la placa DOWN requiere power cycle completo"
date_created: 2026-05-24
assigned: [gviollaz, virginia-viollaz, elias]
priority: P1
status: pending
estimated_hours: 0.5
blocks: []
tags: [operativa, hardware-up, down-board, otos, regla-no-negociable]
---

# TASK-028 — Regla operativa: hardware-up con power cycle completo

## Resumen

Durante la sesión 2026-05-24 (activación de la lib SparkFun OTOS) se
descubrió un comportamiento reproducible: la placa DOWN **no inicializa
correctamente los OTOS** si la batería se conecta sobre una placa que ya
tenía USB conectado y firmware corriendo. Síntoma: scanner I²C reporta
ambos buses vacíos (sin dispositivos en ningún address), `[L=-- R=--]`,
ningún LED encendido en los módulos OTOS.

La solución es siempre **power cycle completo**: desconectar batería +
USB, esperar 10 segundos, reconectar (batería primero, después USB).

## Contexto

Ver journal `journal/2026-05-24-otos-lib-activada-y-power-cycle-bug.md`
para diagnóstico completo con datos del scanner I²C antes y después del
power cycle.

Causa probable (a confirmar): los reguladores buck MP1584 que generan
3.3V para los OTOS pueden quedar en un estado degradado si se aplica
power "en caliente" (batería sobre placa ya alimentada por USB), y
necesitan un arranque "frío" limpio para entregar el voltaje correcto.

## Lo que hay que hacer

### Documentación (Claude o humano)

1. Agregar nota en `docs/firmware/SETUP-ENTORNO-BUILD-WINDOWS.md` (o el
   doc operativo equivalente) en sección "Encendido inicial de la placa
   DOWN" — con el procedimiento de power cycle.
2. Agregar nota en `hardware/electronics/down-board-pack/README.md` en
   sección "Cómo verificar el hardware" — apuntando a esta task.
3. Actualizar la sección "Hardware status" de `src/down/config_down.h`
   con un comentario corto del estilo "primer arranque siempre requiere
   power cycle completo, ver TASK-028".

### Investigación de causa raíz (Enzo o quien sepa de hard)

4. **Hipótesis principal**: los reguladores buck o algún capacitor de
   bypass del rail 3.3V no estabilizan bien cuando se conecta batería
   sobre placa caliente. Verificar:
   - Tensión del rail 3.3V con multímetro en escenario "batería en
     caliente" vs "batería en frío".
   - Si hay diferencia significativa, posible fix: agregar capacitor de
     bulk al rail 3.3V, o agregar un retardo de soft-start.
5. **Hipótesis secundaria**: el `Wire.begin()` se ejecuta antes de que
   los OTOS terminen de inicializar internamente. Posible mitigación en
   firmware: agregar `delay(200)` después de `Wire.begin()` y antes de
   `g_otos_left.begin(Wire)`. Probar y documentar.

## Criterio de cierre

- [ ] Doc operativo actualizado con procedimiento de power cycle.
- [ ] Comentario corto en `config_down.h` referenciando TASK-028.
- [ ] Investigación de causa raíz documentada (multímetro + experimento
      controlado) en journal nuevo o ampliación del journal 2026-05-24.
- [ ] Si se confirma hipótesis secundaria (delay después de Wire.begin)
      y resuelve el problema, aplicar fix en `src/down/otos.cpp`.

## Cambios de estado

- 2026-05-24: creada al detectar el comportamiento durante la activación
  de la lib OTOS (TASK-012). Confirmado reproducible.
- 2026-05-29: **el bug se repitió** en banco (sesión con María). Síntoma nuevo:
  un bus vacío (U5) + el otro respondiendo en **`0x64`** (brownout) en vez de
  `0x17`. Causa confirmada: la batería estaba **conectada pero SIN entregar
  corriente** → el riel 3.3 V del MP1584 (que alimenta los OTOS) quedó hambriento.
  Fix: batería entregando corriente + power cycle completo → ambos OTOS en `0x17`,
  OK. **Refinamiento de la regla**: no alcanza con que la batería esté enchufada;
  tiene que estar **ENTREGANDO corriente** (switch ON / cargada). Los 32 sensores
  de luz pueden seguir leyendo con el riel flojo, pero los OTOS no → no usar "los
  sensores andan" como prueba de que el 3.3 V está sano. Nota elevada a checklist
  visible en `docs/ESTADO-ACTUAL.md`. Sigue **pending** el resto (doc operativo
  formal + medición multímetro del 3.3 V, P0.3). Ver
  `journal/2026-05-29-otos-revividos-power-bateria.md`.
