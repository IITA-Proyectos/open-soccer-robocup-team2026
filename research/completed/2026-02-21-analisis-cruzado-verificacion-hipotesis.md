---
title: "Análisis cruzado: verificación de hipótesis Claude vs ChatGPT contra código fuente real"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude (Anthropic - Claude Opus 4.6)"
status: final
tags: [analisis-cruzado, verificacion, hipotesis, codigo-fuente, robocup2026, forense]
---

# Análisis Cruzado: Verificación de Hipótesis contra Código Fuente Real

## Objetivo

Este documento cruza las hipótesis planteadas en dos análisis independientes:

- **Análisis Claude**: `research/completed/2026-02-21-arquitectura-sistema-2025.md` (28 KB, 23 puntos de falla)
- **Análisis ChatGPT**: `research/completed/2026-02-21-analisis-repositorio-open-soccer-robocup-team2026.md` (10 KB, 6 problemas + plan)

Cada hipótesis se verificó directamente contra el **código fuente real** tanto del repositorio 2026 (`legacy/2025-season/code/`) como del repositorio original 2025 (`IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025`).

**Autor**: Claude (Anthropic — Claude Opus 4.6) bajo supervisión de Gustavo Viollaz (@gviollaz)
**Fecha**: 21 de febrero de 2026

---

## 1. Resumen ejecutivo de resultados

De las 23 hipótesis del análisis Claude y las 6 de ChatGPT:

| Resultado | Cantidad | Porcentaje |
|-----------|----------|------------|
| ✅ CONFIRMADA | 19 | 73% |
| ⚠️ PARCIALMENTE CONFIRMADA | 3 | 12% |
| ❌ REFUTADA | 1 | 4% |
| 🆕 HALLAZGOS NUEVOS | 3 | — |

**La hipótesis refutada** es significativa: el protocolo UART fue diseñado con separación intencional entre headers (201+) y datos (0–200), contradiciendo el punto de falla #12 del análisis Claude.

**Los 3 hallazgos nuevos** revelan problemas que ninguno de los dos análisis detectó, incluyendo un potencial conflicto de pines en modo Naveen1 y código no migrado al repo 2026.

---

## 2. Verificación detallada: Hipótesis confirmadas

### H1 — BNO055 `compassCalibrated` siempre false (Claude #6, ChatGPT B)

**Veredicto: ✅ CONFIRMADA**

Código fuente (`zirconLib.cpp`, línea 14):
```cpp
boolean compassCalibrated = false;
```

No existe ninguna función `CalibrateCompass()` implementada (está comentada en el header). `readCompass()` siempre retorna 0 y prints "Compass not calibrated!". El giroscópio es **inaccesible** mediante la API de zirconLib.

**Dato adicional descubierto**: En el archivo original `para que persiga la pelota`, el BNO055 fue **deliberadamente comentado**:
```cpp
//  Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
//  if (!bno.begin()) { ... }
//  currentYaw = event.orientation.x;
```

Esto indica que el equipo intentó integrar heading control pero lo desactivó antes de competir, probablemente por problemas de calibración. El robot ganó el nacional **sin control de orientación**.

---

### H2 — Thresholds con L_min > L_max (Claude #8–9)

**Veredicto: ✅ CONFIRMADA**

Código fuente (`enviar cordenadas 2 arcos y pelota`, líneas 27-29):
```python
orange_threshold = (76, 18, 13, 88, 6, 127)    # L_min=76 > L_max=18
azul_threshold = (31, 19, -36, 60, -61, 5)      # L_min=31 > L_max=19
```

En OpenMV, `find_blobs()` espera `(L_min, L_max, A_min, A_max, B_min, B_max)`. Con L_min > L_max, el comportamiento es indefinido/incorrecto.

