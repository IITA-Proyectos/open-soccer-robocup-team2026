---
name: bno055-imu-heading-robocup
description: Usar cuando el HEADING de un IMU "no anda" en firmware de robótica de competencia — clavado en 0.0, congelado, derivando, saltando, o el diag lee bien pero el firmware no — o al configurar/calibrar/recuperar/diagnosticar uno o dos BNO055 sobre I2C (Teensy/Arduino) con EMI de motores. Cubre modos (IMUPLUS vs NDOF), calibración y guardar/restaurar offsets (el chip NO tiene EEPROM), árbol de diagnóstico, recuperación de fallas y prácticas de competencia. Triggers - "heading no anda / clavado en 0.0", "el IMU se congela / freeze / yaw congelado", "el rumbo deriva / drift", "el heading salta", "el diag anda pero el firmware no", "calibrar BNO055", "guardar/restaurar calibración del IMU", "BNO055", "IMU", "Euler / quaternion", "IMUPLUS / NDOF", "CALIB_STAT / SYS_STATUS / clock stretching", "dos BNO en el mismo bus / 0x28 / 0x29", "EMI de motores / magnetómetro", "Teensy + IMU". NO es para tunear el lazo de heading-hold (control-pid-zona-muerta) ni para fusionar la pose XY (fusion-pose-odometria-landmarks).
---

# BNO055 / IMU — heading confiable para robótica de competencia

## Principio central — "el diag anda, el firmware no"

El BNO055 fusiona accel+gyro(+mag) **on-chip** (un Cortex-M0 corre la BSX FusionLib a 100 Hz)
y entrega Euler/quaternion listos. **El silicio rara vez es el problema.** La frase ancla:

> **Si un programa diag de lectura DIRECTA (sin tu fusión, sin tus flags de config) lee el
> heading bien y el firmware de competencia da 0.0/congelado, el chip está SANO — el bug
> vive en el ENTORNO DE SOFTWARE (config + fusión + gating de salud) o en la PLACA
> (alimentación/EMI/masa), NO en el sensor.**

Modelo mental: separá SIEMPRE tres capas — **(1) silicio/I2C, (2) software
(config/fusión/escala), (3) placa (alimentación/EMI/masa)**. Correr el diag PRIMERO manda la
culpa a la capa correcta antes de tocar sensor, cable o fuente: es la bifurcación diagnóstica
más rentable.

Dos reglas duras:
- **"Presente en el bus" ≠ "dato válido".** Que ackee 0x28 y `CHIP_ID=0xA0` es NECESARIO
  pero NO suficiente: un BNO puede ackear y devolver yaw clavado.
- **`begin() OK` / compila / "el diag lee" NO prueban que el heading llegue al lazo.**
  Confirmá el EFECTO: el heading cambia con el sentido correcto **al rotar el robot, en el
  consumidor final**.

⚠️ **Este robot vivió DOS fallas de heading con causas raíz OPUESTAS** (una software, una
hardware — ver [casos reales](references/bno055-casos-reales-robot.md)). Por eso el árbol de
abajo **deriva** la causa, no la asume. "0.0 PERFECTO y constante" huele a software/flag;
"congelado intermitente en el último valor" huele a eléctrico/contención. No las fusiones.

## Cuándo usar / cuándo NO

USAR: heading clavado/congelado/derivando/saltando en el firmware; configurar/calibrar/
recuperar uno o dos BNO055; elegir modo de fusión; diseñar la arquitectura de buses I2C con
BNO + otros sensores; el diag lee pero el firmware no.

NO usar (rutear):
- Tunear el lazo de heading-hold (PFM/deadband/anti-windup) → `control-pid-zona-muerta` +
  planta en `dinamica-omni-3-ruedas`.
- Fusionar la POSE XY (OTOS + ToF + heading) → `fusion-pose-odometria-landmarks`; elegir la
  técnica de localización → `localizacion-rcj-soccer`.
