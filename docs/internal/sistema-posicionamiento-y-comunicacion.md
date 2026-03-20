---
title: "Sistema de Posicionamiento y Comunicación entre Robots"
date: 2026-03-20
author: "Gustavo Viollaz + Claude (Anthropic)"
ai-assisted: true
status: draft
tags: [posicionamiento, comunicacion, estrategia, arquitectura, filtro-kalman]
---

# Sistema de Posicionamiento y Comunicación entre Robots

## 1. VISIÓN

El robot debería mantener un **modelo del mundo** actualizado en tiempo real con la posición, orientación y velocidad de:

| Objeto | Info necesaria | Fuente primaria | Fuente secundaria |
|--------|---------------|-----------------|--------------------|
| Mi robot | x, y, θ (heading), vx, vy | Giróscopo + odometría | TOF/LIDAR, líneas, cámara (arcos) |
| Pelota | x, y, vx, vy | Cámara | Compañero (radio) |
| Compañero | x, y, θ | Radio (compañero informa) | Cámara (si lo ve) |
| Oponente 1 | x, y (estimado) | TOF/cámara | Compañero (radio) |
| Oponente 2 | x, y (estimado) | TOF/cámara | Compañero (radio) |
| Arco propio | x, y (fijo) | Conocido al iniciar | Cámara para verificar |
| Arco contrincante | x, y (fijo) | Conocido al iniciar | Cámara para verificar |

Cada dato debería tener un **índice de confianza** (0.0 a 1.0) que decae con el tiempo:
- Dato recién medido por sensor propio: confianza 0.9-1.0
- Dato recibido del compañero: confianza 0.7-0.9
- Dato estimado/predicho (no visto hace >1s): confianza decae exponencialmente
- Dato desconocido: confianza 0.0

---

## 2. ARQUITECTURA POR CAPAS

```
┌───────────────────────────────────────────────┐
│ CAPA 4: ESTRATEGIA                           │
│ Decide: qué hacer (atacar, defender, buscar)   │
│ Usa: modelo del mundo + reglas de juego        │
├───────────────────────────────────────────────┤
│ CAPA 3: MODELO DEL MUNDO                      │
│ Mantiene: posición de todos los objetos         │
│ Fusión: sensores propios + info del compañero   │
│ Predicción: estima posiciones no observadas      │
├───────────────────────────────────────────────┤
│ CAPA 2: COMUNICACIÓN                           │
│ Envía: mi posición + lo que veo                 │
│ Recibe: posición del compañero + lo que ve      │
│ Concilia: fusiona ambas visiones del mundo      │
├───────────────────────────────────────────────┤
│ CAPA 1: SENSORES                               │
│ Giróscopo (heading), Cámara (pelota/arcos),    │
│ Líneas (borde), TOF (paredes/oponentes),       │
│ Encoders (velocidad ruedas)                     │
└───────────────────────────────────────────────┘
```

---

## 3. CAPA 1: SENSORES — QUÉ TENEMOS Y QUÉ NOS FALTA

### 3.1 Disponible hoy

| Sensor | Dato | Frecuencia | Precisión | Limitación |
|--------|------|-----------|----------|------------|
| BNO055 (giróscopo) | Heading (θ) | 100 Hz | ±2° (instantáneo) | Drift ~3°/min en IMUPLUS |
| OpenMV (cámara) | Pelota (x,y), Arcos (x,y) | ~30 Hz | ~5cm a 1m | Solo frontal, ~70° FOV |
| Sensores línea (x3) | Borde sí/no | ~1000 Hz | Binario | Solo 3 puntos, no ángulo |

### 3.2 Necesario agregar (por prioridad)

| Sensor | Dato | Para qué | Prioridad |
|--------|------|---------|----------|
| TOF frontales (x2-3) | Distancia a obstáculo | Evitar oponentes | Alta |
| Lente wide-angle | Mejor FOV cámara | Ver más pelota/arcos | Media |
| TOF radiales (x8-16) | Distancia a paredes | Posición en cancha | Media (Roboliga) |
| Encoders (x3) | Velocidad ruedas | Odometría | Media (Roboliga) |
| 2do BNO055 | Heading redundante | Confiabilidad | Baja |

---

## 4. CAPA 2: COMUNICACIÓN ENTRE ROBOTS

### 4.1 Protocolo propuesto

Cada robot envía su estado 10 veces por segundo (~100ms) al compañero:

```
Paquete (estimado ~20 bytes):
{
  robot_id:      1 byte   (0 = delantero, 1 = arquero)
  mi_x:          2 bytes  (posición X en mm, 0-2400)
  mi_y:          2 bytes  (posición Y en mm, 0-1800)
  mi_heading:    2 bytes  (heading en décimas de grado, 0-3600)
  pelota_x:      2 bytes  (0 = no la veo)
  pelota_y:      2 bytes
  pelota_conf:   1 byte   (confianza 0-100)
  oponente1_x:   2 bytes  (0 = no lo veo)
  oponente1_y:   2 bytes
  timestamp:     2 bytes  (millis mod 65536)
  checksum:      1 byte   (XOR de todos los bytes)
}
```