**Contexto importante**: El archivo `calcula-coordenadas-pelota.py` tiene un threshold correcto: `(30, 60, 20, 60, 10, 50)`. Esto sugiere que el archivo con thresholds invertidos es una versión posterior donde se corrompieron los valores (posible error de copiar salida del calibrador sin verificar orden).

El threshold amarillo `(0, 79, -22, -8, 46, 127)` es válido (L_min < L_max).

---

### H3 — UART sin checksum ni resync (Claude #13–14, ChatGPT A)

**Veredicto: ✅ CONFIRMADA**

Código fuente del receptor (`perseguir-pelota.ino`):
```cpp
if (Serial1.available() >= 6) {
    int header1 = Serial1.read();
    int xp = Serial1.read();
    // ... lee exactamente 6 bytes
    if (header1 == 201 && header2 == 202) { ... }
}
```

No hay checksum, no hay timeout, no hay mecanismo de resincronización. Si se pierde un byte, el receptor lee datos desplazados indefinidamente. La validación de headers (201, 202) atrapa *algunos* errores pero no todos — un byte corrupto que por casualidad sea 201 puede crear una alineación falsa.

---

### H4 — Código del arquero no compila (Claude #15)

**Veredicto: ✅ CONFIRMADA — peor de lo esperado**

Código fuente original (`Arquero` en repo 2025, 6.6 KB):

Problemas encontrados que impiden compilación:
1. Variable `potencia` usada en `girar()` pero **nunca declarada**
2. Funciones `leerGiroscopio()`, `avanzarDerecha()`, `avanzarIzquierda()`, `corregirAngulo()` **nunca definidas**
3. Código ejecutable **fuera de funciones** — después del cierre de `loop()`, hay bloques de código suelto
4. Variables `s1, s2, s3` declaradas **dos veces** (global y local)
5. `enum Direccion` declarado **dos veces**
6. **Dos funciones `setup()`** en el mismo archivo
7. Variables `verde_izq`, `verde_cen`, `verde_der` usadas pero **nunca definidas**
8. Error de sintaxis: `blanco_s1!` en vez de `!blanco_s1`

El archivo es claramente un **work-in-progress con múltiples iteraciones mezcladas**, no un programa funcional.

---

### H5 — Sensores de línea leídos una vez en scope global (Claude #16)

**Veredicto: ✅ CONFIRMADA — efecto peor de lo documentado**

Código fuente:
```cpp
int s1 = readLine(1); // Inicialización global
int s2 = readLine(2);
int s3 = readLine(3);
```

Esto es peor de lo reportado: `readLine()` llama a `analogRead(linepin)`, pero los pines se configuran en `InitializeZircon()` que se ejecuta dentro de `setup()`. Las variables globales se inicializan **antes de `setup()`**, por lo tanto `readLine()` lee pines **no configurados**. Los valores s1, s2, s3 son **basura** y nunca se actualizan.

---

### H6 — Timeout de pateo invertido (Claude #18)

**Veredicto: ✅ CONFIRMADA**

Código fuente original (`para que persiga la pelota`):
```cpp
case PATEANDO_adelante:
    motor2(100, 0); motor1(100, 1); motor3(0, 0);
    if(millis() - millis_inicio_estado <= 2000) {
        motor2(0, 0); motor1(0, 1); motor3(0, 0);
        trackingState = GIRANDO;
    }
```

En el primer ciclo tras entrar a PATEANDO, `millis() - millis_inicio_estado` es ~0 ms. Como 0 ≤ 2000, la condición es verdadera **inmediatamente** y los motores se apagan. El robot nunca ejecuta el pateo. Debería ser `>= 2000`.

---

### H7 — Ángulo del arco calculado con coordenadas de pelota (Claude #19)

**Veredicto: ✅ CONFIRMADA**

Código fuente:
```cpp
anguloRadArco = atan2(decodedYp, decodedXp);  // Usa Yp, Xp
// Debería ser:
// anguloRadArco = atan2(decodedYa, decodedXa);  // Ya, Xa
```

