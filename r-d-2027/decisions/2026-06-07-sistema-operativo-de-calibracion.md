---
title: "Decisión — Sistema operativo de calibración (persistencia descentralizada + config-as-code)"
date: 2026-06-07
status: propuesta R&D 2027 (no aplicada — POST-Incheon)
scope: r-d-2027
related: [r-d-2027/decisions/2026-06-07-esp32-telemetry-bridge.md, r-d-2027/decisions/2026-06-07-can-gateway-architecture.md, r-d-2027/roadmap.md, software/teensy/Soccer 2026/src/down/eeprom_calib.cpp, software/teensy/Soccer 2026/src/top/sensors_imu.cpp, software/teensy/Soccer 2026/src/shared/localization.h, software/teensy/Soccer 2026/src/top/localization_runtime.cpp, docs/firmware/TELEMETRIA-DOWN.md]
---

# Decisión — Sistema operativo de calibración

> R&D 2027. **No aplica al robot de Incheon 2026.** Es trabajo POST-Incheon.

## 1. Contexto

El robot 2026 tiene calibración repartida en muchos lugares: umbrales de línea
del DOWN, offsets del/los BNO del TOP, offsets de los ToF, espacio de color LAB
de las 2 cámaras OpenMV, y un puñado de parámetros tácticos (aceleración,
potencias, jugadas). Hoy cada cosa se calibra a mano por su propio camino y
**no hay un relato único de "qué calibración está operativa ahora mismo"** —
eso genera el clásico susto de banco de "no sé si el robot está corriendo la
calib buena o una vieja".

La idea original del coach fue: **una memoria central en la Teensy 4.1 (CENTRAL)
que al boot propague la calibración a las demás placas y cámaras, con fail-safe
si la propagación falla.** Es una intención correcta (un solo lugar para manejar
todo) pero el análisis de ingeniería concluye que **esa topología es frágil** y
hay que invertirla. Este documento explica por qué y propone la arquitectura
blindada.

### Por qué la "memoria central que propaga al boot" es frágil

- **Acopla el arranque de cada unidad a un enlace.** Si el bus CENTRAL→{TOP,DOWN,
  cámaras} tose en el boot (cable flojo, EMI de motores, una placa que arranca
  ~40 s después que otra — el TOP ya tarda eso por los BNO), las unidades quedan
  esperando una calib que no llega. Un robot que **no arranca su propia
  calibración sin pedir permiso** es un robot más frágil, no más robusto.
- **Punto único de fallo donde menos lo querés.** Si la EEPROM/SD de CENTRAL se
  corrompe, te quedás sin calibración de TODO el robot a la vez.
- **Reintroduce un wire-contract nuevo y delicado** (un protocolo de propagación
  de calib al boot) justo en el momento más sensible del ciclo de vida.
- **Las cámaras no encajan.** Las OpenMV N6 son computadoras con su propio
  almacenamiento; "propagarles" su LAB desde una Teensy por un UART de 11 B es
  pelear contra la herramienta en vez de usarla.
- **El robot YA tiene persistencia local que funciona** y está testeada:
  `eeprom_calib.cpp` (DOWN, con magic+versión+n_sensors+CRC y fallback a
  defaults) y `sensors_imu.cpp` (TOP, persiste offsets de cada BNO en EEPROM con
  magic+versión). Tirar eso para centralizar sería un retroceso.

**Conclusión: invertir la topología.** La persistencia es **descentralizada y
local**; el "un solo lugar" no es una RAM central propagada — es el **repo + una
PC tool**.

## 2. Decisión propuesta

Un "sistema operativo de calibración" con cuatro principios, ninguno de los
cuales es "memoria central propagada al boot":

1. **Almacenamiento DESCENTRALIZADO** — cada unidad persiste SU calibración
   local, con CRC + default fail-safe horneado.
2. **Fuente de verdad = CONFIG-AS-CODE en el repo** — valores óptimos como
   archivos versionados; una PC tool los empuja por USB a la persistencia de
   cada unidad. Git es el historial de "qué cambió".
