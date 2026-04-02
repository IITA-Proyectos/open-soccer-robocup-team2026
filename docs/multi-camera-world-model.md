# Visión Multi-Cámara y World Model para RoboCupJunior Soccer

## Cómo construir un modelo del mundo completo usando múltiples cámaras + comunicación de equipo

**Autor:** Gustavo Viollaz (IITA Salta) con asistencia de Claude Opus 4.6  
**Fecha:** 2026-03-28

---

## 1. EL PROBLEMA: UNA CÁMARA NO ALCANZA

Una sola OpenMV H7 con FOV de ~47° horizontal solo ve un cono estrecho del campo. En un campo de 182×243cm, el robot necesita girar para buscar la pelota, perdiendo tiempo y desorientándose.

### Qué necesita saber el robot en todo momento

| Entidad | Cantidad | Qué necesito | Frecuencia |
|---------|:--:|---|:--:|
| Mi posición | 1 | (x, y, heading) | 100 Hz |
| Pelota | 1 | (x, y, vx, vy, confianza) | 30-100 Hz |
| Mi compañero | 1 | (x, y, heading, estado) | 10 Hz (WiFi) |
| Arco propio | 1 | (ángulo, distancia) | 10 Hz |
| Arco rival | 1 | (ángulo, distancia) | 10 Hz |
| Rivales | 2 | (x, y) aproximado | 10 Hz |

### Qué ven los equipos campeones

El equipo PCBWay (3° mundial Bangkok 2022, High Rank Eindhoven 2024) usa **4 cámaras** en cada robot para lograr visión 360°. Cada cámara tiene su PCB dedicada que procesa la imagen y envía datos al Teensy 4.1 central. Esto les permite ver pelota, arcos y rivales simultáneamente sin girar.

En la liga MSL (adultos), CAMBADA y otros usan espejo catadioptric para visión omnidireccional 360° con una sola cámara. Esto no es práctico para Junior por tamaño y complejidad óptica.

---

## 2. CONFIGURACIONES DE CÁMARAS

### Opción A: 1 cámara frontal (la más simple)

```
FOV: ~47° horizontal
Cobertura: 13% del espacio
Pros: Simple, barato, código directo
Contras: Tiene que girar para buscar. Pierde pelota al girar.
         No puede ver arco rival y pelota al mismo tiempo si están
         en ángulos distintos.
```

### Opción B: 2 cámaras (frente + atrás)

```
         [CAM_F 47°]
            ↑
    ←───[ROBOT]───→
            ↓
         [CAM_B 47°]

FOV total: ~94° (26% cobertura)
Pros: Ve pelota adelante y detecta peligro atrás.
      El arquero puede ver la pelota (frente) y su arco (atrás).
Contras: Zonas ciegas laterales.
```

### Opción C: 4 cámaras (90° cada una = 360°) ← PCBWay

```
         [CAM_0 frente]
            ↑
  [CAM_3]←[ROBOT]→[CAM_1]
            ↓
         [CAM_2 atrás]

FOV total: ~190° efectivo (4×47° con overlap)
Pros: Visión casi 360°. Nunca pierde la pelota por girar.
      Ve pelota, arcos y rivales simultáneamente.
Contras: 4 cámaras = 4 UARTs, más peso, más consumo, más complejo.
         Ambigüedades cuando 2 cámaras ven la misma pelota.
```

### Opción D: 1 cámara con espejo cónico/parabólico

```
Espejo parabólico arriba del robot, cámara mirando hacia arriba.
FOV: 360° pero distorsionado y baja resolución.
Pros: 360° con una sola cámara.
Contras: Resolución muy baja en la periferia. Difícil calibrar.
         El espejo tiene que ser fabricado por los estudiantes (reglas 2026).
```

### Recomendación para IITA 2026

**2 cámaras** (frente + atrás) es el balance óptimo para 2026. Suficiente para cubrir pelota+arco sin la complejidad de 4 cámaras. Para 2027, evaluar pasar a 4.

---

## 3. WORLD MODEL: REPRESENTACIÓN UNIFICADA DEL MUNDO

Independientemente de cuántas cámaras haya, el robot mantiene UN solo modelo del mundo que integra TODAS las fuentes.

### Estructura de datos

