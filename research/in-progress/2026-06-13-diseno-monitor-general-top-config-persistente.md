---
title: "Diseño — Monitor General de la TOP: config persistente de sensores (fail-safe P1) + visión completa (P2/2027)"
date: 2026-06-13
author: "Claude (Anthropic, Opus 4.8)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: in-progress — SPEC para revisar antes de codear el firmware
area: comunicacion
tipo: decision
robot: ambos
related-tasks: [TASK-205, TASK-206]
related: [2026-06-06-diseno-monitoreo-telemetria-usb-y-apps-pc.md, TELEMETRIA-TOP.md]
---

# Monitor del SISTEMA DE POSICIONAMIENTO (placa TOP) — diseño

> **Pedido (Gustavo, 2026-06-13):** un monitor "tipo el de la placa DOWN", INTUITIVO, del **sistema
> de posicionamiento**. La TOP es el HUB donde converge TODO antes de la CENTRAL: cámaras (pelota/
> arcos) + IMU (heading) + ToF (paredes/obstáculos) + ultrasonido + **OTOS y línea+vector de escape
> que la base (DOWN) difunde a la TOP** → se fusiona → **WorldSnapshot → CENTRAL**. El monitor tiene
> que (1) **VER cómo andan todos esos sensores** (incl. lo que llega de la base), (2) **ver qué se
> envía de la TOP a la CENTRAL**, y (3) **calibrar / setear parámetros** (y persistirlos). Permite
> además **deshabilitar sensores que mandan basura**.
>
> Decisión: **Fase A (fail-safe + persistencia + exponer todo lo que ya llega) ahora (P1 Incheon);
> Fase B (visualización rica: grillas 8×8, mapas) post-Incheon (P2/2027).**

## 0. Por qué fases

Casi nada de la lista existe hoy (ni firmware ni GUI), y es **70% firmware / 30% GUI**. La GUI
(`tools/monitor-base`) es **carril del otro agente** → este spec fija el **contrato** (comandos +
bloque de telemetría) para que la GUI se construya sin colisionar. A ~17 días de Incheon, lo que
**paga en cancha** es apagar un sensor que miente (lo vimos hoy: pelota fantasma de cámara dual,
velocidad ±13 m/s) y enmascarar zonas ToF; el resto (grillas 8×8, mapas) es diagnóstico → 2027.

| Pedido | Fase | Dónde vive |
|---|---|---|
| **Ver qué ve CADA cámara por separado** (front vs back): pelota / arco amarillo / arco azul + posición, no solo el fusionado | **A (P1)** | TOP fw (exponer `CamObs` front/back en telemetría) + GUI 2 paneles |
| Deshabilitar cámara F/B, BNO L/R, ultrasonido | **A (P1)** | TOP fw + EEPROM |
| ToF: deshabilitar sensor entero | **A (P1)** | TOP fw + EEPROM |
| ToF: anular zonas (ej. filas superiores), rotar 90°, invertir eje | **A (P1)** | TOP fw + EEPROM |
| Persistir TODA la config en EEPROM de la TOP, cargar al boot | **A (P1)** | TOP fw |
| Reportar el estado de config en telemetría (qué está on/off) | **A (P1)** | TOP fw |
| **OTOS (pose/vel) + línea + vector de escape** que la DOWN difunde a la TOP | **A (P1)** | TOP fw: los getters `comm_down_get_pose/velocity/line_status` **YA existen** → solo exponer en telemetría |
| **Lo que la TOP envía a la CENTRAL** (WorldSnapshot) | **A (ya está)** | bloque `snap` de la telemetría actual |
| Ver 8×8 zonas en grilla con color/barra, paneles front/der/atrás/izq | B (2027) | fw schema v2 + GUI |
| Mapa de distancias al eje + mapa de posición absoluta | B (2027) | GUI |
| Luz CRUDA (32 sensores de la base) | — | vive SOLO en DOWN (su monitor ya la muestra); a la TOP solo llega la **línea ya procesada** (LineStatusV2), no los 32 valores |

## 1. Fase A — `TopConfig` (la estructura persistente)

Módulo PURO nuevo `src/shared/top_config.{h,cpp}` (host-testeable). POD plano + (de)serialización a
bytes para EEPROM + helpers de aplicación. **Defaults = TODO habilitado, sin rotación, sin máscara, y
los `mount_bearing_deg` = el mapeo HARDCODEADO de hoy** (el de `CONVENCION-EJES`, ej. TOF2=der/
TOF3=izq) → con la config en defaults el comportamiento de competencia es **byte-idéntico** (la carga
al boot es no-op si nunca se guardó nada). Recién cuando el equipo reasigna y hace `CFG SAVE` cambia algo.

