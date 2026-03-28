# Análisis: LiDAR, ToF y SLAM para RoboCupJunior Soccer

## ¿Se puede usar? ¿Vale la pena? ¿Qué usan los equipos campeones?

**Autor:** Gustavo Viollaz (IITA Salta) con asistencia de Claude Opus 4.6  
**Fecha:** 2026-03-28  
**Clasificación:** Análisis estratégico de ingeniería

---

## 1. ⚠️ CAMBIO CRÍTICO DE REGLAS 2025 → 2026

### Reglas 2025 (temporada pasada)

> "Components designed to emit IR (e.g. ToF, LiDAR, IR distance sensors, IR LEDs/LASERs etc.) **are not allowed** and tournament organizers will require such devices to be removed or covered up."

Esta prohibición aplicaba a **AMBAS ligas** (Open y Lightweight). IITA no podía usar LiDAR ni ToF en Soccer Open 2025.

### Reglas 2026 (temporada actual)

> "**FOR THE INFRARED LEAGUE ONLY:** Components designed to emit IR (e.g. ToF, LiDAR, IR distance sensors, IR LEDs/LASERs etc.) are not allowed..."

El cambio clave: la restricción ahora aplica **SOLO a la liga Infrared** (ex-Lightweight). La liga **Vision (ex-Open) SÍ PERMITE LiDAR, ToF, y sensores IR** en 2026.

### Implicación para IITA

IITA compite en **Soccer Vision (Open)**. En 2026, por primera vez, **podemos usar LiDAR y ToF legalmente** en el mundial en Incheon, Corea.

**⚠️ VERIFICAR con las reglas nacionales argentinas** (Roboliga), ya que el organizador nacional puede mantener la restricción de 2025. Consultar antes de invertir en hardware.

---

## 2. OPCIONES DE HARDWARE

### 2.1 RPLidar A1M8 (360° LiDAR)

| Parámetro | Valor |
|-----------|-------|
| Rango | 0.15 - 12 m |
| Frecuencia de escaneo | 2-10 Hz (configurable) |
| Resolución angular | ~1° (360 puntos por scan) |
| Precisión | ±3mm (<1.5m), ±3% (>1.5m) |
| Interface | UART (115200 baud) |
| Tamaño | 69.5 × 69.4 × 37mm |
| Peso | ~170g |
| Voltaje | 5V |
| Consumo | ~100mA scan, ~350mA motor |
| Precio | ~$100 USD |
| Láser | Clase 1 (905nm, infrarrojo) |

**Ventajas:** 360° de cobertura, scan completo del campo en 100-200ms, precisión ±3mm, libraries maduras (ROS, Arduino).

**Desventajas:** Grande (69mm cuadrado), pesado (170g), consume ~450mA, requiere espacio libre arriba del robot para girar sin obstrucciones, UART dedicado.

### 2.2 VL53L1X Array (8 sensores ToF)

| Parámetro | Valor |
|-----------|-------|
| Rango | 0.04 - 4 m |
| Frecuencia | 50 Hz por sensor |
| Resolución angular | 45° (8 sensores a 45°) |
| Precisión | ±5mm |
| Interface | I2C (via TCA9548A) |
| Tamaño | 4.9 × 2.5mm cada sensor |
| Peso | ~1g cada uno (~10g total) |
| Voltaje | 2.6-3.5V |
| Consumo | ~20mA por sensor |
| Precio | ~$3 × 8 = $24 USD + mux $2 |
| Láser | Clase 1 (940nm, infrarrojo) |

**Ventajas:** Muy pequeños, livianos, baratos, 8 lecturas simultáneas en continuous mode, ya los tenemos documentados en la skills library.

**Desventajas:** Solo 8 puntos (vs 360 del LiDAR), resolución angular de 45° (puede perder objetos entre sensores), rango máximo 4m (suficiente para campo de 2.4m).

### 2.3 Comparación directa