```cpp
// ============================================================
// WorldModel — Estado completo del mundo visto por el robot
// ============================================================

struct EntityState {
    float x, y;                // Posición en campo (mm)
    float vx, vy;              // Velocidad (mm/s)
    float heading;             // Orientación (grados), solo para robots
    float confidence;          // 0-100
    uint32_t last_seen_ms;     // Timestamp última detección
    uint8_t source;            // Quién lo vio: MY_CAM0, MY_CAM1, PARTNER_WIFI, KALMAN_PREDICT
};

enum EntitySource {
    SRC_MY_CAM_FRONT  = 0x01,
    SRC_MY_CAM_BACK   = 0x02,
    SRC_MY_CAM_LEFT   = 0x04,
    SRC_MY_CAM_RIGHT  = 0x08,
    SRC_PARTNER_WIFI  = 0x10,
    SRC_TOF_ARRAY     = 0x20,
    SRC_ODOMETRY      = 0x40,
    SRC_KALMAN_PRED   = 0x80
};

struct WorldModel {
    // Mi estado
    EntityState me;            // Mi posición (odometría + ToF + gyro)

    // Pelota
    EntityState ball;          // Posición fusionada de todas las fuentes
    bool ball_visible;         // ¿Alguien la ve ahora?

    // Mi compañero
    EntityState partner;       // Posición recibida por WiFi

    // Arcos
    EntityState goal_own;      // Nuestro arco
    EntityState goal_opp;      // Arco rival

    // Rivales (hasta 2)
    EntityState rivals[2];
    int num_rivals_seen;

    // Metadata
    uint32_t tick;             // Número de ciclo
    uint32_t frame_time_us;    // Duración del último ciclo
};

WorldModel world;
```

---

## 4. FUSIÓN MULTI-CÁMARA: RESOLVER AMBIGÜEDADES

### Problema: 2 cámaras ven la misma pelota

Si la pelota está en la zona de overlap entre cámara frontal y lateral, ambas la reportan. Sin fusión, el robot "ve" 2 pelotas fantasma.

### Solución: fusionar por proximidad en coordenadas del campo

```cpp
void fuse_camera_detections(Detection* cam_dets[], int n_cams[],
                            Detection* fused, int& n_fused) {
    // 1. Convertir TODAS las detecciones a coordenadas del campo
    Detection all[16];
    int total = 0;
    for (int c = 0; c < NUM_CAMS; c++) {
        for (int d = 0; d < n_cams[c]; d++) {
            Detection& det = cam_dets[c][d];
            // Convertir (angle_cam, dist_cam) a (field_x, field_y)
            float cam_heading = world.me.heading + cam_offsets[c];
            float global_angle = cam_heading + det.angle;
            all[total].x = world.me.x + det.distance * sinf(global_angle * DEG2RAD);
            all[total].y = world.me.y + det.distance * cosf(global_angle * DEG2RAD);
            all[total].confidence = det.confidence;
            all[total].source = (1 << c);
            all[total].color = det.color;
            total++;
        }
    }

    // 2. Agrupar detecciones cercanas del mismo color
    bool merged[16] = {false};
    n_fused = 0;
    for (int i = 0; i < total; i++) {
        if (merged[i]) continue;
        Detection avg = all[i];
        float weight_sum = avg.confidence;
        int count = 1;

        for (int j = i + 1; j < total; j++) {
            if (merged[j]) continue;
            if (all[j].color != all[i].color) continue;

            float dist = sqrtf(sq(all[i].x - all[j].x) + sq(all[i].y - all[j].y));
            if (dist < 150) {  // <150mm = misma entidad
                avg.x = (avg.x * weight_sum + all[j].x * all[j].confidence) /
                        (weight_sum + all[j].confidence);
                avg.y = (avg.y * weight_sum + all[j].y * all[j].confidence) /
                        (weight_sum + all[j].confidence);
                weight_sum += all[j].confidence;
                avg.source |= all[j].source;  // Bitmask: ambas cámaras
                avg.confidence = min(100.0f, weight_sum / count);
                merged[j] = true;
                count++;
            }
        }
        fused[n_fused++] = avg;
    }
}
```

### Zona de overlap entre cámaras

```
Con 2 cámaras (frente+atrás) a FOV 47° cada una:
  Frente: -23.5° a +23.5°
  Atrás: 156.5° a 203.5° (ó -203.5° a -156.5°)
  Overlap: CERO (no hay overlap con 2 cámaras a 180°)
  Zona ciega: 66.5° a cada lado

Con 4 cámaras a 90° cada una:
  Cam 0 (frente):  -23.5° a +23.5°
  Cam 1 (derecha): 66.5° a 113.5°
  Cam 2 (atrás):   156.5° a 203.5°
  Cam 3 (izquierda): 246.5° a 293.5°
  Overlap: ~0° (4×47° = 188° < 360°, hay gaps de ~43° entre cada par)
  Zona ciega: ~43° entre cada par de cámaras
```

Con FOV real de ~60° (OpenMV lente gran angular), 4 cámaras cubren 240° con overlaps.

