# 🏠 Navegación en Campo de Soccer — RoboCupJunior Soccer Open

---

## 1. DIMENSIONES DEL CAMPO (2026)

```
┌─────────────────────────┐
│    [ARCO RIVAL cyan]        │
│─────────────────────────│  243 cm
│                             │
│          MITAD RIVAL        │
│                             │
│ - - - - CENTRO - - - - - - │  Línea central
│                             │
│        NUESTRA MITAD        │
│                             │
│─────────────────────────│
│   [NUESTRO ARCO magenta]    │
└─────────────────────────┘
         182 cm

Interior: 182 × 243 cm
Área exterior: 182+30 × 243+30 cm (borde de 30cm)
Paredes: negro mate, 22 cm de alto
Piso: carpet verde oscuro
Líneas: blancas (bordes de campo, áreas, centro)
Arcos: ~60 cm de ancho, colores cyan y magenta
Pelota: golf ball naranja, 42mm diámetro
```

---

## 2. SENSORES DE LÍNEA (NO SALIR DEL CAMPO)

Regla crítica: si el robot pisa la línea blanca, está al borde del campo.

```cpp
// 4 sensores IR de línea en la base del robot
// (norte, sur, este, oeste)
#define LINE_THRESHOLD 500  // Calibrar (blanco = alta reflectancia)

bool line_north = analogRead(LINE_N) > LINE_THRESHOLD;
bool line_south = analogRead(LINE_S) > LINE_THRESHOLD;
bool line_east  = analogRead(LINE_E) > LINE_THRESHOLD;
bool line_west  = analogRead(LINE_W) > LINE_THRESHOLD;

void avoid_line() {
    if (line_north) set_robot_velocity(0, -300, 0);  // Retroceder
    if (line_south) set_robot_velocity(0, 300, 0);   // Avanzar
    if (line_east)  set_robot_velocity(-300, 0, 0);  // Izquierda
    if (line_west)  set_robot_velocity(300, 0, 0);   // Derecha

    // Si 2 sensores a la vez: esquina
    if (line_north && line_east) set_robot_velocity(-200, -200, 0);
    // etc.
}
```

---

## 3. POSICIONAMIENTO CON ToF

Si el robot tiene sensores ToF (VL53L1X), puede calcular su posición:

```
Paredes del campo Soccer:
  Pared izquierda:  x = 0
  Pared derecha:    x = 1820 (182 cm)
  Pared fondo:      y = 0 (nuestro arco)
  Pared rival:      y = 2430 (arco rival, 243 cm)

Con ToF Este midiendo 400mm:
  mi_x = 1820 - 400 - robot_radius
```

Para implementación completa ver `tof-array-positioning.md` en el repo hermano, cambiando las dimensiones del campo.

---

## 4. ZONAS DEL CAMPO PARA ESTRATEGIA

```cpp
enum FieldZone {
    ZONE_OUR_GOAL,      // Cerca de nuestro arco (y < 400)
    ZONE_OUR_HALF,      // Nuestra mitad (400 < y < 1215)
    ZONE_THEIR_HALF,    // Mitad rival (1215 < y < 2030)
    ZONE_THEIR_GOAL     // Cerca del arco rival (y > 2030)
};

FieldZone get_zone(float y) {
    if (y < 400)  return ZONE_OUR_GOAL;
    if (y < 1215) return ZONE_OUR_HALF;
    if (y < 2030) return ZONE_THEIR_HALF;
    return ZONE_THEIR_GOAL;
}

// Restricciones por rol:
// Arquero: solo ZONE_OUR_GOAL y parte de ZONE_OUR_HALF
// Delantero: puede ir a cualquier zona pero prioriza ZONE_THEIR_HALF
```

---

## 5. HEADING Y ORIENTACIÓN

Convención de heading:
```
0° = mirando hacia arco rival (norte)
90° = mirando hacia la derecha
180° = mirando hacia nuestro arco
-90° = mirando hacia la izquierda
```

El BNO055 se resetea al inicio del partido con esta convención:
```cpp
void setup() {
    imu.begin(MODE_IMUPLUS);
    // Robot apuntando hacia arco rival al inicio
    imu.reset_heading();  // heading = 0°
}
```

---

## FUENTES

- RoboCupJunior Soccer Rules 2026 (field specifications)
- IITA legacy 2025 navigation code
- Para posicionamiento ToF completo: ver repo hermano skill `tof-array-positioning.md`
- Para BNO055 óptimo: ver repo hermano skill `bno055-nonblocking-reads.md`