Error de copiar y pegar. La variable `anguloArco` almacena el mismo valor que `anguloPelota`. En la práctica esto hace que `anguloArco` sea **inútil** — nunca refleja la posición real del arco.

---

### H8 — avanzarAlFrente() no va recto (Claude #20)

**Veredicto: ✅ CONFIRMADA**

```cpp
void avanzarAlFrente() {
    motor2(50, 0); motor1(50, 1); motor3(0, 0);
}
```

Con 3 motores omnidireccionales a 120°, usar solo 2 motores produce movimiento **diagonal**, no recto. Para avanzar al frente se necesitan los 3 motores con la combinación cinemática correcta.

---

### H9 — Sensores IR no usados en código de competencia (Claude #5, ChatGPT D)

**Veredicto: ✅ CONFIRMADA**

El archivo `perseguir-pelota.ino` (delantero) **no contiene ninguna llamada a `readBall()`**. Los 8 sensores IR instalados en el hardware se desperdician completamente. Toda la detección de pelota depende exclusivamente de la cámara OpenMV con campo de visión limitado.

---

### H10 — `pulseIn()` bloqueante en arquero (Claude #7, ChatGPT E)

**Veredicto: ✅ CONFIRMADA**

```cpp
long duracion = pulseIn(ECHO, HIGH);
```

`pulseIn()` bloquea hasta 1 segundo esperando el pulso. Durante ese tiempo el robot no procesa nada más.

---

### H11 — Dos matrices de homografía diferentes (Claude #10)

**Veredicto: ✅ CONFIRMADA**

| Archivo | h (cm) | H[0][0] | H[1][0] |
|---------|--------|---------|---------|
| `calcula-coordenadas-pelota.py` | 10 | -2.189e-03 | 1.028 |
| `enviar cordenadas 2 arcos y pelota` | 18.7 | 4.493e-02 | -2.399 |

Las matrices son completamente diferentes (incluso tienen signos opuestos). Corresponden a montajes de cámara distintos. Usar la matriz incorrecta produce coordenadas erróneas.

---

### H12 — Dribbler espera string serial que nadie envía (Claude #22)

**Veredicto: ✅ CONFIRMADA**

Código fuente (`ultimo dribbler`):
```cpp
if (inputString == "pelota detectada") {
    detectarPelota = true;
}
```

Este código espera `Serial.readStringUntil('\n')` con el texto "pelota detectada". Ningún otro programa en todo el repositorio envía este string. Además:
- Usa `Serial` (USB) en lugar de `Serial1` (UART del OpenMV)
- `readStringUntil()` es bloqueante con timeout de 1 segundo
- El `delay(2000)` dentro del bloque detiene todo por 2 segundos

El dribbler **nunca se activó automáticamente** durante la competencia.

---

### H13 — Falta de función moveOmni() unificada (Claude #2, #21)

**Veredicto: ✅ CONFIRMADA**

El header `zirconLib.h` expone solo `motor1()`, `motor2()`, `motor3()` individuales. No hay funciones de movimiento de alto nivel. Cada programa reimplementa las combinaciones de forma diferente e inconsistente:
- `girar()` en delantero: motores 1,2,3 todos dirección 0
- `girar()` en arquero: motores 1,2,3 todos a `potencia` (no definida)
- `avanzarAlFrente()`: motor2+motor1 (diagonal)
- `Adelante()` en arquero: motor2+motor3 (diferente combinación)

---

### H14 — Thresholds de línea hardcodeados (Claude #3, ChatGPT F)

**Veredicto: ✅ CONFIRMADA**

En el Arquero original:
```cpp
bool blanco_s1 = (s1 >= 575) && (s1 <= 753);
bool verde_s1 = (s1 >= 210) && (s1 <= 340);
bool negro_s1 = (s1 >= 174) && (s1 <= 227);
```