- Timing del lazo en sí (I/O bloqueante, jitter, WCET) → `tiempo-real-determinismo`.

Esta skill termina en "tenés un heading confiable"; lo que hagas con él es de las otras.

## Fundamentos + modos (lo que MANDA es el datasheet/código, no la memoria)

El BNO055 NO tiene EEPROM interna → al power-on **siempre arranca des-calibrado** (reaparece
en Calibración). Mapa de registros completo + escalas + códigos de estado:
[references/bno055-registros-y-escalas.md](references/bno055-registros-y-escalas.md).

| Modo (OPR_MODE 0x3D) | Fusiona | Heading | Cuándo |
|---|---|---|---|
| CONFIG `0x00` | nada | — | ÚNICO modo donde se escriben offsets/axis-remap/UNIT_SEL; default al boot |
| AMG/ACCGYRO… `0x01–0x07` | crudo, sin fusión | — | diag/raw |
| **IMUPLUS `0x08`** | **accel+gyro (SIN mag)** | **RELATIVO al power-on, drift lento** | **robótica con motores — la apuesta de soccer** |
| COMPASS/M4G `0x09/0x0A` | accel+mag | absoluto, lento | raro en robot que acelera |
| NDOF_FMC_OFF/NDOF `0x0B/0x0C` | accel+gyro+mag | ABSOLUTO al norte | SOLO entorno magnético limpio |

- **Regla de modo:** motores/imanes/estructura ferrosa cerca → **IMUPLUS** y zero-ar por
  software. NDOF cerca de motores es la trampa #1: el mag se ensucia, el heading salta o se
  traba esperando "el norte" (`SYS=0`).
- **Cambio de modo (no negociable):** toda transición pasa OBLIGATORIAMENTE por CONFIG
  (ej. NDOF→CONFIG→IMUPLUS), con delays del datasheet (~7 ms CONFIG→operación, ~19 ms
  operación→CONFIG; Adafruit usa `delay(30)`). **Cambiar config en modo activo se ignora en
  silencio.** No leas Euler hasta que pasó el delay Y `SYS_STATUS=5` (fusión corriendo).
- **Escala (bug clásico):** Euler 1°=16 LSB (`heading_deg = raw/16.0`); gyro 16 LSB/dps.
  Olvidar el /16 o confundir grados/radianes (UNIT_SEL bit2) → heading 16× más chico, "parece
  casi cero" — bug de escala disfrazado de sensor.
- **Euler vs quaternion:** el firmware del BNO tiene errores conocidos de Euler (distorsión/
  saltos con pitch/roll > ~20°). En robot casi-plano da igual; si ves saltos al inclinarse,
  leé QUATERNION (0x20) y convertí — no es freeze.
- **Reloj:** cristal externo 32.768 kHz da mejor fusión; se habilita con `SYS_TRIGGER=0x80`
  SOLO en CONFIG, tras el reset y ANTES del modo de fusión. Pedir EXT_CRYSTAL **sin cristal
  físico** → el chip se queda sin reloj → falla silenciosa que PARECE freeze (pero NO clava en
  0.0 con un diag que lee bien — pista falsa, ver árbol).

## Arquitectura I2C y montaje

- **Direcciones:** 0x28 (COM3/ADR a GND, default) / 0x29 (a VDD). **NO** dos BNO con la MISMA
  dirección en el MISMO bus → buses separados o 0x28/0x29.
- **Clock stretching:** el BNO055 estira el SCL agresivo (throughput real ~44 kbps). El bus
  DEBE soportarlo (Teensy Wire HW sí; RPi por HW lo rompe). En bus compartido con ToF, subí
  pullups externos (~4.7k; algunos usan 2.2k SDA / 4.7k SCL). **Aislar el sensor crítico en su
  propio bus es la mejor práctica, no un parche.**
