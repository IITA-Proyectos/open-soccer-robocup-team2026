# 🏎️ Advanced Kinematics & Path Planning

## Resumen
Este skill aborda el control cinemático complejo para bases omnidireccionales de 3 o 4 ruedas y la planificación de trayectorias (Path Planning) para evadir obstáculos dinámicos de manera suave usando Campos de Potencial Artificial (APF) y Curvas de Bézier.

## Cuándo usar este skill
- Cuando los algoritmos reactivos básicos provocan choques con robots rivales o aliados.
- Para evitar movimientos bruscos que hacen patinar las ruedas de los robots.
- Al navegar hacia puntos absolutos del mapa generados por el Sistema de Localización (MCL).

## 1. Campos de Potencial Artificial (APF)
Los APF tratan al robot como una partícula cargada.
- **La meta (Pelota o Posición destino)** actúa como una carga *atractiva*.
- **Los obstáculos (Robots rivales, paredes)** actúan como cargas *repulsivas*.
- El robot se mueve a lo largo del vector de gradiente resultante.

### Matemática del APF
```cpp
Vector2D f_atractiva = K_att * (Meta - Robot); // Fuerza lineal hacia la meta
Vector2D f_repulsiva = Vector2D(0, 0);

for(Obstaculo obs : obstaculos) {
    float dist = distancia(Robot, obs);
    if (dist < Distancia_Seguridad) {
        // La fuerza repulsiva crece inversamente proporcional a la distancia
        f_repulsiva += K_rep * (1.0/dist - 1.0/Distancia_Seguridad) * (1.0/(dist*dist)) * vector_unitario(obs, Robot);
    }
}

Vector2D f_total = f_atractiva + f_repulsiva;
// f_total dictará el setpoint de velocidad (Vx, Vy) del drive_base
```

## 2. Generación de Trayectorias Suaves (Splines / Bézier)
En lugar de ir en línea recta hacia un punto (lo que causa cambios bruscos en velocidad (jerks)), se generan curvas de Bézier cúbicas entre el punto actual y el destino.

El controlador sigue el perfil de velocidad generado a lo largo de la curva.

## 3. Desacoplamiento de Rotación y Traslación
En una base omnidireccional, es crítico que la corrección de orientación (Heading) sea independiente de la traslación (Vx, Vy).

```cpp
// vx, vy calculados por el Path Planner
// omega calculado por el controlador PID del Heading
float v1 = -vx * sin(theta1) + vy * cos(theta1) + R * omega;
float v2 = -vx * sin(theta2) + vy * cos(theta2) + R * omega;
float v3 = -vx * sin(theta3) + vy * cos(theta3) + R * omega;
```

**Consideración de Saturación**:
Si la demanda combinada de `v1, v2, o v3` supera el PWM máximo (255), se debe escalar todo proporcionalmente para no deformar el vector de traslación resultante.
