# 🔄 Dribbler Mechanics & Control

## Resumen
Guía sobre diseño mecánico, control electrónico y software para dribblers (rodillos retenedores de pelota), un componente crítico para dominar la categoría Soccer y mantener el control del balón al girar o esquivar rivales.

## Reglas Críticas RCJ Soccer
- El dribbler **no puede cubrir más del 20%** de la pelota.
- La pelota no debe quedar atascada si el robot se levanta.

## 1. Diseño Físico y Electromecánico
* **Material**: Rodillos de silicona (Shore A 20-40) o tubos de látex montados sobre un eje de aluminio o fibra de carbono.
* **Motores**:
    - **Brushed DC (Maxon / Faulhaber)**: Excelente torque a baja velocidad, fácil control (PWM estándar).
    - **Brushless (BLDC, ej. motores de drones pequeñas)**: Alta RPM, requieren ESC, pero muy ligeros.
* **Transmisión**: Correas síncronas (Timing belts, GT2) para mover el motor fuera de la zona de retención y dar el torque adecuado.
* **Suspensión**: Montar el ensamble del dribbler sobre un mecanismo pivotante con resortes para absorber impactos y mantener presión constante sobre la pelota (compliance).

## 2. Electrónica
- Usar un driver dedicado (ej. DRV8838 o un puente H pequeño) para motores Brushed.
- Usar un ESC estándar (señal PWM de servo de 1000us a 2000us) para motores Brushless.

## 3. Control de Software (PID Activo)

Un dribbler no debe estar encendido al 100% todo el tiempo, ya que consume mucha batería, calienta los motores y puede empujar la pelota fuera en lugar de retenerla.

### Algoritmo Básico (Encendido/Apagado por IR)
Se requiere un sensor infrarrojo (TCRT5000 o ToF Vl53l0x) montado justo detrás del dribbler.
```cpp
if (ball_detected_in_kicker_sensor()) {
    dribbler.setSpeed(75); // Retención suave
} else if (ball_is_close_in_camera()) {
    dribbler.setSpeed(100); // Modo succión activa (Acercamiento)
} else {
    dribbler.setSpeed(0); // Ahorro de batería
}
```

### Control Dinámico al Girar
Si el robot gira rápidamente (Omega muy alto), la fuerza centrífuga tiende a expulsar la pelota. El controlador debe aumentar la velocidad del dribbler proporcionalmente a la velocidad angular del robot.

```cpp
float omega = imu.getGyroZ();
float base_speed = 70.0;
float dynamic_speed = base_speed + abs(omega) * K_omega;
dribbler.setSpeed(constrain(dynamic_speed, 0, 100));
```