- **Este robot (ground-truth `src/top/sensors_imu.cpp:60-66`):** 2 BNO @0x28 en buses
  SEPARADOS — PRIMARIO (idx 0) en **Wire2 (24/25), SOLO sin ToF** → sin contención; SECUNDARIO
  (idx 1) en **Wire (18/19), comparte con 4 ToF**. El primario va en idx0 a propósito (la
  fusión prioriza idx0 + failover al present). Wire a 100 kHz (a 400 kHz con ToF rangeando el
  read multi-byte del BNO se corrompe). NUNCA hubo BNO en 0x29.
- **Montaje:** lo más cerca del CENTRO DE ROTACIÓN (descentrado → aceleración centrípeta en
  giros ensucia el accel); amortiguá vibración (grommets/standoffs blandos) — la vibración
  rectifica a **bias de gyro (VRE) que NO se filtra después**, se previene mecánicamente.
- **Convención de signo:** definí UNA (este firmware: `HEADING_SIGN=-1`, el chip da yaw CW+,
  la cancha usa CCW+; se invierte en la fuente, `sensors_imu.cpp:145`). Un signo invertido hace
  que el control "corrija al revés" y el robot se vaya girando. **Verificá a mano que el
  heading SUBE en el sentido que definiste positivo ANTES de cerrar el lazo.**

## Calibración + guardar/restaurar offsets (el chip NO tiene EEPROM)

`CALIB_STAT (0x35)` empaqueta 4 valores 0..3: `SYS=(reg>>6)&3, GYRO=(reg>>4)&3,
ACCEL=(reg>>2)&3, MAG=reg&3`. GYRO calibra con el robot QUIETO unos segundos; ACCEL con 6
poses estables; MAG con movimiento normal (Fast Mag Calib, ya no hace falta el figure-8).

⚠️ **Gotcha IMUPLUS (clave soccer):** sin magnetómetro, **MAG se queda en 0 PARA SIEMPRE y es
CORRECTO** — no es sensor fallado; SYS también puede no llegar a 3. En IMUPLUS solo importan
**GYRO=3** (heading) y ACCEL=3 (tilt). **NUNCA bloquees el arranque esperando MAG=3 o SYS=3 en
IMUPLUS: cuelga el boot para siempre.**

**Persistencia (porque arranca des-calibrado SIEMPRE):** leé los 22 bytes de offsets una vez
calibrado, guardalos en la EEPROM/flash del MCU host, y reescribilos al boot **EN CONFIG
MODE** (registros 0x55–0x6A; la escritura efectiva ocurre al escribir el MSB del par). Flujo
copy-paste + el layout EEPROM real del robot:
[references/bno055-bringup-y-offsets.md](references/bno055-bringup-y-offsets.md).

⚠️ **Distinguí las DOS EEPROMs** (fuente de confusión): la del CHIP (no existe, hay que
emularla en el MCU) vs la del MCU host (Teensy), donde viven TANTO los offsets de calib COMO
**los flags de config tipo `bno_left_en`** — la trampa del flag (Fase 3c) vive en la segunda.

## Árbol de diagnóstico — "heading que no anda" (el corazón)

Orden barato→caro. **Deriva** la causa, no la asume. Cada fase termina en una verificación.

- **FASE 0 — ¿Qué síntoma exacto?** `0.0` PERFECTO y constante (huele a "variable default de
  sensor descartado"); congelado en el ÚLTIMO valor / ruido (chip vivo-pero-clavado o I2C
  corrupto); deriva lenta (calib/offsets/drift normal); salta (glitch, EMI del mag en NDOF, o
  Euler con pitch alto). **El 0.0 exacto y el último-valor-clavado apuntan a causas DISTINTAS.**