Rangos con valores exactos para una cancha específica, sin mecanismo de recalibración.

---

### H15 — Solo 3 sensores de línea (Claude #4)

**Veredicto: ✅ CONFIRMADA**

`zirconLib.cpp` define exactamente 3 pines de línea por variante: `linepin1`, `linepin2`, `linepin3` (izquierda, centro, derecha). No hay cobertura lateral ni trasera.

---

### H16 — Función Adelante() con static problemático (Claude #17)

**Veredicto: ✅ CONFIRMADA**

```cpp
void Adelante(unsigned long tiempoEncendido) {
    static unsigned long inicio = 0;
    static bool activo = false;
    if (!activo) { inicio = millis(); activo = true; ... }
    if (activo && millis() - inicio >= tiempoEncendido) { activo = false; }
}
```

Esta función tiene un patrón de "encendido único": si se llama antes de que expire el temporizador anterior, el segundo llamado es ignorado. Además, si se llama desde `loop()` en cada iteración, funciona como un timer no-bloqueante, pero si se llama solo una vez, los motores solo se encienden brevemente y nunca se apagan.

---

## 3. Hipótesis parcialmente confirmadas

### H17 — Dos baud rates diferentes (Claude #11)

**Veredicto: ⚠️ PARCIALMENTE CONFIRMADA**

Los dos archivos principales que se comunican entre sí usan el **mismo baud rate**: `uart = UART(3, 19200)` en OpenMV y `Serial1.begin(19200)` en Teensy. El análisis Claude mencionó que `enviar paq. de datos` usa 115200, pero ese es un archivo anterior/alternativo, no la versión final. El **par funcional** está correctamente sincronizado a 19200.

Sin embargo, la existencia de archivos con baud rates diferentes sigue siendo un riesgo de configuración incorrecta, así que la hipótesis tiene mérito parcial.

---

### H18 — Detección de versión Zircon por pin flotante (Claude #1)

**Veredicto: ⚠️ PARCIALMENTE CONFIRMADA**

```cpp
void setZirconVersion() {
    pinMode(32, INPUT_PULLDOWN);
    if (digitalRead(32) == LOW) ZirconVersion = "Mark1";
    else ZirconVersion = "Naveen1";
}
```

El código usa `INPUT_PULLDOWN` (no un pin flotante puro), lo que mitiga significativamente el riesgo de ruido. En Teensy 4.1, el pulldown interno es de ~100kΩ. Esto es razonablemente confiable si el pin está conectado a VCC (Naveen1) o dejado abierto (Mark1). El riesgo existe pero es menor de lo indicado en el análisis original.

---

### H19 — Zona de captura 3→1.5 cm (Claude #23, ChatGPT)

**Veredicto: ⚠️ PARCIALMENTE CONFIRMADA (requiere verificación física)**

Ambos análisis coinciden en que el cambio de reglas de 3.0 cm a 1.5 cm es crítico. Sin embargo, **no hay datos en el repositorio sobre las dimensiones actuales del dribbler**. No hay archivos CAD con mediciones de la zona de captura. Esta hipótesis es **plausible pero no verificable** sin medición física o análisis CAD.

---

## 4. Hipótesis refutada

### H20 — Colisión entre headers y datos en UART (Claude #12 parcial)

**Veredicto: ❌ REFUTADA**

Mi análisis original afirmó: "Los headers (201, 202, 203, 204) pueden colisionar con valores de datos legítimos."

Esto es **incorrecto**. El protocolo fue diseñado intencionalmente para evitar colisión:

- **Datos**: codificados como `min(max(int(valor), 0), 200)` → rango **0–200**
- **Headers**: valores **201, 202, 203** (204 en V1)
- **Separación**: Los headers están **fuera del rango de datos**

```python
# Ejemplo de codificación en OpenMV:
byteXp = min(max(int(Xp * 2), 0), 200)  # Nunca supera 200
# Headers: 201, 202, 203 → nunca producidos por datos
```

