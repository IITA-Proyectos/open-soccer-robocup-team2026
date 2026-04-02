# 📡 LiDAR vs ToF para Posicionamiento — Guía de Decisión

## Skill: cuándo usar cada tecnología en RoboCup Soccer

---

## 1. REGLAS 2026: AHORA SÍ SE PUEDE

Desde 2026, la liga **Soccer Vision (ex-Open) PERMITE** sensores que emiten IR (ToF, LiDAR, IR distance). La restricción aplica **SOLO a la liga Infrared** (ex-Lightweight).

**Verificar reglas nacionales antes de implementar.**

---

## 2. CUADRO DE DECISIÓN

```
¿Tu robot tiene espacio arriba (>40mm libres)?
  └─ No → 8× VL53L1X (perimetral, tiny)
  └─ Sí → ¿Presupuesto > $100 USD?
           └─ No → 8× VL53L1X ($26)
           └─ Sí → ¿Necesitás detectar robots rivales?
                    └─ No → VL53L1X (más rápido, más simple)
                    └─ Sí → RPLidar A1 (360° obstacle detection)
```

---

## 3. IMPLEMENTACIÓN VL53L1X PARA SOCCER

El campo de soccer es más chico que RoboSports (182×243cm vs 236×114cm). Los ToF llegan perfecto.

```cpp
// Adaptar field geometry para Soccer
struct SoccerFieldGeometry {
    float wall_left   = 0;       // x = 0
    float wall_right  = 1820;    // x = 1820mm (182 cm)
    float wall_back   = 0;       // y = 0 (nuestro arco)
    float wall_front  = 2430;    // y = 2430mm (arco rival, 243 cm)
    float robot_radius = 100;    // mm
};

// El resto del código de ToFArray funciona igual
// Ver repo hermano: skills/03-sensor-fusion/tof-array-positioning.md
```

---

## 4. IMPLEMENTACIÓN RPLIDAR A1 (SI SE DECIDE USAR)

### Conexión

```
RPLidar A1 → Teensy 4.1 Serial3 (o cualquier Serial libre)
  TX → RX3 (pin 15)
  RX → TX3 (pin 14)
  GND → GND
  5V → 5V (motor necesita 5V)
  CTRL → pin digital (para encender/apagar motor)
```

### Lectura básica

```cpp
#include <RPLidar.h>  // Librería SLAMTEC

RPLidar lidar;

void setup() {
    lidar.begin(Serial3);
    pinMode(PIN_LIDAR_MOTOR, OUTPUT);
    analogWrite(PIN_LIDAR_MOTOR, 200);  // Motor a velocidad media
}

// Estructura para un scan completo
struct LidarScan {
    float distances[360];  // Distancia en mm por cada grado
    bool valid[360];       // ¿Dato válido?
    unsigned long timestamp;
};

LidarScan current_scan;

void update_lidar() {
    if (IS_OK(lidar.waitPoint())) {
        RPLidarMeasurement point = lidar.getCurrentPoint();
        int angle = (int)point.angle;  // 0-359
        if (angle >= 0 && angle < 360 && point.quality > 10) {
            current_scan.distances[angle] = point.distance;
            current_scan.valid[angle] = true;
        }
    }
}
```

### Localización por paredes conocidas

```cpp
struct Position {
    float x, y, heading;
    float confidence;
};

Position localize_from_scan(LidarScan& scan, float imu_heading) {
    // 1. Buscar las 4 paredes del campo en el scan
    // Las paredes son líneas rectas a distancias conocidas
    
    // Simplificación: usar las 4 direcciones cardinales
    // (ajustadas por el heading del robot)
    float dist_forward = get_average_distance(scan, imu_heading, 10);  // ±10°
    float dist_back    = get_average_distance(scan, imu_heading + 180, 10);
    float dist_right   = get_average_distance(scan, imu_heading + 90, 10);
    float dist_left    = get_average_distance(scan, imu_heading - 90, 10);
    
    Position pos;
    pos.heading = imu_heading;  // Del gyro (más preciso que LiDAR)
    
    // X: de paredes laterales
    if (dist_left > 0 && dist_right > 0) {
        pos.x = dist_left + ROBOT_RADIUS;
        // Verificar: dist_left + dist_right ≈ 1820mm
    } else if (dist_left > 0) {
        pos.x = dist_left + ROBOT_RADIUS;
    } else if (dist_right > 0) {
        pos.x = FIELD_WIDTH - dist_right - ROBOT_RADIUS;
    }
    
    // Y: de paredes delante/atrás
    if (dist_back > 0) {
        pos.y = dist_back + ROBOT_RADIUS;
    } else if (dist_forward > 0) {
        pos.y = FIELD_LENGTH - dist_forward - ROBOT_RADIUS;
    }
    
    pos.confidence = 90;  // Alto si vemos paredes
    return pos;
}

float get_average_distance(LidarScan& scan, float center_angle, int half_width) {
    float sum = 0;
    int count = 0;
    for (int d = -half_width; d <= half_width; d++) {
        int idx = ((int)center_angle + d + 360) % 360;
        if (scan.valid[idx] && scan.distances[idx] > 50 && scan.distances[idx] < 3000) {
            sum += scan.distances[idx];
            count++;
        }
    }
    return count > 3 ? (sum / count) : -1;  // -1 = no hay datos suficientes
}
```

### Detección de robots rivales (bonus del LiDAR)

```cpp
struct Obstacle {
    float x, y;       // Posición en el campo
    float size;        // Tamaño estimado
};

void detect_obstacles(LidarScan& scan, Position& my_pos, Obstacle* obstacles, int& n_obs) {
    n_obs = 0;
    
    for (int a = 0; a < 360; a++) {
        if (!scan.valid[a]) continue;
        float d = scan.distances[a];
        
        // Calcular posición absoluta de este punto
        float world_angle = (my_pos.heading + a) * DEG2RAD;
        float px = my_pos.x + d * sinf(world_angle);
        float py = my_pos.y + d * cosf(world_angle);
        
        // ¿Es una pared? (dentro de 50mm de una pared conocida)
        if (px < 50 || px > FIELD_WIDTH-50 || py < 50 || py > FIELD_LENGTH-50) {
            continue;  // Es pared, ignorar
        }
        
        // No es pared → es un obstáculo (robot rival o compañero)
        if (n_obs < MAX_OBSTACLES) {
            obstacles[n_obs++] = {px, py, 100};  // Tamaño estimado
        }
    }
    
    // Clustering: agrupar puntos cercanos
    // (simplificación: ya tenemos los puntos individuales)
}
```

---

## 5. TABLA RESUMEN DE RECOMENDACIÓN

| Escenario | Recomendación | Razón |
|-----------|:--:|-------|
| IITA Soccer 2026 (primera vez) | **8× VL53L1X** | Barato, probado, suficiente para campo soccer |
| Si se quiere detectar rivales | RPLidar A1 | 360° scan permite ver obstáculos |
| Presupuesto limitado | VL53L1X | $26 vs $100 |
| Robot ya muy pesado/lleno | VL53L1X | 10g vs 170g |
| Temporada 2027+ (experiencia previa) | Híbrido | LiDAR arriba + ToF abajo |
| Reglas nacionales prohíben IR | **Solo encoders+gyro** | Cumplir reglamento |

---

## FUENTES

- RoboCupJunior Soccer Rules 2026: IR restriction only for Infrared league
- RPLidar A1 datasheet (SLAMTEC)
- VL53L1X datasheet (ST)
- Ver docs/lidar-tof-slam-analysis.md para análisis completo
- Ver repo hermano: tof-array-positioning.md, parallel-sensing.md