- **FASE 1 — ¿El silicio vive?** Corré el DIAG de lectura directa (sin tu fusión/flags): leé
  `CHIP_ID(0x00)==0xA0`, `ST_RESULT(0x36)==0x0F`, `SYS_STATUS(0x39)==5` (si =1, leé SIEMPRE
  `SYS_ERR 0x3A`), y `EULER 0x1A` crudo. **Verificación: girá el robot → ¿el Euler crudo
  CAMBIA?** Si `CHIP_ID=0xA0` y el Euler crudo sigue el giro → silicio SANO, **bifurcá a Fase
  3**. Si no cambia o NAKea → hardware/bus, **andá a Fase 2**. (Oráculos del robot:
  `diag_bno_dual_live`, `diag_bno_freeze_probe`.)

- **FASE 2 — Hardware/bus (SOLO si el diag TAMBIÉN falla):** clock stretching (bajá a 100 kHz,
  bus aislado); pullups; alimentación (un brownout tira TODO, incluido el diag — si el diag
  anda con la misma fuente, NO es alimentación); acople ELÉCTRICO en la placa. ⚠️ **Caso real
  R1:** el rangeo de los ToF (VCSEL) congelaba la fusión del BNO por acople eléctrico LOCAL en
  la placa — NO el bus (aislado por scan), NO la batería, NO el cristal/frecuencia/clock.
  **Verificación:** bisección por sensor (`-DTOF_ONLY_INDEX=N`), scan I2C dual-bus, osciloscopio
  en 3V3/GND del BNO sincronizado con el rangeo.

- **FASE 3 — Software (el diag ANDA pero el firmware NO).** Instrumentá el camino COMPLETO, una
  etapa por línea, cada ciclo: **(1)** Euler crudo de CADA chip; **(2)** heading por-sensor tras
  remap/offset/signo; **(3)** qué sensores ve VIVOS la fusión + pesos/salud; **(4)** fused final.
  El punto donde el número se vuelve 0.0/constante/NaN = la etapa rota. (En el robot:
  `TOP_DBG_BNO`, imprime `RAW_eul / off / in0 / fused / pres / calg`.)
  - **3a** — ¿`OPR_MODE` quedó en CONFIG/raw en vez de fusión? (no pasó por CONFIG+delay).
  - **3b** — ¿ESCALA? (¿dividiste por 16? ¿grados vs radianes?).
  - **3c — LA TRAMPA DEL FLAG (causa raíz del caso ancla):** un flag de habilitación por-sensor
    persistido en la EEPROM del MCU (`bno_left_en`) puede dejar un chip SANO fuera de la fusión.
    Mecanismo: `enabled=false` → la fusión lo marca DEAD, peso 0 (`imu_fusion.cpp:102`) →
    `fused_heading=0.0` SIEMPRE. En el robot el EEPROM tenía `bno_left_en=0` (alguien corrió
    `BNO_L_OFF` en una sesión vieja creyendo que el chip fallaba). **Verificación: al boot,
    LOGUEÁ el valor REAL de TODOS los flags de habilitación y de qué sensores usa la fusión.**
    Un sensor "apagado en una sesión vieja" es indistinguible de uno "fallado" si no se loguea
    el motivo. NOTA: `bno_left_en` es firmware-specific, NO está en el datasheet Bosch.

- **FASE 4 — Hipótesis, UNA variable por vez** (bisección por flags + leer estados intermedios).
  No teorices: **descartá con DATOS.**

- **FASE 5 — Gate de verificación (antes de cantar victoria):** rotá el robot y confirmá que el
  fused_heading **que llega al CONSUMIDOR FINAL** cambia con el sentido correcto. Self-test
  verde / `begin()` OK / "el diag lee" / compila NO lo prueban.

## Recuperación de fallas (en capas)

Distinguí primero DOS modos: **(A)** sensor descartado por config/fusión (raw VIVO pero fused=0
→ no es recuperación, es FIX de config, Fase 3c); **(B)** freeze REAL (raw CONGELADO mientras el
robot gira).