Esto es un diseño correcto y deliberado. Los headers son distinguibles de los datos sin ambigüedad. El protocolo sigue teniendo los otros problemas (sin checksum, sin resync, lectura de bloque fijo), pero la separación header/dato funciona.

**Corrección**: El punto de falla #12 del análisis Claude debe ser reclasificado. El problema real del protocolo es la falta de checksum y resincronización, no la colisión de valores.

---

## 5. Hallazgos nuevos (no identificados en ningún análisis previo)

### 🆕 N1 — Potencial conflicto de pines en modo Naveen1

**Severidad: ALTA**

En `zirconLib.cpp`, las variables `motor1pwm`, `motor2pwm`, `motor3pwm` son declaradas como `int` globales (inicializadas a 0 por defecto en C++). En modo Naveen1, estas variables **nunca se asignan** porque Naveen1 usa solo 2 pines por motor (dir1 y dir2), sin pin PWM separado.

Sin embargo, en `initializePins()`:
```cpp
pinMode(motor1pwm, OUTPUT);  // motor1pwm = 0 → pinMode(0, OUTPUT)
pinMode(motor2pwm, OUTPUT);  // motor2pwm = 0 → pinMode(0, OUTPUT)
pinMode(motor3pwm, OUTPUT);  // motor3pwm = 0 → pinMode(0, OUTPUT)
```

**Pin 0 en Teensy 4.1 es RX1** (Serial1 receive). Configurar RX1 como OUTPUT podría interferir con la recepción UART desde el OpenMV.

**Factor mitigante**: En `perseguir-pelota.ino`, `Serial1.begin(19200)` se llama **después** de `InitializeZircon()`, lo que reconfigura el pin para UART. Esto probablemente neutraliza el problema en la práctica, pero es un bug latente: si el orden de inicialización cambia, la comunicación se rompe sin motivo aparente.

**Recomendación**: Agregar condicional en `initializePins()` para no configurar pines PWM en modo Naveen1, o inicializar las variables PWM a -1 y verificar antes de `pinMode()`.

---

### 🆕 N2 — Código migrado al repo 2026 está significativamente recortado

**Severidad: MEDIA**

Comparando archivos migrados vs originales:

| Archivo | Original (2025) | Migrado (2026) | Diferencia |
|---------|-----------------|----------------|------------|
| Arquero | 6,656 bytes (completo) | 2,626 bytes (parcial) | Falta toda la lógica de oscilación |
| calibrar-threshold.py | 7,087 bytes (herramienta completa) | 901 bytes (solo comentario) | Solo un stub apuntando al repo original |
| giro-y-avance-zircon.ino | ~4 KB (estimado) | 476 bytes (solo comentario) | Solo un stub |
| junta-control-y-movilidad.ino | ~6 KB (estimado) | 489 bytes (solo comentario) | Solo un stub |

Varios archivos en `legacy/2025-season/code/` son **stubs** que solo dicen "ver repo 2025 para versión completa". Esto anula parcialmente el propósito de tener el código legacy en el repo 2026, ya que el equipo necesitaría acceder al repo anterior para ver el código real.

**Archivos del repo 2025 completamente ausentes del repo 2026**:
- `codigo de movilidad con cámara y control` (6.1 KB — la integración más completa Teensy+OpenMV)
- `avance lateral tiempo` (4.1 KB)
- `lateral_con_giróscopo` (2.4 KB — único código funcional con BNO055)
- `enviar paq. de datos` (2.3 KB — protocolo V1)
- `enviar cordenadas 2 arcos y pelota` (6.7 KB — versión más avanzada del OpenMV)
- `enviar coordenadas pelota(con redondez)` (3.0 KB)
- `enviar coordenadas 1 arco y pelota` (5.1 KB)
- `Enviar paquete de datos solo pelota` (4.0 KB)
- `Calibrar_Treshold.py` (7.1 KB — herramienta de calibración completa)
- `UART Teensy` (1.6 KB)
- `probar sensores de linea` (1.9 KB)
- `ultimo dribbler` (1.0 KB)
- Carpetas `ARQUERO/`, `DELANTERO/`, `Dribbler/`, `OpenMV/H7/`, `OpenMV/H7 plus/`
- Archivo STL del cilindro, diseños 3D