### 4.2 Hardware de radio

**Opción recomendada**: ESP-NOW sobre el mismo ESP32 del módulo de árbitros (HW-001). Así un solo ESP32 maneja ambas funciones: comunicación con árbitros (WiFi) + comunicación entre robots (ESP-NOW, que funciona sin router).

ESP-NOW tiene latencia de ~1-5ms y alcance de ~50m en línea directa. Más que suficiente para una cancha de 2.4m x 1.8m.

### 4.3 Conciliación de información

Cuando ambos robots reportan posición de la pelota:
- Si ambos la ven: promediar ponderado por confianza y distancia (el que está más cerca es más preciso)
- Si solo uno la ve: usar esa posición con confianza reducida para el otro
- Si ninguno la ve: estimar posición basada en última posición conocida + velocidad estimada. Decaer confianza exponencialmente.

---

## 5. CAPA 3: MODELO DEL MUNDO

### 5.1 Estructura de datos

```cpp
struct ObjetoMundo {
    float x;           // posición X en cancha (mm)
    float y;           // posición Y en cancha (mm)
    float heading;     // orientación (grados, solo para robots)
    float vx;          // velocidad estimada X (mm/s)
    float vy;          // velocidad estimada Y (mm/s)
    float confianza;   // 0.0 a 1.0
    unsigned long ultimaActualizacion;  // millis()
};

struct ModeloMundo {
    ObjetoMundo miRobot;
    ObjetoMundo companero;
    ObjetoMundo pelota;
    ObjetoMundo oponente[2];
    // Arcos son posiciones fijas conocidas
};
```

### 5.2 Fusión sensorial

**Implementación progresiva** (de simple a complejo):

**Nivel 1 (para Mundial)**: Fusión simple. Heading del giróscopo + posición relativa de la pelota desde la cámara. Sin posición absoluta del robot.

**Nivel 2 (Roboliga)**: Fusión media. Agregar posición del robot estimada por TOF (distancia a paredes). Filtro complementario simple: 90% posición anterior + 10% nueva medición.

**Nivel 3 (futuro)**: Filtro de Kalman extendido. Modelo cinemático del robot como predicción, sensores como corrección. Estado: [x, y, θ, vx, vy, ω]. Observaciones: giróscopo (θ), cámara (posición relativa pelota/arcos), TOF (distancia paredes), encoders (vx, vy, ω).

---

## 6. APLICACIONES DEL MODELO DEL MUNDO

### 6.1 Mapa de velocidad (heat map)

Sabiendo la posición del robot en la cancha, se puede definir velocidad adaptativa:

```
┌────────────────────────────────────────┐
│ LENTO   MEDIO    RAPIDO    MEDIO   LENTO │
│  40%     70%      100%      70%     40%  │
│                                          │
│ LENTO   MEDIO    RAPIDO    MEDIO   LENTO │
│  40%     70%      100%      70%     40%  │
└────────────────────────────────────────┘
```

Cerca del borde: velocidad reducida para no salirse.
En el centro: velocidad máxima.

### 6.2 Coordinación de roles

Con posición conocida de ambos robots:
- Si ambos persiguen la pelota: el más cercano ataca, el otro cubre
- Si la pelota está en nuestra mitad: delantero retrocede a defender
- Si el compañero tiene la pelota: posicionarse para recibir pase

### 6.3 Predicción de pelota

Con velocidad estimada de la pelota:
- Anticipar hacia dónde va la pelota
- El arquero puede moverse ANTES de que la pelota llegue
- Si la pelota se dirige al arco propio: alarma al arquero

---

## 7. IMPLEMENTACIÓN PROGRESIVA

| Fase | Qué se implementa | Cuándo | Hardware necesario |
|------|-------------------|--------|-------------------|
| 0 | Solo heading + pelota relativa (actual) | Ahora | Nada nuevo |
| 1 | moverRobot() con PD heading | Abril 2026 | Nada nuevo |
| 2 | Estimación posición por cámara (arcos) | Mayo 2026 | Nada nuevo (o lente wide) |
| 3 | Comunicación básica entre robots | Junio 2026 | ESP32 (del módulo árbitros) |
| 4 | TOF frontales + evasión | Julio 2026 | 2-3 VL53L0X |
| 5 | Posición por TOF radiales | Post-mundial | 8-16 TOF + placa |
| 6 | Filtro de Kalman + fusión completa | Post-mundial | Encoders + todo lo anterior |
| 7 | Mapa de velocidad + coordinación | Post-mundial | Todo lo anterior |

---

## 8. REFERENCIAS

- TDPs de equipos top de RoboCup Junior Soccer Open que implementan localización (pendiente investigar en `research/backlog/`)
- RoboCup MSL (Medium Size League) papers sobre localización — conceptos aplicables a Junior con simplificación
- Filtro de Kalman para robots móviles: "Probabilistic Robotics" (Thrun, Burgard, Fox) — referencia académica
- ESP-NOW: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
