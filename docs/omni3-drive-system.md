# Sistema de Movimiento Omnidireccional de 3 Ruedas

## Documento técnico completo para RoboCupJunior Soccer Open

**Autor:** Gustavo Viollaz (IITA Salta) con asistencia de Claude Opus 4.6  
**Fecha:** 2026-03-28

---

## 1. POR QUÉ 3 RUEDAS OMNI

### Ventajas sobre 2WD y 4WD

| Aspecto | 2WD diferencial | 3 omni | 4 mecanum |
|---------|:-:|:-:|:-:|
| Movimiento omnidireccional | No | **Sí** | Sí |
| Giro en el lugar | Sí | **Sí** | Sí |
| Traslación + rotación independientes | No | **Sí** | Sí |
| Cantidad de motores | 2 | **3** | 4 |
| Complejidad mecánica | Baja | **Media** | Alta |
| Fuerza de empuje | Alta | Media | Media |
| Eficiencia energética | Alta | Media | Baja |
| Uniformidad de velocidad | Alta | **Variable según dir.** | Uniforme |
| Tamaño / peso | Bajo | **Medio** | Alto |

### La limitación del 3-omni: velocidad no uniforme

Con 3 ruedas a 120°, la velocidad máxima depende de la DIRECCIÓN:

```
Dirección 0° (frente):     100% de vel max (una rueda empuja directo)
Dirección 30° (diagonal):  86.6% de vel max
Dirección 90° (lateral):   86.6% de vel max (dos ruedas combinadas)
```

Esto es una diferencia del 13.4% que en la práctica es imperceptible. La solución de la clase `OmniDriveBase` normaliza las velocidades de motor para que nunca se exceda el límite, manteniendo la dirección correcta.

---

## 2. HARDWARE TÍPICO PARA ROBOCUP JUNIOR

```
Componentes:
  3x Motor DC con encoder (ej: JGA25-370, Pololu micro metal)
  3x Rueda omni (48mm o 58mm diámetro)
  3x Driver de motor (H-bridge: L298N, DRV8833, TB6612)
  1x Teensy 4.1 (controller principal)
  1x BNO055 (heading)
  1x OpenMV H7+ (visión)
  Batería: LiPo 2S o 3S
  Estructura: base circular 3D printed
```

### Montaje típico

```
Vista superior:

        ○ Rueda 0 (frente, 0°)
       /|\
      / | \
     /  |  \
    /   |   \
   / [TEENSY] \
  / [BNO055]   \
 ○─────────────○
 R1 (240°)     R2 (120°)

Diámetro total: ~160-180mm (dentro de 200mm Soccer limit)
```

> ⚠️ NOTA: este montaje (0deg/120deg/240deg) es un TEMPLATE conceptual KIWI generico y NO es la geometria del robot 2026. El robot real (ROBOT1) usa drivers U5/U17/U7 con WHEEL_ANGLES_DEG={330, 210, 90} (CALIBRADO 2026-06-08; ver config_central.h y docs/firmware/DIAG-CENTRAL-MOTORS.md). La disposicion VALIDADA es: M1/U5=330deg delantera-IZQUIERDA, M2/U17=210deg delantera-DERECHA (INVERTIDO HW), M3/U7=90deg trasera. Pendiente de banco: SOLO el tuneo fino del lateral (que no rote) + confirmar el sentido de la traslacion.

---

## 3. NIVELES DE CONTROL

```
Nivel 4: ESTRATEGIA
  "Ir a la pelota y patear al arco"
  ↓
Nivel 3: NAVEGACIÓN (OmniDriveBase)
  go_to(500, 800, 0)    // x, y, heading
  move(90, 300)          // dirección, distancia
  ↓
Nivel 2: CINEMÁTICA
  (vx, vy, omega) → (w1, w2, w3)
  Field-centric transform con gyro
  ↓
Nivel 1: PID DE VELOCIDAD (IntervalTimer 1kHz)
  Encoder → PID → PWM para cada motor
```

### Nivel 1: PID de velocidad de motor

CADA motor tiene su propio PID que corre en interrupt a 1kHz:

```cpp
// En IntervalTimer ISR:
for (int i = 0; i < 3; i++) {
    float actual_speed = read_encoder_speed(i);  // rad/s
    float error = motor_setpoint[i] - actual_speed;
    float output = motor_pid[i].compute(error);
    set_motor_pwm(i, output);
}
```