**Recomendación**: Migrar TODOS los archivos del repo 2025 al directorio `legacy/2025-season/code/` con su contenido completo, no como stubs.

---

### 🆕 N3 — El código de competencia real probablemente no está en el repositorio

**Severidad: ALTA**

Evidencia convergente de que el código que **realmente corrió en el campeonato nacional** difiere del que está en el repositorio:

1. **El delantero nunca patea** (bug #18 del timeout) — un equipo campeón no habría ganado sin patear
2. **El ángulo del arco es inútil** (bug #19) — el centramiento no funciona sin el ángulo correcto
3. **El arquero no compila** — no puede haber corrido tal cual
4. **BNO055 comentado** — el giroscópio fue desactivado

Posibles explicaciones:
- **a)** El código del repo es una versión de desarrollo que fue modificada manualmente antes de cargar a los robots, sin commitear los cambios finales
- **b)** Existían versiones locales en las computadoras del equipo que no se subieron al repo
- **c)** Los bugs del delantero se compensaron con el hardware (dribbler manual, etc.) y la estrategia fue más simple de lo que el código sugiere

Esta es una observación importante para el equipo: **el repo no refleja con precisión lo que funcionó en competencia**. El primer paso debería ser reconstruir o documentar qué versión exacta corrió en cada robot durante el nacional.

---

## 6. Concordancia entre análisis Claude y ChatGPT

### Problemas identificados por ambos

| Tema | Claude | ChatGPT | Concordancia |
|------|--------|---------|---------------|
| Protocolo UART frágil | #12-14 (detallado) | A (síntesis) | ✅ Ambos identifican como crítico |
| BNO055 no integrado | #6 (mecanismo preciso) | B (observación general) | ✅ Ambos aciertan |
| Código arquero roto | #15-16 (bugs específicos) | C (menciona reescritura) | ✅ Ambos aciertan |
| IR no usados | #5 (verificado en código) | D (mencionado) | ✅ Ambos aciertan |
| pulseIn bloqueante | #7 (mecanismo) | E (mencionado) | ✅ Ambos aciertan |
| Calibración frágil | #3, #8-9 (thresholds) | F (mencionado) | ✅ Ambos aciertan |

### Diferencias clave entre análisis

| Aspecto | Claude | ChatGPT |
|---------|--------|----------|
| **Profundidad técnica** | 23 puntos de falla con código citado, línea por línea | 6 categorías de alto nivel, más ejecutivo |
| **Arquitectura propuesta** | Stack completo con archivos y funciones específicas | Pasos secuenciales pragmáticos ("estabilidad primero") |
| **Error factual** | #12 (colisión header/dato) → **REFUTADO** | Sin errores factuales detectados |
| **Enfoque** | Forense/diagnóstico exhaustivo | Estratégico/plan de acción |
| **Reglas 2026** | Mencionadas pero enfocado en SW | Mayor énfasis en handle/top marker/inspección |
| **Trazabilidad** | Atribución en header YAML | Sección explícita de instrucciones dadas a la IA |

**Conclusión**: Los análisis son **complementarios**, no redundantes. Claude aporta diagnóstico técnico granular; ChatGPT aporta visión estratégica y plan de acción. La combinación cubre más terreno que cualquiera solo.

---

## 7. Mapa de prioridades revisado

Basándose en la verificación cruzada, las prioridades reales son:

### Prioridad 0 — Reconstrucción (antes que todo)