3. **Mostrar-qué-cargó** — cada unidad anuncia al boot qué calib levantó
   (versión + origen + CRC-OK vs FAIL-SAFE) y la app de monitoreo lo lee.
4. **Se monta sobre el modo PRUEBA (bench mode)** — la infra de "conectar USB →
   modo prueba/calibración" con heartbeat fail-safe es el sustrato; este OS es
   la capa de gestión de parámetros encima.

```
   REPO (fuente de verdad, versionado)
   calibration/robot1/{down,top,cam_front,cam_rear}.json
   calibration/robot1/profiles/{lab,incheon}/...
        │
        │  PC tool (push por USB, una unidad por vez)  ── git = historial
        ▼
   ┌──────────┐   ┌──────────┐   ┌──────────────┐   ┌──────────────┐
   │  DOWN    │   │  TOP     │   │ cam_front N6 │   │ cam_rear  N6 │
   │ EEPROM   │   │ EEPROM   │   │ flash / SD   │   │ flash / SD   │
   │ +CRC     │   │ +CRC     │   │ +CRC         │   │ +CRC         │
   │ +default │   │ +default │   │ +default     │   │ +default     │
   └────┬─────┘   └────┬─────┘   └──────┬───────┘   └──────┬───────┘
        │ "v7 EEPROM   │ "v3 EEPROM     │ "LAB v5 SD       │ "LAB v5 SD
        │  CRC-OK"     │  CRC-OK"       │  CRC-OK"         │  CRC-OK"
        └──────────────┴────────────────┴──────────────────┘
                          ▼ (al boot, cada una sola)
                  app de monitoreo lee el banner de cada unidad
```

Cada unidad arranca **sola**, de su propia memoria, sin depender de nadie. Si su
dato no valida → arranca con su default seguro. CENTRAL deja de ser el dueño de
la calibración de todos; a lo sumo es **un consumidor más** del banner de cada
placa (vía el snapshot/telemetría que ya existe).

## 3. Almacenamiento descentralizado (pieza 1)

Cada unidad es dueña de su calibración. Patrón común para todas:
**`[magic | version | payload | CRC]` con default fail-safe horneado en código.**

| Unidad | Qué calibra | Dónde persiste | Estado hoy |
|---|---|---|---|
| **DOWN** | umbrales de línea (carpet/blanco/márgenes) por sensor del anillo | EEPROM, vía `ec_save`/`ec_load` (`eeprom_calib.cpp`) | **YA EXISTE.** Valida magic+versión+n_sensors+CRC; en false no pisa `calib[]` → cae a defaults. |
| **TOP — IMU** | offsets de cada BNO (yaw cero + sensor offsets del chip) | EEPROM (`sensors_imu.cpp`) | **YA EXISTE.** Magic+versión por chip; restaura al boot si existe, si no recalibra. |
| **TOP — ToF** | offset radial + bias/escala + (opc.) pitch + flag de validez **por sensor** | EEPROM del TOP (misma idea, región propia) | **NO existe el modelo por-sensor** — ver §7 (hoy `tof_offset_mm` es 1 valor compartido y PLACEHOLDER). |
| **cam_front N6** | umbrales LAB (pelota/arcos), exposición, homografía | flash interna / microSD de la cámara | persistido a mano por la OpenMV IDE; falta formalizar versión+CRC+default. |
| **cam_rear N6** | ídem | flash interna / microSD | ídem. |

Regla dura: **si el dato no valida (CRC malo, versión incompatible, vacío) →
arranca con el seguro y avísalo, sin depender de nadie.** El default horneado es
un set conservador que permite jugar (degradado) aunque la EEPROM esté en blanco.
Esto ya es exactamente lo que hace el DOWN; el OS lo estandariza para todas las
unidades.

## 4. Config-as-code: fuente de verdad en el repo (pieza 2)

El "un solo lugar para manejar todo" que pidió el coach **es real, pero vive en
el repo, no en una RAM central**:

- Los valores óptimos son **archivos VERSIONADOS**:
  `calibration/robot1/{down,top,cam_front,cam_rear}.json` (y `robot2/` para el
  segundo robot, que tiene HW distinto).
- **Perfiles por entorno**: `calibration/robot1/profiles/lab/` vs
  `profiles/incheon/` — porque la iluminación (LAB de cámaras) y la altura de
  pared de la cancha (ToF, ver §7) cambian entre el lab de Salta e Incheon.
- Una **PC tool** los empuja por **USB** a la persistencia local de cada placa
  y cámara (escribe la EEPROM/flash y le pone su CRC). Una unidad por vez,
  determinístico, verificable (lee de vuelta y compara CRC).
- **Git es el historial**: "qué cambió, cuándo y por qué". Un `git diff` sobre
  el JSON cuenta la historia de la puesta a punto. Eso es trazabilidad real, no
  un blob opaco en una RAM que nadie versiona.

Ventaja sobre la idea original: el "lugar único" es **PC tool + git**, no una
memoria central propagada. Se gana versionado, diff, rollback (`git checkout` de
un perfil viejo) y reproducibilidad — y se pierde el acoplamiento boot↔enlace.

## 5. Mostrar-qué-cargó: el anti-"no sé qué está operativo" (pieza 3)

**Es la feature clave de confiabilidad.** Cada placa, al boot, **anuncia qué
calibración cargó**:

```
DOWN:      calib v7  EEPROM  CRC-OK
TOP-IMU:   calib v3  EEPROM  CRC-OK
TOP-ToF:   FAIL-SAFE defaults   <-- CRC malo / EEPROM vacia
cam_front: LAB v5  SD  CRC-OK
cam_rear:  LAB v5  SD  CRC-OK
```

- El banner sale por la telemetría que **ya existe** (JSON Lines schema v1 del
  modo prueba; ver `docs/firmware/TELEMETRIA-DOWN.md`). No hace falta protocolo
  nuevo: se agrega un campo "calib" al frame de identidad de cada unidad.
- La **app de monitoreo** lo muestra en un panel: las 4 (o 5) unidades, su
  versión, su origen (EEPROM/SD), CRC-OK o FAIL-SAFE. De un vistazo se ve si el
  robot corre la calib buena o si algo cayó al seguro.
- Esto cierra el agujero que la "memoria central propagada" pretendía cerrar,
  pero **sin** el acoplamiento: la verdad de cada unidad la cuenta la unidad
  misma, no un intermediario.

## 6. Cámaras OpenMV N6 (pieza 4)

Las **OpenMV N6 son computadoras** con su propio USB, IDE, flash interna y
microSD. Su espacio de color nativo es **LAB** (no HSV) — toda la calibración de
color se piensa en LAB. Recomendaciones:

- **NO reconstruir el editor de umbrales.** La OpenMV IDE ya tiene un Threshold
  Editor LAB excelente (histogramas, sliders, vista en vivo). Reimplementarlo es
  trabajo tirado. El OS de calibración **usa** la IDE para encontrar umbrales,
  no la reemplaza.
- **Persistir por-cámara, con versión+CRC+default.** Cada N6 guarda su propio
  `cam_front.json` / `cam_rear.json` en flash/SD. Las dos cámaras de un robot
  son físicamente distintas y miran zonas distintas → **cada una su propio
  set**, no uno compartido.
- **Vista PC lado-a-lado para igualar las 2 cámaras de un robot.** Lo que más
  duele es que front y rear vean el mismo objeto con colores distintos. Una
  vista PC que muestre el blob detectado por ambas en simultáneo (mismo objeto,
  dos cámaras) permite **igualar** sus umbrales. Eso sí aporta valor y la IDE
  no lo da.
- **(Futuro) relay PC→TOP→cámara.** Más adelante, empujar el set LAB desde la PC
  a la cámara pasando por el TOP (reutilizando el enlace cámara↔TOP) para no
  tener que enchufar cada N6 por USB. Es comodidad, no es el camino crítico:
  el USB directo a la cámara siempre funciona como fallback.

