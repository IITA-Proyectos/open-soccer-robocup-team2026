---
title: "Limitaciones del Robot Actual — Marzo 2026"
date: 2026-03-20
author: "Gustavo Viollaz + Claude (Anthropic)"
ai-assisted: true
status: final
tags: [limitaciones, hardware, sensores, competencia, estado-actual]
---

# Limitaciones del Robot Actual — Marzo 2026

## Propósito

Este documento describe el estado real de los robots del equipo IITA Salta a marzo 2026, identificando todas las limitaciones conocidas con su impacto en competencia. Sirve como línea base para priorizar mejoras.

---

## L1 — Sin módulo de comunicación con árbitros

**Severidad**: BLOQUEANTE para Mundial Incheon 2026

**Descripción**: Las reglas 2026 exigen un módulo de comunicación oficial que permite a los árbitros iniciar y detener los robots. El módulo incluye un ESP32 y una pantalla que muestra un código QR para que los jueces escaneen.

**Estado**: No implementado. No tenemos hardware ni software.

**Impacto**: Sin este módulo, el robot NO puede participar en el mundial. Es un requisito de homologación.

**Análisis técnico**: El módulo estándar de RoboCup Junior usa ESP32 con WiFi para comunicarse con el sistema del árbitro. Necesita: ESP32, pantalla pequeña (OLED o TFT), comunicación serial con el Teensy principal para recibir comandos start/stop, firmware del protocolo oficial.

**Acción requerida**: Investigar especificación oficial del módulo, conseguir hardware, implementar firmware, integrar con el programa principal.

**Plazo**: ANTES de mayo 2026 (deadline de homologación).

---

## L2 — Motores sin encoders

**Severidad**: ALTA

**Descripción**: Los motores TT actuales no tienen encoders. No se puede medir la velocidad real de cada rueda, lo que impide control preciso de trayectoria.

**Estado**: Parcialmente compensado con giróscopo BNO055 (control PD de heading). Pero el heading no corrige errores de velocidad de traslación.

**Impacto en competencia**:
- El robot no avanza derecho → falla al patear (patea torcido)
- Orbitar alrededor de la pelota es impreciso
- No se puede hacer odometría (estimar posición por movimiento)

**Opciones identificadas**:

| Opción | Viabilidad Mundial | Complejidad | Costo |
|--------|-------------------|-------------|-------|
| Encoders magnéticos en motores actuales | Media (espacio físico + pines Teensy) | Media | Bajo |
| Motores brushless con encoders integrados | Baja (rediseño mecánico completo) | Alta | Alto |
| Solo giróscopo (situación actual mejorada) | Alta (ya implementado) | Baja | $0 |

**Decisión recomendada**: Para Mundial: maximizar el uso del giróscopo (IMUPLUS + PD heading, ya en staging). Investigar encoders magnéticos en paralelo. Para Roboliga: migrar a motores brushless con encoders.

**Análisis adicional**: Verificar pines disponibles en Teensy. Los encoders magnéticos (tipo AS5600 o similares) necesitan I2C o analógico, pero si ya estamos usando I2C para el BNO055, habría que usar un multiplexor I2C o encoders con salida de pulsos (interrupt pins). El Teensy tiene buena capacidad de interrupciones, lo que facilitaría encoders de cuadratura.

---

## L3 — Sin detección de oponentes

**Severidad**: ALTA

**Descripción**: El robot no tiene forma de detectar a los robots oponentes. Se mueve ciegamente y choca frecuentemente, lo que puede causar penalizaciones, daños mecánicos, y pérdida de orientación del giróscopo por impacto.

**Estado**: Sin hardware ni software.

**Opciones identificadas**:

| Opción | Viabilidad Mundial | Efectividad | Complejidad |
|--------|-------------------|-------------|-------------|
| Ultrasónicos HC-SR04 frontales (2-3) | Alta | Media (corto alcance, lento) | Baja |
| TOF VL53L0X/VL53L1X frontales | Alta | Alta (rápido, preciso) | Media |
| Detección por cámara (blobs no-pelota-no-arco) | Media | Baja (campo de visión limitado) | Alta |
| Array de IR reflectivos | Media | Media | Media |

**Decisión recomendada**: Para Mundial: colocar 2-3 sensores TOF VL53L0X en la parte delantera. Son rápidos (30ms), precisos, pequeños, I2C (con direcciones configurables). Mucho mejor que HC-SR04 que son lentos y bloqueantes.

**Integración**: Agregar lógica de evasión de obstáculos como prioridad alta en la máquina de estados. Si un TOF detecta obstáculo < 15cm, frenar o esquivar.

---

## L4 — Solo 3 sensores de línea

**Severidad**: MEDIA

**Descripción**: Con 3 sensores de línea (izquierda, centro, derecha) solo podemos detectar que tocamos la línea blanca y retroceder. No podemos seguir la línea (para el arquero posicionarse en el arco) ni detectar la línea desde múltiples ángulos.

**Impacto en competencia**:
- El arquero no puede seguir la línea del área para posicionarse
- La reacción a línea es binaria (detectó/no detectó) sin información de ángulo de incidencia
- Si el robot se acerca a la línea de costado, puede que ningún sensor la detecte

**Solución a futuro**: Placa propietaria con 16-48 sensores de luz (tipo QRD1114 o TCRT5000) distribuidos en anillo en la base del robot, con procesador local (Teensy 4.0 o ESP32) que calcule la posición angular de la línea y la envíe como dato procesado al controlador principal.