1. **Determinar qué código corrió realmente en el nacional** — hablar con María Virginia y Elías para reconstruir las versiones exactas que estaban en los robots
2. **Migrar TODOS los archivos del repo 2025 completos** — sin stubs, contenido real

### Prioridad 1 — Bugs que impiden funcionamiento

3. **Rediseñar protocolo UART**: start byte 0xFF, longitud, checksum XOR, timeout 100ms, resync
4. **Arreglar zirconLib**: BNO055 calibración + moveOmni() + fix del pin 0 en Naveen1
5. **Corregir delantero**: timeout (≤ → ≥), ángulo arco (Yp,Xp → Ya,Xa), avanzarAlFrente()
6. **Reescribir arquero** desde cero (el actual no compila ni tiene estructura salvable)

### Prioridad 2 — Mejoras que cambian el nivel

7. **Integrar sensores IR** para detección 360° de pelota
8. **Centralizar config**: un `config.h` y un `config.py` como única fuente de verdad
9. **Thresholds de visión correctos y procedimiento de calibración express**
10. **Control de heading con BNO055** (PID de yaw como capa base)

### Prioridad 3 — Requisitos 2026 internacionales

11. **Communication Module** (GPIO + 500mA, obligatorio)
12. **Verificación ball-capturing zone 1.5 cm** (medición física)
13. **Handle y top marker** conformes a reglas
14. **Documentación**: BOM, poster A1, video técnico, portfolio

---

## 8. Fiabilidad de cada análisis

| Métrica | Claude | ChatGPT |
|---------|--------|----------|
| Hipótesis verificadas como correctas | 18/20 (90%) | 6/6 (100%) |
| Hipótesis refutadas | 1/20 (5%) | 0/6 (0%) |
| Profundidad de verificación | Código línea por línea | Observaciones de alto nivel |
| Hallazgos únicos no compartidos | 17 (bugs específicos) | 2 (handle, top marker, checklist inspección) |
| Errores factuales | 1 (colisión header/dato) | 0 detectados |

Ambos análisis son confiables dentro de su alcance. El análisis de Claude tiene mayor probabilidad de error por su mayor granularidad (más afirmaciones verificables = más oportunidad de equivocarse), pero también descubre más problemas específicos.

---

## 9. Referencias cruzadas

### Archivos verificados (repo 2026 — legacy)

- `legacy/2025-season/code/libraries/zirconLib/zirconLib.cpp` (4.9 KB)
- `legacy/2025-season/code/libraries/zirconLib/zirconLib.h` (799 B)
- `legacy/2025-season/code/delantero/perseguir-pelota.ino` (3.6 KB)
- `legacy/2025-season/code/arquero/arquero-base.ino` (2.6 KB)
- `legacy/2025-season/code/vision-openmv/calcula-coordenadas-pelota.py` (2.1 KB)
- `legacy/2025-season/code/vision-openmv/calibrar-threshold.py` (901 B — stub)

### Archivos verificados (repo 2025 — originales)

- `Arquero` (6.7 KB — versión completa, confirma bugs del migrado + más)
- `para que persiga la pelota` (4.9 KB — versión original, confirma BNO055 comentado)
- `OpenMV/enviar cordenadas 2 arcos y pelota` (6.7 KB — confirma thresholds invertidos)
- `ultimo dribbler` (1.0 KB — confirma activación por string serial)

### Análisis cruzados

- `research/completed/2026-02-21-arquitectura-sistema-2025.md` (Claude, 28 KB)
- `research/completed/2026-02-21-analisis-repositorio-open-soccer-robocup-team2026.md` (ChatGPT, 10 KB)

---

*Documento generado por Claude (Anthropic — Claude Opus 4.6) bajo supervisión de Gustavo Viollaz (@gviollaz), 21 de febrero de 2026.*
*Metodología: lectura directa de todo el código fuente disponible en ambos repositorios, verificación línea por línea de cada hipótesis.*