| Aspecto | RPLidar A1 | 8× VL53L1X |
|---------|:-:|:-:|
| Puntos por scan | **360** | 8 |
| Frecuencia | 5-10 Hz | **40-50 Hz** |
| Precisión posición | **±5mm** | ±15mm (trilateración) |
| SLAM completo | **Sí** | No (solo posición) |
| Detección de obstáculos | **Sí (360°)** | Parcial (8 direcciones) |
| Tamaño/peso | Grande/pesado | **Tiny/liviano** |
| Consumo | 450mA | **160mA** |
| Costo | $100 | **$26** |
| Complejidad software | Alta (SLAM) | **Media (trilateración)** |
| Complejidad hardware | Media (UART) | **Media (I2C mux)** |
| Espacio requerido | Mucho (arriba robot) | **Mínimo (perimetral)** |

---

## 3. SLAM: ¿VALE LA PENA PARA SOCCER?

### ¿Qué es SLAM?

Simultaneous Localization And Mapping: construir un mapa del entorno mientras se localiza el robot dentro del mapa. Usado extensamente en robots de servicio, drones, y autos autónomos.

### ¿Por qué NO tiene sentido SLAM completo para RoboCup Junior Soccer?

1. **El mapa ya es conocido.** El campo de soccer tiene dimensiones fijas (182×243cm). No necesitás "descubrir" el mapa.
2. **El entorno es simple.** Solo hay paredes rectangulares, arcos, y robots rivales. No hay pasillos, habitaciones, ni obstáculos complejos.
3. **SLAM requiere CPU intensivo.** GMapping, Hector SLAM, o Cartographer necesitan Raspberry Pi o superior. Un Teensy no puede correrlo.
4. **La latencia de SLAM es alta.** Un scan completo + procesamiento = 50-200ms. Para un robot que necesita reaccionar en <30ms, es demasiado.
5. **Los obstáculos se mueven.** Los robots rivales se mueven constantemente, invalidando el mapa. SLAM asume entorno mayormente estático.

### ¿Qué SÍ tiene sentido?

**Localización por distancia a paredes conocidas (no SLAM).** Sabemos que las paredes están a distancias fijas. Medir distancia a las paredes + heading del gyro = posición del robot. Esto es **trilateración**, no SLAM.

Con LiDAR se puede hacer esto con más precisión que con ToF array, porque tenés 360 puntos en vez de 8. Pero la pregunta es si esa precisión extra justifica el costo, tamaño, y complejidad.

---

## 4. QUÉ USAN LOS EQUIPOS TOP

### RoboCup Junior Soccer Open (campeones/top 5)

| Equipo | Año | Sensores de distancia | Posicionamiento |
|--------|:--:|---|---|
| PCBWay team (Eindhoven, EU) | 2024 | 80 fotorresistores (líneas) + cámara | Detección de líneas para zona |
| RoBorregos (México) | 2024 | Ultrasonido + cámara OpenMV | Ángulo/distancia por cámara |
| Faten (US, campeón USA) | 2024 | Cámara + IR | Visión + odometría |
| Equipos alemanes top | 2023-24 | Cámara + encoders + gyro | Odometría con gyro |

**Hallazgo: NINGÚN equipo top de Junior Soccer Open usa LiDAR.** La razón principal fue la prohibición en 2025 y anteriores. En 2026 esto cambia.

### RoboCup MSL (Middle Size League, adultos/universidades)

| Equipo | Sensores | Posicionamiento |
|--------|---------|----------------|
| CAMBADA (campeones) | Omnivisión + encoders + compass | Monte Carlo Localization |
| Tech United (campeones) | Omnivisión + laser scanner | Feature-based localization |
| Water (China) | Omnivisión + LiDAR | Particle filter |

En MSL, varios equipos top usan LiDAR para localización (no SLAM). Miden distancias a paredes y arcos, y usan particle filter para estimar posición. Pero estos son robots de 50×50cm con presupuesto de miles de dólares.

### RoboCup SPL (Standard Platform League, NAO robots)

Todos usan la misma plataforma (NAO). Localización basada en visión de líneas del campo + Monte Carlo. Sin LiDAR.

---

## 5. RECOMENDACIÓN PARA IITA 2026

### Opción A: 8× VL53L1X (RECOMENDADA)