```c
constexpr uint8_t  TOP_CONFIG_MAGIC   = 0x7C;   // marcador "TopConfig" en EEPROM
constexpr uint8_t  TOP_CONFIG_VERSION = 1;
constexpr int      TOP_CFG_NUM_TOF    = 6;   // 4 hoy + 2 futuros (NUM_TOF_MAX)

struct TofZoneConfig {
    uint8_t  enabled;             // 1 = el sensor participa; 0 = NO_READING siempre
    int16_t  mount_bearing_deg;   // UBICACIÓN: hacia dónde mira en el robot. 0=frente,
                                  //   90=derecha, 180=atrás, 270=izquierda. Reemplaza el
                                  //   mapeo HARDCODEADO de hoy (TOF2=der/TOF3=izq). Acepta
                                  //   arbitrario (futuro 45°): es solo un ángulo para el radar
                                  //   y la dirección del obstáculo, no requiere resampleo.
    int16_t  zone_rotation_deg;   // ROTACIÓN del indexado de la grilla 8×8 (montaje del chip).
                                  //   Fase A SOLO 0/90/180/270; 45° = futuro (resampleo de grilla).
    uint8_t  flip;                // bit0 = invertir X, bit1 = invertir Y
    uint64_t zone_mask;           // 1 bit por zona; 1 = USAR, 0 = ANULAR (ej. filas superiores)
};

struct TopConfig {
    uint8_t  magic, version;
    uint8_t  cam_front_en, cam_back_en;     // 1 = se fusiona; 0 = se trata como cámara muerta
    uint8_t  bno_left_en,  bno_right_en;    // 1 = entra a la fusión de heading; 0 = excluido
    uint8_t  ultrasonic_en;                 // 1 = HC-SR04 reporta; 0 = NO_READING
    TofZoneConfig tof[TOP_CFG_NUM_TOF];
    uint16_t crc;                           // crc16 sobre los bytes anteriores
};
```

Funciones puras: `top_config_defaults(cfg)`, `top_config_serialize(cfg, buf, cap)`,
`top_config_deserialize(buf, len, cfg) -> bool` (valida magic/version/crc → false = usar defaults),
`top_config_apply_zone_mask(raw_zones[64], cfg.tof[i]) -> distancia_mm` (rota/invierte el indexado,
anula las zonas con bit 0, toma el mínimo de las que quedan). **Todo esto = tests host.**

## 2. Comandos host→TOP (contrato con la GUI — el otro agente la construye a esto)

Mismo estilo que DOWN (texto, una línea; ya hay `tt_parse_command`):

| Comando | Acción |
|---|---|
| `CAM F ON\|OFF` / `CAM B ON\|OFF` | habilita/deshabilita cámara frontal/trasera |
| `BNO L ON\|OFF` / `BNO R ON\|OFF` | habilita/deshabilita BNO izq/der (fusión heading) |
| `US ON\|OFF` | habilita/deshabilita el HC-SR04 |
| `TOF <n> ON\|OFF` | habilita/deshabilita el ToF n entero |
| `TOF <n> POS FRONT\|BACK\|RIGHT\|LEFT` | **UBICACIÓN**: asigna hacia dónde mira (frente/atrás/der/izq = 0/180/90/270°). Futuro: `TOF <n> POS <deg>` para ángulos arbitrarios (45°) |
| `TOF <n> ROT <0\|90\|180\|270>` | **ROTACIÓN** del indexado de la grilla 8×8 (montaje del chip). Futuro: 45° |
| `TOF <n> FLIP <X\|Y\|NONE>` | invierte un eje de la grilla del ToF n |
| `TOF <n> ZONE <ON\|OFF> <0..63>` | anula/activa una zona puntual |
| `TOF <n> ZONEMASK <hex16>` | setea la máscara completa de una (la GUI manda esto al pintar) |
| `CFG SAVE` | persiste `TopConfig` en EEPROM (ACK `[TOP] config guardada`) |
| `CFG LOAD` | recarga de EEPROM en vivo |
| `CFG RESET` | vuelve a defaults (no persiste hasta `CFG SAVE`) |

`IMU ZERO`/`IMU SAVE` (heading) ya existen y se mantienen.

## 3. Dónde se aplica cada flag (apply points)

- **Cámara F/B** → `cameras_runtime`/`cameras_fusion`: si `cam_*_en==0`, tratar esa cámara como
  watchdog-muerta (no entra a la fusión; `cam_*_ok` reporta el override). Mata la pelota fantasma. La
  telemetría per-cámara (§5a) muestra cuál de las dos miente → se apaga ESA con dato, no a ciegas.
- **BNO L/R** → `sensors_imu`: si deshabilitado, excluir de la fusión circular (como hoy con
  `TOP_BNO_PRIMARY_ONLY`, pero por config en vez de macro).
- **ToF sensor** → `sensors_tof`: si `enabled==0`, devolver `TOF_NO_READING`.
- **ToF ubicación (`mount_bearing_deg`)** → reemplaza el mapeo HARDCODEADO actual (`CONVENCION-EJES`:
  TOF2=der/TOF3=izq). La distancia del sensor n se asocia a su bearing → así la GUI llena los paneles
  front/der/atrás/izq con el sensor ASIGNADO, y la dirección del obstáculo en el WorldSnapshot
  (`min_obstacle` + futura zona angular) usa el bearing, no el índice físico. Arbitrario (45°) ya
  funciona acá sin resampleo.
