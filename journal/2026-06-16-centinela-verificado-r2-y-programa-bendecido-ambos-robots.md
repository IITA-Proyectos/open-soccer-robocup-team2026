# 2026-06-16 — Centinela VERIFICADO en banco (R2) + programa bendecido para ambos robots

## Qué pasó

Sesión de banco con Gustavo sobre robot2, flasheando `top_robot2_pri_xval`. Resultado:
**el centinela dual-BNO + la cross-validación quedaron VERIFICADOS en R2**, y Gustavo pidió
dejar el mismo programa para los dos robots.

## Evidencia de banco (ROBOT2)

- Boot: `[IMU] PRIMARIO OK` (Wire2 24/25 @ 0x28) + `[IMU] CENTINELA init OK (2do BNO en Wire)`.
- `[i2c-scan Wire, ToF dormidos] ACK en: 0x28` → **único 0x28, sin ningún BNO en 0x29**
  (confirma físicamente la corrección del cableado).
- Giro a mano: el `hdg` trackeó suave (0 → -22 → +47 → ~1) y se clavó al frenar, **sin
  congelarse** → el bus propio Wire2 cumple su objetivo (no sufre la contención con los ToF).
- `imu_R=N` es ESPERADO: el secundario NO entra a la fusión (`TOP_BNO_PRIMARY_ONLY`), vive
  como centinela @1Hz.

## Feature del monitor: "centinela" en vez de "falla"

El monitor leía solo el flag de fusión (`g_ready[1]`, false por primario-solo) y pintaba el
2º BNO rojo "falla". Ahora distingue 3 estados:
- **en fusión** (right_ok),
- **centinela @1Hz: X°** (sano, fuera de fusión — 2da opinión),
- **no responde** (falla real).

Firmware: getter `sensors_imu_sentinel_heading_deg()` + 2 campos `imu_sentinel_ok/deg`
(`sok`/`sdeg`) en el stream DORMIDO. Monitor: `Imu.sentinel_ok/deg` + `health.py`.
Tests: `test_telemetry_top` 22/22 + monitor 236/236. (Commit a785858.)

## R1 unificado al MISMO programa (firmware-listo, falta su boot-check)

Para que "este programa" valga en R1 (que quedó arquitectónicamente idéntico a R2):
- **`Wire2.begin()` ahora es incondicional** (antes `#if ROBOT2 || TOP_BNO1_ON_WIRE2`). Era un
  bug latente de mi unificación del 2026-06-15: con el primario movido a Wire2 para ambos,
  `top_robot1` plano NO iniciaba Wire2 → primario muerto. Corregido.
- **Centinela des-gateado de `ROBOT2`** (sensors_imu.cpp:315 y :525): R1 ya tiene el 2º BNO en
  Wire @ 0x28, así que el centinela compila/corre igual para ambos.
- **Envs nuevos de R1** (espejo de R2, SIN `-DTOP_BNO_TOF_DECONFLICT` obsoleto → R1 toma el
  path del centinela igual que R2): `top_robot1_pri`, `top_robot1_pri_fastbno` (⭐ competencia),
  `top_robot1_pri_xval` (banco). Los 6 envs (R1+R2) compilan SUCCESS.

**Honestidad (regla 1):** R1 NO se probó en banco esta sesión (sus BNO estaban desconectados).
Por eso NO marco R1 como "verificado en banco" — queda el boot-check físico en **TASK-216**
(riesgo bajo: R2 ya validó el diseño con HW idéntico).

## Banco fino que NO se hizo (opcional, post-Incheon)

Analizador lógico en Wire durante la ventana de 1 Hz + medir piso de ruido del ω para fijar
`tol_base`/`consensus_k`. El centinela hoy aporta DIAGNÓSTICO (telemetría), sin failover —
suficiente para Incheon.

## Aclaración de envs (pedido de Gustavo)

Banner nuevo en `platformio.ini`: al TOP va SIEMPRE el firmware de COMPETENCIA con la
telemetría DORMIDA (despierta solo con USB+app/ENTER; en partido sin USB nunca despierta).
NINGÚN env es "solo telemetría". No existe un build "ISR/DMA en paralelo" aparte: la
optimización de hoy es el superloop rápido (fast-BNO + payload recortado → ~190k loops/s).

## Atribución
Banco + verificación: Gustavo Viollaz (humano con la placa). Feature del monitor + unificación
R1 + envs + esta entrada: Claude Opus 4.8 (Anthropic), 2026-06-16.