**Razones:**
- Ya tenemos los skills y código listos (tof-array-positioning.md)
- Barato ($26), liviano (10g), pequeño
- 40-50 Hz de update (más rápido que LiDAR)
- Suficiente para posicionamiento en campo de 182×243cm
- Combinado con gyro y odometría, da ±15mm de precisión
- No requiere CPU adicional (corre en Teensy)
- El sistema de parallel sensing ya lo soporta

**Limitaciones:**
- Solo 8 direcciones (puede no detectar robot rival entre sensores)
- No hace "scan" del entorno completo

### Opción B: RPLidar A1 (SOLO si hay presupuesto y espacio)

**Razones para considerar:**
- 360° de detección = puede detectar robots rivales
- ±3mm de precisión en localización
- Mapeo de obstáculos dinámicos (rivales)

**Razones para NO hacerlo en 2026:**
- Grande y pesado para robot de 22cm
- $100 USD (caro para equipo argentino)
- Requiere montaje arriba del robot con espacio libre
- Software más complejo
- Consumo alto (450mA) = baterías más grandes
- **Nunca fue testeado por ningún equipo Junior Soccer** = riesgo alto

### Opción C: Híbrida (FUTURO, 2027)

Para temporada 2027, después de ganar experiencia con ToF en 2026:
- RPLidar arriba del robot para detección de rivales + localización precisa
- ToF array a nivel bajo para complementar
- Teensy como controller, RPi Zero para procesar scans del LiDAR

---

## 6. LOCALIZACIÓN POR LIDAR SIN SLAM

Si en el futuro se decide usar LiDAR, el approach NO es SLAM sino **known-map localization**:

```
1. Tenemos el mapa del campo (rectangular, dimensiones conocidas)
2. LiDAR escanea 360°, obtiene ~360 puntos de distancia
3. Algoritmo busca las 4 paredes en el scan
   (line fitting con RANSAC o Hough transform)
4. De las paredes detectadas, calcular posición del robot
   - Distancia a pared izquierda/derecha → coordenada X
   - Distancia a pared fondo/frente → coordenada Y
   - Ángulo de las líneas de pared → heading (verificar con gyro)
5. Opcional: detectar arcos como features adicionales
6. Fusionar con odometría vía EKF
```

Esto es MUCHO más simple que SLAM y no necesita CPU potente.

### Detección de robots rivales con LiDAR

Beneficio extra del LiDAR que el ToF array no puede dar fácilmente:

```
1. Después de extraer las paredes del scan
2. Los puntos que NO corresponden a paredes = obstáculos
3. Agrupar puntos cercanos (clustering)
4. Cada cluster = un robot rival (o nuestro compañero)
5. Estimar posición de cada obstáculo
6. Usar para path planning (evitar rivales)
```

---

## 7. REGLAS NACIONALES: VERIFICAR

Las reglas internacionales 2026 permiten ToF/LiDAR en Soccer Vision. Pero la Roboliga Argentina puede tener reglas diferentes.

**ACCIÓN REQUERIDA:** Consultar con el organizador nacional (Roboliga) si las reglas nacionales 2026 permiten sensores IR/ToF/LiDAR en Soccer Open, o si mantienen la restricción de 2025.

Si las reglas nacionales permiten:
→ Implementar ToF array para el nacional
→ Llevar el sistema probado a Incheon

Si las reglas nacionales NO permiten:
→ Desarrollar con ToF para prácticas
→ Solo activar en Incheon (donde SÍ se permite)
→ Tener software que funcione CON y SIN posicionamiento ToF

---

## FUENTES

- RoboCupJunior Soccer Rules 2025: prohibición de IR/ToF/LiDAR en ambas ligas
- RoboCupJunior Soccer Rules 2026: restricción SOLO para Infrared league
- RoboCupJunior Forum: 2026 rules discussion
- PCBWay RCJ team: 80 photoresistors, no LiDAR
- RoBorregos: robocup-soccer-open-2024, ultrasound + OpenMV
- CAMBADA MSL: omnivision + Monte Carlo Localization
- Tech United MSL: laser scanner for feature-based localization
- RPLidar A1 datasheet (SLAMTEC)
- VL53L1X datasheet (ST)
- IITA skills library: tof-array-positioning, parallel-sensing