- **ToF zonas/rotación/flip (`zone_rotation_deg`,`flip`,`zone_mask`)** → `sensors_tof`: requiere leer
  las **64 zonas** del VL53L7CX (hoy se reduce a 1 distancia internamente), aplicar rotación/flip al
  indexado y `top_config_apply_zone_mask` (anular zonas) ANTES de reducir al mínimo. Esta
  infraestructura per-zona es **compartida con Fase B** (no es trabajo tirado).
- **Ultrasonido** → `sensors_hcsr04`: si deshabilitado, `NO_READING`.

## 4. EEPROM de la TOP

- Región propia, sin pisar la calib del IMU (`sensors_imu_save_calibration`) ni otros usuarios de
  EEPROM del TOP. Reservar un offset fijo documentado (a confirmar leyendo quién más usa EEPROM en TOP).
- **Carga al boot UNGATED** (en `main_top` setup, antes de los sensores): si hay config válida la
  aplica; si no (magic/crc malos), defaults → **no-op = competencia byte-idéntica**.
- `CFG SAVE` persiste; ACK/NAK explícito (como DOWN aprendió en TASK-306).

## 5. Telemetría — bloque `cfg`

Dos agregados al frame TOP (aditivos; `TELEMETRY_TOP_SCHEMA` 1→2):

**(a) Detecciones POR CÁMARA** (`cam_front` / `cam_back`), no solo el fusionado. Cada una:
`ball {vis,x,y}`, `yellow {vis,x|ang,y|dist}`, `blue {vis,...}` — los `CamObs` que la fusión ya
consume hoy y tira. Con esto la GUI muestra **qué ve la frontal vs la trasera vs el fusionado** lado
a lado → un desacuerdo (la pelota fantasma: front dice una cosa, back otra) se ve de un vistazo, y el
equipo decide **cuál cámara apagar** (`CAM F|B OFF`) con dato, no a ciegas. Costo: chico (2 cámaras ×
pocos campos). El fusionado actual (`cam`) se mantiene.

**(b) Bloque `cfg`** con el estado actual (cam_en, bno_en, us_en, y por-ToF: `enabled`,
**`bearing`** (ubicación), `rotation`, `flip`, `zonemask`) para que la GUI muestre qué está on/off y
**dónde está asignado cada ToF** sin adivinar. El bloque de texto humano (`tt_format_human`) suma
líneas `F:`/`B:` (por cámara) y `CFG:` (ej. `ToF0=front rot90 ...`).

**(c) Bloque `base`** — lo que la TOP RECIBE de la DOWN (los getters `comm_down_get_*` ya existen):
OTOS `pose {x,y,heading}` + `vel {vx,vy,omega}`, y línea `{present, escape_angle, penetration_mm,
cross_track_mm, quality, frames_received, crc_errors}`. Así el monitor de posicionamiento ve también
la **odometría** y el **vector de escape de borde** que vienen de la base, no solo los sensores
propios de la TOP. (La luz CRUDA de 32 sensores NO llega a la TOP — eso es del monitor de DOWN.)
El WorldSnapshot que la TOP manda a la CENTRAL ya está en el bloque `snap` (sin cambios).

## 6. Plan de prueba (banco — el equipo cierra hardware)

1. `pio run -e top_robot2_pri` SUCCESS (ya tenemos toolchain).
2. Boot sin config guardada → todo habilitado, conducta de competencia idéntica (regresión).
3. `CAM B OFF` con la trasera mandando basura (la pelota fantasma de hoy) → la fusión deja de verla;
   `CFG SAVE`; power-cycle → sigue deshabilitada (persistencia).
4. `TOF 0 OFF` → ese ToF deja de reportar; `min_obst` lo ignora.
5. `TOF 0 ZONE OFF` en las filas superiores → la distancia de ese ToF deja de "ver" el techo/estructura.
6. `BNO R OFF` → heading usa solo el otro BNO (degrade con gracia).
7. Todo persiste tras power-cycle.

## 7. Riesgos / decisiones

- **Compromiso de formato EEPROM**: versionado (magic+version+crc); una config vieja se rechaza limpio.
- **Deshabilitar un sensor a mitad de partido degrada la conducta** — es un fail-safe DELIBERADO,
  documentar (no es un bug si el robot "ve menos": lo apagaste a propósito).
- **Byte-identidad de competencia**: la carga al boot es ungated → los defaults DEBEN ser no-op exacto.
- **Lectura per-zona del VL53L7CX**: hoy `sensors_tof` reduce a 1 distancia; exponer 64 zonas sube
  el costo del bus I²C/loop → medir que no rompa el loop a 100 Hz (el round-robin ya está).

## 8. Fase B (post-Incheon, 2027) — resumen

Telemetría schema v2 con las 64 zonas por ToF (~512 B/frame) · GUI con grilla 8×8 color/barra,
paneles front/der/atrás/izq · mapa de distancias al eje + mapa de posición absoluta · agregación de
OTOS/luz de DOWN en la misma app (monitor unificado). Todo sobre la infraestructura de Fase A.

## 9. Lo que NO hace este spec

No toca la GUI (otro agente). No expone OTOS/luz (son de DOWN). No cambia la conducta de competencia
(defaults no-op). No cierra nada en hardware (lo valida el equipo).