**Para Mundial**: Los 3 sensores actuales son funcionales. Asegurar que el chequeo de línea esté en TODOS los estados de la máquina (bug actual del arquero donde falta s3).

---

## L5 — Sin posicionamiento en cancha

**Severidad**: MEDIA-ALTA

**Descripción**: No tenemos forma de saber en qué posición (x, y) de la cancha está el robot. Solo sabemos la orientación (heading del giróscopo), si estamos en el borde (sensores de línea), y hacia dónde quedan los arcos (cámara, cuando los ve).

**Impacto en competencia**:
- No se puede definir zonas de juego (el delantero no sabe si está en su mitad o en la del rival)
- No se puede implementar estrategia basada en posición (velocidad adaptativa, zonas de riesgo)
- No se puede coordinar con el compañero ("vos cubrí la izquierda, yo la derecha")

**Opciones identificadas**:

| Opción | Viabilidad | Precisión | Complejidad |
|--------|-----------|----------|-------------|
| Estimación por cámara (tamaño/posición de arcos) | Alta | Baja (~30cm) | Media |
| Sensores TOF / LIDAR (8-16 sensores) en placa superior | Media | Media (~10cm) | Alta |
| Fusión cámara + TOF + giróscopo + líneas | Baja para mundial | Alta (~5cm) | Muy alta |
| Solo giróscopo + líneas (actual) | Ya implementado | Muy baja | Ninguna |

**Decisión recomendada**: Para Mundial: implementar estimación básica por cámara (tamaño de arcos indica distancia, posición Y indica lateralidad). Es lo más rápido de implementar con hardware existente. Para Roboliga: placa TOF superior.

---

## L6 — Sin sistema de pateo (kicker + dribbler)

**Severidad**: ALTA (competitivamente)

**Descripción**: El robot no tiene kicker (electroímán o solenoide para patear) ni dribbler (motor con gusano para "chupar" la pelota). Actualmente "patea" embistiendo la pelota con el cuerpo del robot.

**Componentes necesarios para un sistema completo**:
- **Kicker**: Solenoide o electroímán + circuito de almacenamiento de energía (capacitor) + MOSFET de control + servo para dirección
- **Dribbler**: Motor pequeño con rodillo/gusano en la parte frontal inferior
- **Alimentación**: Circuito de carga de capacitor separado (10-50V típico para solenoides)

**Impacto en competencia**: Sin kicker, el "pateo" es lento y el rival puede interceptar. Sin dribbler, el robot no puede mantener posesión de la pelota.

**Decisión**: Impracticable para Mundial por falta de espacio mecánico y tiempo de desarrollo. **Objetivo prioritario para Roboliga 2026**.

---

## L7 — Solo una cámara frontal

**Severidad**: MEDIA

**Descripción**: Tenemos una sola OpenMV H7 mirando hacia adelante. El campo de visión es limitado (~70° con lente estándar). No vemos nada de lo que pasa a los costados o atrás.

**Impacto en competencia**:
- Cuando la pelota está atrás del robot, hay que girar completamente para encontrarla (perdemos tiempo)
- No vemos oponentes que se acercan por los costados
- El arquero no ve la pelota si viene desde un ángulo lateral

**Opciones**:
- **Lente wide-angle (140°)**: Mejora inmediata sin hardware adicional. Con 3 cámaras de 140° se cubren 360°.
- **Cámara omnidireccional con espejo cónico**: Una sola cámara mirando arriba con espejo cónico da 360°. Requiere calibración compleja y procesamiento de imagen deformada.
- **Múltiples cámaras**: 3-4 OpenMV con procesamiento distribuido.

**Decisión recomendada**: Para Mundial: conseguir lente wide-angle para la OpenMV actual (mejora el FOV de 70° a ~140° con una sola compra). Para Roboliga: evaluar configuración de 3 cámaras o espejo cónico.

---

## L8 — Motores sobrealimentados (5V alimentados a 7.4V)

**Severidad**: MEDIA (confiabilidad)

**Descripción**: Los motores TT están diseñados para 5V pero se alimentan a 7.4V (batería LiPo 2S). Funcionan con buena potencia pero se queman con el uso prolongado.

**Impacto**: Pérdida de motores durante o antes de la competencia. Riesgo de fallo mecánico en pleno partido.

**Solución inmediata**: Llevar motores de repuesto. Limitar PWM máximo por software para reducir corriente.

**Solución a futuro**: Migrar a motores adecuados para 7.4V, idealmente brushless con encoders y reductores.

---

## Resumen de prioridades

| ID | Limitación | Severidad | Para Mundial | Para Roboliga |
|----|-----------|-----------|-------------|---------------|
| L1 | Módulo árbitros | BLOQUEANTE | Obligatorio | Obligatorio |
| L2 | Sin encoders | Alta | PD giróscopo (ya en staging) | Motores brushless |
| L3 | Sin detección oponentes | Alta | TOF frontales | LIDAR/array TOF |
| L6 | Sin kicker/dribbler | Alta | No viable | Prioritario |
| L5 | Sin posicionamiento | Media-Alta | Estimación por cámara | Placa TOF |
| L7 | Una sola cámara | Media | Lente wide-angle | Multi-cámara |
| L8 | Motores quemados | Media | Repuestos + limitar PWM | Motores nuevos |
| L4 | 3 sensores de línea | Media | Corregir bugs existentes | Placa anillo |