Esto es FUNDAMENTAL. Sin PID de velocidad por motor, el robot no va recto porque los motores tienen respuestas diferentes.

### Nivel 2: Cinemática (100 Hz en loop)

Convierte (vx, vy, omega) del nivel 3 en setpoints para los 3 PIDs de motor.

### Nivel 3: Navegación (OmniDriveBase)

La clase `OmniDriveBase` del skill `omni3-drive-base.md` implementa este nivel completo.

---

## 4. CALIBRACIÓN: PASO A PASO

### 4.1 Medir wheel_radius

```
1. Marcar un punto en la rueda y en el piso
2. Rodar la rueda exactamente 1 vuelta completa
3. Medir la distancia recorrida en el piso (en mm)
4. wheel_radius = distancia / (2 * π)

Ejemplo: rueda omni 58mm nominal
  Circunferencia medida: 179mm
  wheel_radius = 179 / 6.283 = 28.5mm
  (NO usar el valor nominal de 29mm!)
```

### 4.2 Medir robot_radius

```
1. Medir distancia del centro del robot al punto de contacto
   de la rueda con el piso
2. Usar esta distancia como robot_radius

Ejemplo: 85mm
```

### 4.3 Calibrar PID de heading

```
1. Poner heading_ki = 0, heading_kd = 0
2. Empezar con heading_kp = 1.0
3. Comandar turn(90) y observar:
   - Si oscila: bajar Kp
   - Si tarda mucho: subir Kp
4. Cuando llega sin overshoot excesivo: agregar Kd
   heading_kd = Kp * 0.1 (punto de partida)
5. Si hay error estacionario: agregar Ki pequeño
```

### 4.4 Calibrar odometría

```
1. Comandar move(0, 1000) (avanzar 1 metro)
2. Medir con regla cuánto avanzó realmente
3. Ajustar wheel_radius: si avanzó 1050mm, rueda es más grande
   wheel_radius_nuevo = wheel_radius * (1000 / 1050)
4. Repetir para movimiento lateral: move(90, 1000)
5. Repetir para giro: turn(360) y medir si hace exactamente 360°
   Si hace 365°: robot_radius es más chico de lo medido
```

---

## 5. CONSUMO ENERGÉTICO POR DIRECCIÓN

Investigación de Guo et al. muestra que el consumo energético de un robot 3-omni varía significativamente según la dirección de movimiento. Moverse a 0° (una rueda empuja directo) consume menos que moverse a 30° (las 3 ruedas contribuyen parcialmente).

Para competencia esto es relevante para **planificación de trayectoria**: si hay dos caminos equivalentes, preferir el que usa direcciones más eficientes energéticamente.

---

## 6. PROBLEMAS COMUNES Y SOLUCIONES

| Problema | Causa | Solución |
|----------|-------|----------|
| Robot no va recto | Motores sin PID individual | PID de velocidad por motor a 1kHz |
| Robot gira cuando debería ir recto | Heading PID off | Calibrar heading PID con gyro |
| Distancias incorrectas | wheel_radius mal medido | Calibrar con movimiento real de 1m |
| Giros inexactos | robot_radius mal medido | Calibrar con giro de 360° |
| Robot "patina" al arrancar | Aceleración excesiva | Rampa de aceleración (trapezoidal) |
| Odometría driftea | Acumulación de error | Corregir con ToF/EKF periódicamente |
| Rueda pierde tracción | Superficie resbalosa o rueda sucia | Limpiar ruedas, verificar peso distribuido |
| Movimiento errático bajo carga | Motor se satura (PWM 100%) | Normalizar velocidades en cinemática |

---

## FUENTES

- Oliveira et al.: Dynamical Models for Omni-directional Robots (CMU, 2008)
- Guo et al.: Power consumption modeling of 3-wheeled omnidirectional robot
- Pybricks DriveBase API: inspiración de interfaz
- Modern Robotics (Northwestern) Ch.13.2
- Springer: 3-servo-wheel platform with OPS-9 plane positioning
- RoBorregos: robocup-soccer-open-2024 (3-omni architecture)
- manav20/3-wheel-omni: PID + vectoring (GitHub)
- IITA legacy 2025 season kinematics code
