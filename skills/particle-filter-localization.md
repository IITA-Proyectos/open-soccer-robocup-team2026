# 📍 Particle Filter Localization (MCL)

## Resumen
Skill para localización global absoluta en el campo usando Monte Carlo Localization (MCL) o Filtros de Partículas. Fusiona odometría (cinemática), IMU (BNO055), visión (líneas blancas de la cancha) y escáner láser LiDAR.

## Cuándo usar este skill
- Cuando el robot se pierde al ser chocado o al levantarlo.
- Para ejecutar "Kickoff set plays" con precisión milimétrica.
- Para posicionamiento táctico (e.g., el arquero alineándose perfectamente con el centro del arco).

## Conceptos Clave del MCL
El filtro de partículas mantiene "N" hipótesis (partículas) de dónde podría estar el robot. Cada partícula tiene un estado `[x, y, theta]`.

1. **Predicción (Motion Model)**:
   Cuando el robot se mueve, se aplica la cinemática a todas las partículas agregando un ruido gaussiano.
2. **Actualización (Sensor Model)**:
   Si el LiDAR lee una pared a 50cm, se evalúa la probabilidad de cada partícula de ver esa pared. Las partículas consistentes con la lectura ganan "peso".
3. **Remuestreo (Resampling)**:
   Las partículas con poco peso desaparecen, y las de alto peso se multiplican.

## Implementación Técnica (LiDAR + OpenCV)

### 1. Detección de Líneas (OpenCV/OpenMV)
Se extraen las líneas blancas (bordes del área, medio campo, etc.). Las distancias a estas líneas sirven como landmarks.

### 2. LiDAR 2D (ej. RPLidar A1/S1)
El LiDAR 2D escanea a 10Hz los bordillos negros del campo (22cm de altura).
*Nota de reglas*: Asegúrate de que los láseres estén dentro del espectro e intensidad permitidos.

### Ejemplo de Estructura (Pseudocódigo)
```cpp
struct Particle {
    float x, y, theta;
    float weight;
};

std::vector<Particle> particles;

void mcl_predict(float delta_x, float delta_y, float delta_theta) {
    for(auto& p : particles) {
        p.x += delta_x + gaussian_noise(0, sigma_x);
        p.y += delta_y + gaussian_noise(0, sigma_y);
        p.theta += delta_theta + gaussian_noise(0, sigma_t);
    }
}

void mcl_update(std::vector<float> lidar_scans) {
    float sum_weights = 0;
    for(auto& p : particles) {
        // Calcular la distancia esperada a la pared desde 'p'
        float expected_scan = calculate_expected_scan(p);
        
        // Ponderar usando PDF Gaussiana
        p.weight = gaussian_pdf(lidar_scans[0], expected_scan, sensor_noise);
        sum_weights += p.weight;
    }
    
    // Normalizar
    for(auto& p : particles) p.weight /= sum_weights;
}
```

## Consideraciones
- Mantener la cantidad de partículas entre 200-500 para CPUs tipo Teensy 4.1 o ESP32.
- Manejar el "Kidnapped Robot Problem" introduciendo partículas aleatorias ocasionales (un ~5% del total).
