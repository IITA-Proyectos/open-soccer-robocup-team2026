---
id: TASK-011
title: "Confirmar PIN_KICKER_SOL del solenoide en la placa Zircon (ROBOT2)"
date_created: 2026-05-15
assigned: [enzzo195]
priority: P1
status: cancelled
estimated_hours: 1
blocks: []
tags: [hardware, zircon, robot2-delantero, kicker, firmware-central]
---

# TASK-011 — Confirmar PIN_KICKER_SOL en Zircon (ROBOT2) — CANCELADA

> **CANCELADA (2026-06-03):** El robot **NO tiene kicker físico**. No se cablea
> solenoide; el delantero empuja la pelota por inercia avanzando hacia el arco
> rival. El campo `kicker_fire` (struct `MotorCommand`), el driver del solenoide
> (`motors_zircon.cpp`) y el pin/constantes (`PIN_KICKER_SOL`, `KICKER_PULSE_MS`,
> `KICKER_COOLDOWN_MS` en `config_central.h`) fueron eliminados del firmware vivo.
> Esta task queda sin objeto. El texto de abajo se conserva como registro histórico.

## Resumen

El firmware CENTRAL Nivel 2 (commit `b10c66d` del 2026-05-15) implementó el control
del kicker (solenoide) del delantero como pulso GPIO one-shot con cooldown. El
**número de pin** del GPIO del Teensy 4.1 al MOSFET del solenoide quedó como
**placeholder** porque nadie confirmó el cableado real. Necesitamos saber qué pin
del Teensy 4.1 está conectado al gate del MOSFET en el Zircon Rev v15.

## Contexto

En `software/teensy/Soccer 2026/src/central/config_central.h` quedó:

```cpp
#if defined(ROBOT2)
    constexpr int PIN_KICKER_SOL = 23;   // ⚠️ A CONFIRMAR ENZO (TASK-NUEVA)
    constexpr uint32_t KICKER_PULSE_MS    = 80;
    constexpr uint32_t KICKER_COOLDOWN_MS = 1500;
#endif
```

El `23` es un placeholder elegido porque está libre en ambos robots según el mapa
viejo de pines (`hardware/electronics/mapa-pines-teensy-ambos-robots.md` del
2026-03-20). **No fue confirmado** contra el schematic del Zircon Rev v15 ni contra
la placa fabricada.

Si el pin está mal, el firmware:
- Cargará bien (no hay error de compilación).
- **NO disparará el solenoide** cuando strategy emita `kicker_fire=1`.
- Puede dañar otra cosa si el pin del placeholder está cableado a algo
  sensible (LED, sensor, comunicación).

## Pasos concretos

### Opción A — Verificar en EasyEDA / schematic (más rápido)

1. Abrir el proyecto Zircon Rev v15 en EasyEDA o KiCad (ubicación: confirmar con
   Enzo dónde están los fuentes del Zircon).
2. Buscar el componente MOSFET del kicker. Refdes típico: `Q1`, `Q2`, etc.
   Suele ser un NMOS de potencia (IRLB8721, AO3400, etc.) cerca del conector
   del solenoide.
3. Identificar a qué pin del Teensy 4.1 (componente `U?`) va el **gate** del
   MOSFET. Mirar el net name.
4. Reportar el número de pin **lógico** (no el número de pad del footprint).

### Opción B — Verificar con multímetro en la placa

1. Identificar el conector del solenoide en la placa Zircon (2 pines: +V, GND
   con MOSFET en serie).
2. Identificar el MOSFET asociado. Su gate va a un GPIO del Teensy.
3. Con el multímetro en continuidad, medir desde el gate del MOSFET a cada pin
   GPIO disponible del Teensy 4.1 hasta encontrar el que conecta.
4. Reportar el número de pin lógico.

### Opción C — Si la pista está cortada o no existe

Si no hay un MOSFET cableado para el kicker en el Zircon Rev v15 (porque
nadie lo agregó):
- Diseñar un breakout externo: MOSFET + diodo flyback + conector al solenoide,
  conectado a un GPIO libre del Teensy + GND + alimentación del solenoide
  (separada de la lógica para no inducir ruido).
- Documentar el GPIO elegido y actualizar el firmware con ese número.
- Esto **es bloqueante** si el delantero va a patear en Incheon. Si no patea
  (estrategia conservadora Incheon), se puede diferir.

## Criterio de cierre

- [ ] Confirmado el número de pin lógico del Teensy 4.1 conectado al gate del
      MOSFET del solenoide en ROBOT2.
- [ ] Actualizado `config_central.h` (`PIN_KICKER_SOL = <pin real>`) en una PR
      o commit con atribución correcta.
- [ ] Comentario `⚠️ A CONFIRMAR ENZO (TASK-NUEVA)` removido del archivo.
- [ ] Documentado el resultado en `hardware/electronics/mapa-pines-teensy-ambos-robots.md`.
- [ ] Si Opción C aplica (no hay MOSFET): nuevo task para diseño del breakout
      externo + decisión coach sobre si se hace para Incheon.

## Notas / decisiones

_(actualizar cuando se ejecute)_

## Cambios de estado

- 2026-05-15: creado por Claude tras quedar el placeholder en el commit `b10c66d`
  del firmware CENTRAL Nivel 2 (behind-the-ball + kicker + KICKOFF + GK_CLEAR).
- 2026-06-03: **CANCELADA**. Decisión de equipo: el robot no lleva kicker físico
  (el delantero empuja la pelota por inercia). Se eliminó todo el código y la
  configuración del kicker del firmware vivo. La task queda sin objeto.