## 7. ToF: calibración INDEPENDIENTE POR SENSOR (requisito verificado en código)

**Hallazgo en el código** (`src/shared/localization.h` + `localization_runtime.cpp`):

- Existe **azimut por sensor**: `tof_mount_angle_deg[4] = {0,180,270,90}` =
  {frente, atrás, derecha, izquierda}. Bien.
- Pero el **offset radial es UN valor compartido**: `tof_offset_mm` (un único
  `uint16_t` para los 4), y además está marcado **PLACEHOLDER** en
  `localization_runtime.cpp` (`g_config.tof_offset_mm = TOF_OFFSET_MM;`, comentado
  "Valor PLACEHOLDER en pinout_common.h; medir en HW").

Es decir, hoy **NO hay**, por-ToF:
- **inclinación / pitch** — un ToF puede apuntar unos grados arriba/abajo según
  el paralelismo del montaje; eso sesga la distancia proyectada.
- **bias + escala de distancia** — cada sensor tiene su offset y su ganancia.
- **settings por TIPO de sensor** — el **ToF IZQUIERDO es de OTRO fabricante**
  y mide distinto (rango, tiempo de integración, comportamiento ante pared).

**Conclusión a documentar — la calibración de ToF debe ser INDEPENDIENTE POR
SENSOR:**

- **offset radial por-ToF** (no un `tof_offset_mm` compartido → un array de 4).
- **bias + escala de distancia por-ToF** (`dist_corr = escala_i * dist_cruda +
  bias_i`).
- **(opcional) corrección de inclinación / pitch por-ToF** + un **flag de
  validez por-ToF** (si un sensor está degradado, marcarlo inválido y que la
  localización lo ignore, igual que `tof_valid[]` pero como propiedad calibrada,
  no sólo runtime).
- **soporte de parámetros por TIPO de sensor** (el izquierdo de otro fabricante
  lleva su propio perfil).
- **calibración por-venue**: la **altura de pared de la cancha** (Incheon)
  afecta lo que el ToF lee → el perfil ToF entra en el split `lab/` vs
  `incheon/` de §4.

Estructura propuesta (config-as-code, vive en `calibration/robot1/top.json`):

```json
"tof": [
  {"name":"frente",  "mount_angle_deg":0,   "type":"vl53l1x", "radial_mm":95, "bias_mm":0, "escala":1.00, "pitch_deg":0.0, "valid":true},
  {"name":"atras",   "mount_angle_deg":180, "type":"vl53l1x", "radial_mm":95, "bias_mm":0, "escala":1.00, "pitch_deg":0.0, "valid":true},
  {"name":"derecha", "mount_angle_deg":270, "type":"vl53l1x", "radial_mm":95, "bias_mm":0, "escala":1.00, "pitch_deg":0.0, "valid":true},
  {"name":"izquierda","mount_angle_deg":90, "type":"OTRO",    "radial_mm":95, "bias_mm":0, "escala":1.00, "pitch_deg":0.0, "valid":true}
]
```

El struct `LocalizationInputs`/config pasa de `uint16_t tof_offset_mm` (escalar)
a un array por-sensor; el fallback exacto es: si no hay perfil válido, usar el
`tof_offset_mm` único de hoy (comportamiento legacy, cero regresión).

## 8. Parámetros tácticos del cerebro (pieza 5) — más cuidado, no menos

Aceleración, potencias de motor, parámetros de jugadas: **NO son calibración de
sensores, son comportamiento de competencia.** Cambiarlos altera cómo juega el
robot, no cómo percibe. Por eso se tratan con **más** cuidado:

- **Exponer sólo los params seguros.** No todo parámetro del cerebro es editable
  por la tool. Una lista blanca explícita de "qué es tuneable" (p. ej. ganancias
  de PID dentro de rangos acotados); lo demás queda en código, fuera del OS de
  calib.
- **Fail-safe + mostrar-qué-cargó igual que los sensores.** Si el perfil táctico
  no valida → defaults seguros, y el banner lo dice ("BRAIN: FAIL-SAFE").