**Detectar freeze REAL por software:** el heading no cambia **al LSB** durante N muestras
mientras el robot se mueve. Clave: igualdad **EXACTA** — un BNO vivo jitterea ≥1 LSB aun quieto;
un valor clavado al centideg exacto muchos ticks = el (0,0,0) de `getVector()` ante fallo I2C.
**Guarda de gyro:** solo declarar congelado si ADEMÁS el gyro probó que el robot GIRABA (evita
el falso-DEAD del robot quieto). Reusá el heading ya leído → cero I2C extra. (Robot:
`imu_freeze.h`, N=40/T=1500 ms, `imu_freeze_update_g`.)

**Escalera (menos→más invasiva):** (1) watchdog de stale → `present=false` → la fusión hace
failover; (2) **fallback a gyro crudo** (`GYR_DATA 0x14`, integrar yaw a mano — sobrevive a un
freeze de la FUSIÓN); (3) soft-resync de drift (re-cero del offset, SIN `begin()` en el loop);
(4) re-init condicional (`begin()`: poll CHIP_ID + set modo); (5) soft-reset `SYS_TRIGGER=0x20`
→ esperar reaparición de CHIP_ID; (6) último recurso: power-cycle por hardware / pin RESET.

⚠️ **Advertencia soft-reset (no negociable):** `SYS_TRIGGER=0x20` NO es seguro en todos los
lotes — confirmado que deja el chip "desaparecido del I2C" hasta power-cycle (dotnet/iot #777),
y el reset por línea RST falla sin reloj de 32 kHz. **NUNCA lo metas en un watchdog de
producción sin fallback de power-cycle/RST por GPIO y sin validarlo en banco con TU lote.**
Diseñá el PCB con el pin RESET cableado al MCU.

**Bug de firmware a conocer:** yaw congelado en rotación lenta CONSTANTE (~1–2 dps) — la fusión
deja de actualizar el yaw, vuelve al cambiar de velocidad. Para correcciones ultra-finas no
dependas del Euler; el gyro crudo no sufre este bug.

## Mejores prácticas de competencia (RoboCup Soccer)

- **Modo: IMUPLUS, siempre, con motores cerca.**
- **Patrón ganador "relativo confiable + absoluto intermitente":** el heading relativo del IMU
  manda el lazo rápido (estable, baja latencia, sin saltos); cuando tenés una observación
  ABSOLUTA confiable (bearing al arco por visión, trilateración ToF, landmark), corregís con un
  filtro complementario de **ganancia BAJA** (`heading += k·(abs−rel)`, k chico) gateado por
  frescura y rechazo de outliers. **NUNCA inyectes la corrección absoluta directa** (un falso
  positivo de visión = giro brusco).
- **Cero al boot** apuntando a una referencia conocida + re-zero manual por botón siempre
  disponible. El robot debe estar QUIETO al inicializar (la calib del gyro asume reposo).
- **Todo heading de gyro DERIVA:** medí TU drift en banco (quieto X min, graficá) antes de
  asumir números; auto-recalibrá el bias del gyro cuando el robot está quieto varios segundos.
- **Frecuencia:** la fusión interna es 100 Hz fijo — no leas más rápido (re-leés el mismo dato).
  Leé el IMU al PRINCIPIO del ciclo, burst-read de los 6 bytes de Euler de una.

## Errores comunes

| Síntoma | Causa raíz real | Trampa (lo que parece) | Fix + verificación |
|---|---|---|---|
| heading=0.0 PERFECTO y constante | flag de config (`bno_left_en=0`) en EEPROM del MCU → fusión lo marca DEAD | "chip muerto / freeze" | loguear flags al boot, forzar enabled del primario; **rotar el robot** |
| congelado en último valor, INTERMITENTE | acople eléctrico del rangeo ToF (VCSEL) en la placa | "bus / cristal / alimentación" | bisección por-ToF + scan dual-bus; revisar masa/3V3/EMI |
| MAG nunca llega a 3 en IMUPLUS | IMUPLUS no usa mag, es correcto | "mag fallado" | esperar solo GYRO/ACCEL=3; NO bloquear el boot |
| heading 16× más chico ("casi cero") | no dividir Euler por 16 / grados↔radianes | "sensor débil" | dividir /16, chequear UNIT_SEL bit2 |
| "configuré IMUPLUS pero usa mag" | cambio de config fuera de CONFIG mode (se ignora) | "el chip no obedece" | ir a CONFIG, escribir, volver, delays; leer OPR_MODE |
| ceros/basura justo tras cambiar de modo | leíste antes del delay / `SYS_STATUS≠5` | "congelado" | esperar delay + `SYS_STATUS=5` |
| drift fuerte / "a veces anda" tras boot | no restauraste offsets (el chip no tiene EEPROM) | "chip de baja calidad" | guardar/restaurar 22 bytes en CONFIG; verificar header EEPROM |
| uno/ambos BNO "no responden" | 2 BNO @0x28 en el mismo bus | "chip quemado" | buses separados o 0x28/0x29 |
| chip "desaparecido del I2C" tras recuperar | soft-reset `0x20` en lote sensible | "se rompió solo" | NO soft-reset sin fallback de power-cycle/RST |
| robot gira acelerando el error tras cerrar el lazo | signo de yaw invertido (axis-remap/sign) | "PID mal tuneado" | verificar a mano que el heading sube en + ANTES de cerrar el lazo |

**Anti-racionalizaciones:** "es EMI de los motores" → la EMI mata el MAGNETÓMETRO; accel+gyro
(IMUPLUS) son mucho menos sensibles (un gyro bias mal estimado PARECE EMI). "ackea, está sano"
→ ackear ≠ fusión viva. "el diag SUCCESS prueba que anda" → el diag valida el CHIP, no tu
pipeline de config/fusión. "es corrupción de EEPROM, improbable si el I2C anda" → la corrupción
del flag es de la EEPROM del MCU, no del bus del sensor. "3 intentos fallidos = arquitectura
mal" → primero descartá config/escala/flags con el árbol (`superpowers:systematic-debugging`).

## Skills relacionadas

- Tunear el lazo de heading-hold → `control-pid-zona-muerta` + planta `dinamica-omni-3-ruedas`.
- Fusionar la POSE XY → `fusion-pose-odometria-landmarks`; elegir técnica → `localizacion-rcj-soccer`.
- Timing del lazo (I/O bloqueante/jitter/WCET — raíz del freeze por contención) →
  `tiempo-real-determinismo`; control discreto en tiempo real → `control-embebido-tiempo-real`.
- Estado seguro / watchdog / redundancia cuando el heading ES seguridad →
  `sistemas-criticos-tolerancia-fallas`.
- Método de debug genérico → `superpowers:systematic-debugging`; verificar antes de cerrar →
  `superpowers:verification-before-completion`. Test en hardware real → `hardware-test-protocol`;
  documentar → `engineering-journal`.

## Referencias (no inflar el inline)

- [references/bno055-registros-y-escalas.md](references/bno055-registros-y-escalas.md) — mapa
  de registros (0x00–0x6A), tabla de los 13 OPR_MODE, UNIT_SEL/escalas, TODOS los códigos de
  ST_RESULT/SYS_STATUS/SYS_ERR, axis-remap (presets P0–P7).
- [references/bno055-bringup-y-offsets.md](references/bno055-bringup-y-offsets.md) — secuencia
  de bringup paso a paso, flujo guardar/restaurar offsets (22 bytes, EXIGE CONFIG), calibración
  por sensor, layout EEPROM real del robot.
- [references/bno055-casos-reales-robot.md](references/bno055-casos-reales-robot.md) — los DOS
  casos ancla (software `bno_left_en` vs hardware acople-ToF) con sus tablas de prueba y
  punteros a los archivos reales.