---

## 5. ESTIMACIÓN CUANDO NO VEO: KALMAN PREDICT

Cuando ninguna cámara ve la pelota, el WorldModel NO la pierde inmediatamente. Usa el Kalman filter para predecir dónde está:

```cpp
void update_world_model() {
    // 1. SIEMPRE hacer predict del Kalman (100 Hz)
    world.ball_kalman.predict(0.01f);

    // 2. ¿Alguna cámara ve la pelota?
    if (fused_ball_detected) {
        world.ball_kalman.update(fused_ball_x, fused_ball_y, measurement_R);
        world.ball.source = detection_source;
    }
    // Si NO la veo: el Kalman sigue prediciendo con modelo de fricción
    // La confianza baja automáticamente (ver ball-tracking-advanced.md)

    // 3. ¿Mi compañero ve la pelota?
    if (partner_msg.ball_visible && !fused_ball_detected) {
        // El compañero la ve pero yo no → usar dato del compañero
        world.ball_kalman.update(partner_ball_x, partner_ball_y,
                                 partner_measurement_R * 2);  // Más ruido por WiFi
        world.ball.source = SRC_PARTNER_WIFI;
    }

    // 4. Copiar estado del Kalman al WorldModel
    world.ball.x = world.ball_kalman.x[0];
    world.ball.y = world.ball_kalman.x[1];
    world.ball.vx = world.ball_kalman.x[2];
    world.ball.vy = world.ball_kalman.x[3];
    world.ball.confidence = world.ball_kalman.confidence;
    world.ball.last_seen_ms = world.ball_kalman.last_update_ms;
    world.ball_visible = world.ball.confidence > 20;
}
```

### Jerarquía de confianza por fuente

| Fuente | R (noise) | Confianza máxima | Latencia |
|--------|:-:|:-:|:-:|
| Mi cámara (cercana, <300mm) | 100 | 100 | 33ms |
| Mi cámara (lejana, >600mm) | 400 | 70 | 33ms |
| Cámara compañero (WiFi) | 800 | 50 | 100-200ms |
| Kalman predict (sin medición) | — | Decae 2/frame | 0ms |
| Última posición conocida | — | 0 después de 2s | — |

---

## 6. COMUNICACIÓN DE EQUIPO (WiFi)

### Mensaje extendido entre robots

```cpp
struct TeamMessage {
    // Mi estado
    uint8_t  role;              // GOALKEEPER / STRIKER
    uint8_t  state;             // FSM state
    int16_t  my_x, my_y;       // Mi posición (mm, signed)
    int16_t  my_heading;        // Mi heading (décimas de grado)

    // Lo que yo veo de la pelota
    int16_t  ball_x, ball_y;    // Posición en campo (mm)
    int16_t  ball_vx, ball_vy;  // Velocidad (mm/s)
    uint8_t  ball_confidence;   // 0-100
    uint8_t  ball_source;       // Bitmask de fuentes

    // Lo que yo veo de los rivales
    int16_t  rival1_x, rival1_y;
    int16_t  rival2_x, rival2_y;
    uint8_t  rivals_seen;       // 0, 1, o 2

    // Intención
    int16_t  target_x, target_y; // A dónde estoy yendo
    uint8_t  seq;                // Número de secuencia
    uint8_t  checksum;           // CRC simple
};  // 30 bytes, enviar cada 100ms (10 Hz)
```

### Protocolo WiFi (ESP32)

```
ESP32-NOW (peer-to-peer, sin router):
  - Latencia: ~5-15ms
  - Rango: >10m (suficiente para campo de 2.4m)
  - Sin pairing complejo
  - Broadcast o unicast
  - Callback de confirmación

Frequencia: 10 Hz (cada 100ms)
Tamaño: 30 bytes × 10 Hz = 300 bytes/s (trivial)
```

---

## 7. ARCOS COMO LANDMARKS PARA POSICIONAMIENTO

Los arcos cyan/magenta son landmarks fijos. Si veo un arco, sé en qué dirección está y puedo refinar mi posición.

```cpp
void update_position_from_goal(Detection& goal_det, bool is_own_goal) {
    // El arco está en una posición fija conocida
    float goal_y = is_own_goal ? 0 : FIELD_LENGTH;
    float goal_x = FIELD_WIDTH / 2;  // Centro del campo

    // Si veo el arco a ángulo A y distancia D:
    float global_angle = (world.me.heading + goal_det.angle) * DEG2RAD;
    float estimated_x = goal_x - goal_det.distance * sinf(global_angle);
    float estimated_y = goal_y - goal_det.distance * cosf(global_angle);

    // Corregir posición con peso bajo (landmark, no medición directa)
    world.me.x = world.me.x * 0.8f + estimated_x * 0.2f;
    world.me.y = world.me.y * 0.8f + estimated_y * 0.2f;
}
```

