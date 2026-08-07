# ⏱️ RTOS Firmware Architecture

## Resumen
Este skill detalla la implementación de una arquitectura de tiempo real duro basada en un Sistema Operativo en Tiempo Real (RTOS) como **FreeRTOS** o **Zephyr** para controlar robots de fútbol a frecuencias ultra-altas (100Hz - 1000Hz).

## Cuándo usar este skill
- Cuando el `loop()` principal de Arduino/C++ sufre latencias variables (jitter) debido a lecturas de sensores I2C/UART bloqueantes.
- Para sincronizar la lectura de múltiples cámaras, IMU, LiDAR y control PID de motores sin interrupciones.
- Típicamente usado en placas potentes como Teensy 4.0/4.1 (ARM Cortex-M7) o ESP32.

## Diseño de Tareas Concurrentes (FreeRTOS)

Una buena arquitectura separa el firmware en múltiples hilos (Tasks) con diferentes prioridades. Las colas (Queues) o semáforos se usan para compartir datos de manera segura (Mutex).

| Tarea (Task) | Frecuencia | Prioridad | Descripción |
|--------------|------------|-----------|-------------|
| **ControlTask** | 100 Hz | Alta (4) | Loop PID de motores y Cinemática Inversa. Requiere determinismo estricto. |
| **ImuTask** | 200 Hz | Alta (3) | Lectura del BNO055 via I2C o SPI e integración de heading. |
| **VisionTask** | 30-60 Hz | Media (2) | Recepción asíncrona (UART/SPI) de datos desde la OpenMV/RPi. |
| **LogicTask** | 50 Hz | Media (2) | La FSM de estrategia (Striker/Goalie). Evalúa el World Model y define el `setpoint` de movimiento. |
| **CommTask** | 10-20 Hz | Baja (1) | Envío de telemetría a la PC, módulo oficial del árbitro, y ESP-NOW. |

## Ejemplo de Implementación (C++)

```cpp
#include <Arduino_FreeRTOS.h>

// Handle de las tareas
TaskHandle_t hControlTask;
TaskHandle_t hLogicTask;

// Estructura compartida segura
struct RobotState {
    float current_x, current_y, current_theta;
    float ball_x, ball_y;
} robot_state;

// Mutex para el estado
SemaphoreHandle_t stateMutex;

void setup() {
    stateMutex = xSemaphoreCreateMutex();
    
    xTaskCreate(ControlTask, "Control", 256, NULL, 4, &hControlTask);
    xTaskCreate(LogicTask, "Logic", 512, NULL, 2, &hLogicTask);
    
    vTaskStartScheduler();
}

void ControlTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = 10; // 100Hz = 10ms (asumiendo 1 tick = 1ms)
    
    while(1) {
        // 1. Obtener setpoint del motor
        // 2. Leer encoders
        // 3. Ejecutar PID
        // 4. Escribir PWM a motores
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Determinismo estricto
    }
}
```

## Prevención de Problemas
- **Priority Inversion**: Usar Priority Inheritance (Mutexes en lugar de Semáforos binarios).
- **Deadlocks**: Siempre adquirir mutexes en el mismo orden y mantener el tiempo de bloqueo al mínimo indispensable.
- **Stack Overflow**: Monitorizar `uxTaskGetStackHighWaterMark` para ajustar el tamaño del stack de cada tarea.