- **Rangos con clamp en firmware** (no confiar en que la PC mande algo sano —
  ver la lección del `output_clamp ≤327` del HeadingPID: un valor fuera de rango
  invierte el giro). El clamp vive en el firmware, no en la tool.
- **Versionado aparte** del de sensores: un cambio táctico es una decisión de
  juego y merece su propio diff/commit con razón.

## 9. Relación con el modo PRUEBA (bench mode) (pieza 6)

Este OS **se monta sobre** la infra de bench mode (subproyecto hermano
`mejoras/calibracion-bench-mode`): "conectar USB → entrar a modo prueba/
calibración" con **heartbeat fail-safe** (si se cae el USB/heartbeat, el robot
vuelve a estado seguro). El OS de calibración es la **capa de gestión de
parámetros** encima de ese sustrato:

- El bench mode da el canal seguro (USB + heartbeat + estado seguro).
- El OS de calibración da: persistencia local con CRC, push config-as-code desde
  el repo, y el banner mostrar-qué-cargó.
- La telemetría JSON Lines v1 del bench mode es el transporte del banner y de los
  comandos `CAL .../SAVE` (idempotentes, reversibles).

## 10. Consecuencias

### Ganamos
- Cada unidad arranca sola de su propia memoria → **sin punto único de fallo en
  el boot**, sin acoplar arranque a un enlace.
- Trazabilidad real (git diff de los JSON) y rollback de perfiles.
- El agujero "no sé qué calib corre" se cierra con el banner mostrar-qué-cargó.
- ToF por-sensor → localización correcta (hoy el offset compartido + PLACEHOLDER
  es deuda silenciosa).
- Reutiliza lo que YA funciona (DOWN EEPROM, TOP IMU EEPROM, OpenMV IDE,
  telemetría v1, bench mode).

### Sacrificamos
- Más archivos de config que mantener (un JSON por unidad × robot × perfil).
- La PC tool hay que construirla (push USB + verify CRC + panel de banners).
- Migrar el ToF de escalar a array toca `LocalizationInputs` y su runtime
  (con fallback legacy exacto para no regresar).

### Lo que NO cambia
- Con el OS sin tocar, el robot de hoy es byte-idéntico (todo esto es aditivo y
  POST-Incheon).
- Los contratos de wire y el comportamiento del robot en partido.

## 11. Alcance y qué NO hacer antes de Incheon

- **Alcance: es trabajo de r-d-2027, POST-Incheon.** Nada de esto se aplica al
  binario de competencia ahora. Aislamiento total (regla del README de
  `r-d-2027/`).
- **NO construir el OS completo antes de Incheon.** A ~23 días del mundial,
  ponerse a refactorizar persistencia y meter una PC tool nueva es **trampa de
  scope**: alto riesgo, distrae de la puesta a punto real, y no mejora el juego
  del robot en Incheon.
- **Qué SÍ es seguro hacer antes de Incheon (oportunista, NO obligatorio):**
  agregar el **banner mostrar-qué-cargó** al DOWN y al TOP-IMU (ya persisten con
  CRC; sólo falta que lo *anuncien* por la telemetría existente). Es aditivo,
  byte-seguro con el flag de telemetría, y da valor de banco inmediato. Si
  distrae aunque sea un poco → se pausa (manda la cadencia de Incheon).
- **Lo demás (config-as-code en el repo, PC tool de push, ToF por-sensor,
  perfiles lab/incheon, vista PC de cámaras, params tácticos) → después de
  Incheon.**

## 12. Quién decide y cuándo

- **Propuesta:** Claude (Anthropic) a pedido de Gustavo, 2026-06-07.
- **Aprobación de la documentación R&D (este archivo):** Gustavo, 2026-06-07.
- **Aprobación para empezar la PC tool / config-as-code:** pendiente —
  POST-Incheon.
- **Aprobación para migrar el ToF a por-sensor en el firmware real:** pendiente —
  decisión post-Incheon, con banco (medir radial/bias/escala/pitch de cada ToF).