---

## 8. DETECCIÓN DE RIVALES

Los rivales son los objetos más difíciles de detectar: no tienen color específico, se mueven rápido, y pueden confundirse con compañero.

### Estrategias por nivel de sofisticación

| Nivel | Método | Complejidad |
|:--:|--------|:-:|
| 1 | Ignorar rivales (la mayoría de equipos Junior) | Ninguna |
| 2 | ToF detecta "algo" que no es pared → rival | Baja |
| 3 | LiDAR filtra paredes, resto = obstáculos | Media |
| 4 | Cámara detecta blobs grandes no-naranja, no-cyan, no-magenta | Media |
| 5 | Mi compañero me dice su posición → lo que no es mi compañero es rival | Media |

Para IITA 2026, **Nivel 2+5** es realista: ToF detecta obstáculos, y descartamos la posición del compañero (conocida por WiFi) para identificar rivales.

---

## 9. ESCENARIOS Y FLUJOS DE DECISIÓN

### Escenario: Pelota desaparece (oclusión por rival)

```
t=0:   Cámara frontal ve pelota en (400, 800), confidence=90
t=33ms: Rival se cruza, cámara no ve pelota
        → Kalman predict: pelota en (405, 810), confidence=88
t=66ms: Sigo sin ver
        → Kalman predict: (410, 818), confidence=86
t=100ms: Compañero la ve en (415, 825), confidence=60 (por WiFi)
         → Kalman update con dato del compañero, confidence=70
t=133ms: Mi cámara la ve de nuevo en (418, 830), confidence=90
         → Kalman update, confidence=95
         → Nunca la perdí realmente. Transición suave.
```

### Escenario: Pelota pateada (cambio brusco de dirección)

```
t=0:   Pelota en (500, 600) moviéndose lento (vx=50, vy=30)
t=33ms: Rival la patea
t=66ms: Cámara detecta pelota en (300, 900) ← innovation ENORME
        → Kalman detecta cambio brusco (innovation > 200mm)
        → Resetea velocidad, acepta nueva posición
        → Aprende nueva velocidad en 3-5 frames
```

### Escenario: Decisión de quién va a la pelota

```
Mi posición: (200, 400), soy STRIKER
Compañero: (900, 200), es GOALKEEPER
Pelota: (500, 500)

Distancia mía: 316mm
Distancia compañero: 500mm
→ Yo estoy más cerca → YO voy, compañero cubre

Pero si la pelota va hacia nuestro arco (vy < -200):
→ GOALKEEPER tiene prioridad, STRIKER orbita
```

---

## 10. PIPELINE COMPLETO DE ACTUALIZACIÓN

```
Cada 10ms (100 Hz loop):

  1. Leer IMU → actualizar world.me.heading (BNO055, 0.3ms)
  2. Leer encoders → actualizar odometría → world.me.x, world.me.y
  3. Leer ToF (si disponible) → corregir posición
  4. Predict Kalman de pelota y rivales

Cada 33ms (cuando llega frame de cámara ~30 Hz):

  5. Recibir detecciones de cámara(s) via UART
  6. Convertir detecciones cámara→campo
  7. Fusionar multi-cámara (resolver duplicados)
  8. Update Kalman de pelota con detecciones fusionadas
  9. Update arcos (posición desde landmarks)
  10. Update rivales (detecciones no identificadas)

Cada 100ms (cuando llega WiFi ~10 Hz):

  11. Recibir TeamMessage del compañero
  12. Actualizar world.partner
  13. Si compañero ve pelota y yo no → update Kalman con su dato
  14. Identificar rivales (obstáculos que no son mi compañero)

Cada ciclo de FSM (100 Hz):

  15. Decidir acción basada en world completo
```

---

## FUENTES

- PCBWay team: 4 cámaras + Teensy 4.1+4.0 para visión 360° (RoboCup Junior 2024-2025)
- CAMBADA: espejo catadioptric omnidireccional + world model cooperativo (MSL campeones)
- CAMBADA: compartir posición de pelota entre robots (ScienceDirect 2010)
- Neves et al.: Efficient omnidirectional vision for CAMBADA (2010)
- RoboCupJunior Rules 2026: sin restricción de número de cámaras en Vision league
- ESP-NOW: protocolo peer-to-peer para ESP32
- IITA skills: ball-tracking-advanced.md, soccer-match-fsm.md, soccer-ball-detection.md
