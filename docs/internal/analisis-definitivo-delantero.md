---
title: "Análisis del Programa Definitivo del Delantero"
date: 2026-03-20
author: "Claude (Anthropic) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: final
tags: [delantero, analisis, bugs, mejoras, confiabilidad]
---

# Análisis del Programa Definitivo del Delantero

## Archivos analizados

- `software/robot-delantero/definitivo-delantero` (~900 líneas, 33KB)
- `software/vision/enviar coordenadas 2 arcos y pelota` (OpenMV, ~130 líneas)
- `software/libraries/` (zirconLib de referencia)

## Resumen ejecutivo

El programa implementa una máquina de estados completa para un delantero de Soccer Open con ~28 estados que cubren: búsqueda de pelota, apuntado, avance, orbitado (centrando), pateo (corto y largo), evasión de línea, y un modo arquero embebido. Incluye giróscopo BNO055 con corrección proporcional, comunicación UART de 9 bytes con la OpenMV (protocolo 201/202/203), y soporte para 2 robots con configuración por `#define`.

El código es funcional y muestra experiencia de competencia real, pero tiene problemas significativos de confiabilidad, legibilidad y mantenibilidad que conviene resolver antes de Incheon 2026.

---

## 1. ARQUITECTURA Y DISEÑO

### 1.1 Lo positivo

- **Máquina de estados bien pensada**: El flujo GIRANDO → APUNTAR_PELOTA → AVANZANDO → CENTRANDO → PATEANDO es correcto estratégicamente.
- **Giróscopo inicializado correctamente**: A diferencia del legacy, acá `bno.begin()` se llama en `setup()` con `setExtCrystalUse(true)` y se captura `initialYaw` como offset. El problema de los 35° del legacy **está resuelto**.
- **Corrección proporcional de heading**: `error = currentYaw - initialYaw` con normalización ±180° y `kp = 0.3`. Esto es un avance enorme respecto al legacy.
- **Timeouts en todos los estados**: Cada estado tiene escape por timeout, lo que evita que el robot se quede "colgado" en un estado para siempre.
- **Detección de línea en casi todos los estados**: Los sensores de línea se chequean como prioridad alta en la mayoría de los estados.
- **Protocolo UART mejorado**: Pasó de 6 bytes (2 objetos) a 9 bytes (3 objetos: pelota + arco amarillo + arco azul).
- **Rampa de aceleración en pateo**: `avanzar_patear()` incrementa velocidad gradualmente para no perder tracción.
- **Soporte dual robot**: `#define ROBOT1`/`ROBOT2` para manejar diferencias de pinout y calibración entre los dos robots.

### 1.2 Problemas arquitectónicos

- **Archivo monolítico de 900 líneas**: Todo en un solo archivo sin separación por módulos. Hace que sea difícil de leer, debuggear y que dos personas trabajen en paralelo.
- **Delantero y arquero en el mismo programa**: Los estados del arquero (`moverce_izquierda`, `impulso_inicial`, etc.) están mezclados con los del delantero. Debieran ser programas separados o al menos módulos claramente separados.
- **Código de motor duplicado en todos lados**: Cada estado repite `analogWrite`/`digitalWrite` en línea. No hay abstracción de movimiento.

---

## 2. BUGS CRÍTICOS

### BUG 1 — Protocolo UART sin sincronización robusta

```cpp
if (Serial1.available() >= 9) {
    header1 = Serial1.read();
    if (header1 == 201) {
        codedXp = Serial1.read();
        // ... lee 8 bytes más
    }
    else {
        hayarco_azul = false;
        hayarco_amarillo = false;
        haypelota = false;
    }
}
```

**Problema**: Si se pierde un byte o llega basura, la lectura se desincroniza. El código lee `header1`, si no es 201 descarta **un solo byte** y marca todo como no detectado. Pero los bytes restantes quedan en el buffer y el próximo ciclo va a leer datos desfasados. Esto causa:
- Parpadeo de detección (ve/no ve la pelota aleatoriamente)
- Coordenadas corruptas (lee un Y como si fuera un X)
- El robot pierde la pelota cuando la tiene enfrente

**Agravante**: Del lado OpenMV, el `uart.write(bytearray(packet))` no tiene ningún checksum ni byte de fin de paquete. Un byte corrupto o perdido por ruido eléctrico es irrecuperable.

**Solución propuesta**:
```cpp
// Buscar header 201 descartando bytes basura
while (Serial1.available() >= 9) {
    if (Serial1.peek() != 201) {
        Serial1.read(); // descartar byte basura
        continue;
    }
    // Ahora sí leer el paquete completo
    header1 = Serial1.read();
    codedXp = Serial1.read();
    codedYp = Serial1.read();
    header2 = Serial1.read();
    // ... etc
    
    // Validar los 3 headers
    if (header1 == 201 && header2 == 202 && header3 == 203) {
        // Paquete válido, procesar
    } else {
        // Paquete corrupto, descartar
        continue;
    }
    break;
}
```

Mejor aún: agregar un checksum al protocolo (XOR de todos los bytes del payload).

---

### BUG 2 — Variable `velocidadActualPateo` nunca se resetea

```cpp
int velocidadActualPateo = 0;

void avanzar_patear() {
    // Incrementa velocidadActualPateo hasta velocidadFinalPateo (240)
    velocidadActualPateo += pasoPateo;
}
```

`velocidadActualPateo` se incrementa durante la patada pero **nunca se resetea a 0** cuando termina la patada o cuando cambia de estado. Entonces la segunda patada arranca a velocidad máxima inmediatamente (sin rampa). La rampa de aceleración solo funciona la primera vez.

**Solución**: Resetear al entrar a los estados de patada:
```cpp
case PATEANDO_adelante:
    if (first_entry) velocidadActualPateo = 0; // resetear rampa
    avanzar_patear();
```

---

### BUG 3 — `START_BYTE` definido pero nunca usado

```cpp
#define START_BYTE 0xAA;
```

Tiene un punto y coma extra en el `#define` (esto puede causar errores de compilación inesperados si se usa) y además nunca se usa en el código. El protocolo usa 201/202/203 hardcodeados.

**Solución**: Eliminar la línea o reemplazar los magic numbers 201/202/203 por constantes con nombre.

---

### BUG 4 — zirconLib incluida pero bypass completo de sus funciones de motor

```cpp
#include <zirconLib.h>
// ...
void setup() {
    InitializeZircon();
```

Se incluye `zirconLib.h` y se llama `InitializeZircon()`, que configura pines según la versión Mark1/Naveen1. Pero el programa define sus propios pines (`INA1`, `INB1`, `PWM1`, etc.) y controla los motores directamente con `analogWrite`/`digitalWrite`, ignorando por completo las funciones `motor1()`, `motor2()`, `motor3()` de la librería.

**Riesgo**: `InitializeZircon()` llama a `initializePins()` que configura los pines de la librería como OUTPUT. Si los pines de la librería no coinciden con los `#define` del programa, puede haber conflictos de pines.

**Solución**: O usar la librería completamente, o no incluirla y hacer la inicialización propia. No mezclar.

---

### BUG 5 — Errores de ortografía en nombres de estado

```cpp
enum Estado {
    moverce_izquierda, moverce_derecha,
    // ...
};
```

`moverce` debería ser `moverse`. Parece menor pero en un proyecto de equipo dificulta buscar estados y genera confusión.

---

## 3. PROBLEMAS DE CONFIABILIDAD

### R1 — Detección de línea no cubre todos los estados

Los estados del arquero (`moverce_derecha`, `moverce_izquierda`, `impulso_derecha`, `impulso_izquierda`) solo chequean `s1` y `s2` pero no `s3`. Si el sensor derecho detecta blanco, el robot no reacciona.

Además, en los estados `IMPULSO_CENTRANDO_horario` y `IMPULSO_CENTRANDO_antihorario` no se chequean sensores de línea.

**Solución**: Crear una función `chequearLineas()` que se llame al inicio de cada estado y retorne un flag o cambie el estado directamente.

---

### R2 — Estado PATEANDO_atras_arquero sin timeout

```cpp
case PATEANDO_atras_arquero:
    retroceder_patear();
    if ((s1 >= blanco1) or (s2 >= blanco2) or (s3 >= blanco3)) {
        parar();
        estado = moverce_derecha;
    }
break;
```

Este estado retrocede hasta detectar línea blanca. Si los sensores de línea fallan (cable suelto, sensor sucio), **el robot retrocede para siempre**.

**Solución**: Agregar timeout de seguridad (ej: 2000ms).

---

### R3 — Corrección del giróscopo solo se usa en funciones proporcionales del arquero

La variable `error` y `correccion` se calculan en cada loop, pero solo se usan en `aiproporcional()` y `adproporcional()` (funciones del arquero). El delantero calcula el error pero **no lo usa para corregir trayectoria al avanzar**.

`avanzar()` manda potencia fija a los motores:
```cpp
void avanzar() {
    analogWrite(PWM1, 100); // sin corrección
    analogWrite(PWM2, 100); // sin corrección
}
```

**Esto es el problema de "no anda derecho"** — tienen la solución (giróscopo funcionando) pero no la aplican al movimiento recto del delantero.

**Solución**: Crear `avanzarRecto()` que use `error` para compensar:
```cpp
void avanzarRecto(int potBase) {
    int corr = constrain(error * kp_avance, -30, 30);
    analogWrite(PWM1, potBase + corr);
    analogWrite(PWM2, potBase - corr);
    analogWrite(PWM3, 0);
}
```

---

### R4 — Constantes mágicas por todo el código

Hay decenas de valores numéricos sueltos que hacen el código difícil de ajustar:
- `700`, `1000`, `9000`, `500`, `300`, `350`, `400` (tiempos en ms)
- `100`, `150`, `50`, `60`, `180`, `240` (velocidades de motor)
- `30.0`, `50.0`, `15.0` (tolerancias de ángulo)
- `0.3`, `0.4`, `1.8` (factores de velocidad)

**Solución**: Definir todas como `#define` o `const` al inicio del archivo con nombres descriptivos:
```cpp
#define TIMEOUT_GIRANDO_MS 9000
#define TIMEOUT_AVANZANDO_MS 20000
#define VELOCIDAD_GIRO 100
#define TOLERANCIA_APUNTADO_GRADOS 15.0
```

---

### R5 — Uso de `or` en vez de `||`

```cpp
if (s1 >= blanco1 or s2 >= blanco2 or s3 >= blanco3)
```

`or` es un alias de `||` en C++ y funciona, pero no es idiomático en Arduino/C++. Algunos compiladores podrían dar warnings.

---

### R6 — La OpenMV puede enviar valores que coincidan con headers

Si `Xp` vale 201, 202 o 203 (coordenada válida ~201cm), el Teensy puede interpretar un dato como header y desincronizar todo el paquete.

**Solución propuesta**: En la OpenMV, clampear coordenadas a 1-200 (nunca 0 ni 201-203):
```python
def safe_encode(val, min_val=1, max_val=200):
    return max(min_val, min(max_val, int(val)))
```

O mejor: usar un protocolo con byte stuffing o checksum.

---

## 4. LEGIBILIDAD Y ESTILO

### 4.1 Código de motor repetido masivamente

Este patrón aparece más de 30 veces en el archivo:
```cpp
analogWrite(PWM1, 100 * a); digitalWrite(INA1, 0); digitalWrite(INB1, 1);
analogWrite(PWM2, 100 * a); digitalWrite(INA2, 0); digitalWrite(INB2, 1);
analogWrite(PWM3, 100 * a); digitalWrite(INA3, 0); digitalWrite(INB3, 1);
```

**Propuesta**: Crear funciones primitivas limpias:
```cpp
void setMotor(int pwmPin, int inaPin, int inbPin, int velocidad, bool adelante) {
    analogWrite(pwmPin, abs(velocidad));
    digitalWrite(inaPin, adelante ? 1 : 0);
    digitalWrite(inbPin, adelante ? 0 : 1);
}

void moverRobot(int vel1, int vel2, int vel3) {
    setMotor(PWM1, INA1, INB1, vel1, vel1 >= 0);
    setMotor(PWM2, INA2, INB2, vel2, vel2 >= 0);
    setMotor(PWM3, INA3, INB3, vel3, vel3 >= 0);
}

// Uso:
void girar(int velocidad) {
    moverRobot(-velocidad, -velocidad, -velocidad);
}
void avanzar(int velocidad) {
    moverRobot(velocidad, -velocidad, 0);
}
void orbitar_horario(int velocidad) {
    moverRobot(-60*c, -60*c, 180*c);
}
```

Esto reduciría el programa de 900 líneas a ~400 y haría los ajustes de velocidad triviales.

---

### 4.2 Chequeo de línea duplicado ~15 veces

Este bloque se repite en casi todos los estados:
```cpp
if (s1 >= blanco1) { estado = DETECTA_LINEA_1; millis_inicio_estado = millis(); }
if (s2 >= blanco2) { estado = DETECTA_LINEA_2; millis_inicio_estado = millis(); }
if (s3 >= blanco3) { estado = DETECTA_LINEA_3; millis_inicio_estado = millis(); }
```

**Propuesta**: Función al inicio del switch:
```cpp
bool chequearLineas(int s1, int s2, int s3) {
    if (s1 >= blanco1) { estado = DETECTA_LINEA_1; millis_inicio_estado = millis(); return true; }
    if (s2 >= blanco2) { estado = DETECTA_LINEA_2; millis_inicio_estado = millis(); return true; }
    if (s3 >= blanco3) { estado = DETECTA_LINEA_3; millis_inicio_estado = millis(); return true; }
    return false;
}

// En el loop, antes del switch:
if (chequearLineas(s1, s2, s3)) break; // o al inicio de cada case
```

---

### 4.3 APUNTAR_PELOTA duplicado 3 veces

Los estados `APUNTAR_PELOTA`, `APUNTAR_PELOTA_horario` y `APUNTAR_PELOTA_antihorario` son **casi idénticos** (la única diferencia es a qué estado retornan). Se podrían unificar con un parámetro:
```cpp
Estado estado_retorno_apuntar; // se setea antes de entrar al estado
```

---

### 4.4 Nombres de archivo sin extensión

El archivo se llama `definitivo-delantero` sin extensión `.ino`. Arduino IDE no lo va a reconocer como sketch. Debería ser `definitivo-delantero.ino`.

Igualmente, el archivo de visión se llama `enviar coordenadas 2 arcos y pelota` (con espacios, sin extensión `.py`). Debería ser `enviar-coordenadas-2-arcos-y-pelota.py`.

---

## 5. OPORTUNIDADES DE MEJORA DE DESEMPEÑO

### D1 — Orbitado más inteligente

Actualmente el robot orbita en una dirección fija (horario) y solo cambia a antihorario si detecta línea. Un orbitado inteligente elegiría la dirección que minimice la distancia al arco contrincante, usando la posición Y del arco:
```cpp
if (Ycontrincante > Yp) {
    estado = CENTRANDO_horario;    // arco está a la derecha de la pelota
} else {
    estado = CENTRANDO_antihorario; // arco está a la izquierda
}
```

### D2 — Velocidad adaptativa según distancia a pelota

El robot siempre avanza a velocidad fija (100). Podría ir más rápido cuando la pelota está lejos y frenar al acercarse:
```cpp
int velocidad = map(Xp, tolerancia_cercania, 200, 60, 100);
avanzarRecto(velocidad);
```

### D3 — Predicción de pérdida de pelota

Cuando el robot deja de ver la pelota, espera 500ms y vuelve a GIRANDO. Podría recordar la última posición conocida y avanzar hacia allá brevemente antes de girar.

### D4 — El modo arquero podría aprovechar la detección de arco propio

El arquero actualmente se mueve lateralmente y patea cuando la pelota está cerca. Podría usar el arco propio (azul o amarillo) para mantenerse centrado en el arco y ajustar su posición defensiva.

### D5 — La pausa antes de patear es muy larga

```cpp
case PATEANDO_pausa_inicial:
    parar();
    if (millis() - millis_inicio_estado >= 1000) // 1 SEGUNDO parado
```

1 segundo parado antes de patear es una eternidad en competencia. El rival puede robar la pelota. Reducir a 200-300ms o eliminar si la mecánica lo permite.

---

## 6. ANÁLISIS DEL PROGRAMA DE VISIÓN (OpenMV)

### Lo positivo
- Detección de 3 objetos (pelota + 2 arcos) por color en LAB
- Transformación homográfica para obtener coordenadas reales en cm
- Corrección por altura de cámara y radio de pelota
- LEDs indicadores para debug visual

### Problemas
- **Sin checksum en el protocolo UART**: Ya mencionado en BUG 1
- **pixels_threshold muy bajo para pelota (7)**: Puede detectar ruido como pelota
- **Auto white balance**: Se activa al inicio y luego no se desactiva explícitamente. La calibración de color puede cambiar si la iluminación varía
- **Nombre de archivo con espacios**: Dificulta uso en scripts y terminal
- **`print()` en cada frame**: `print("Enviando:", packet)` en cada frame reduce el FPS. Debería estar deshabilitado o condicional

---

## 7. RESUMEN DE PRIORIDADES

| # | Problema | Impacto | Esfuerzo | Categoría |
|---|---------|---------|----------|----------|
| 1 | Protocolo UART sin sincronización | Crítico | Medio | Confiabilidad |
| 2 | `velocidadActualPateo` no se resetea | Crítico | Trivial | Bug |
| 3 | Corrección giróscopo no usada en avanzar() | Crítico | Bajo | Desempeño |
| 4 | Conflicto zirconLib vs pines propios | Alto | Bajo | Bug |
| 5 | Valores UART pueden coincidir con headers | Alto | Bajo | Confiabilidad |
| 6 | PATEANDO_atras_arquero sin timeout | Alto | Trivial | Confiabilidad |
| 7 | Detección de línea incompleta en algunos estados | Alto | Bajo | Confiabilidad |
| 8 | Pausa de 1s antes de patear | Alto | Trivial | Desempeño |
| 9 | Archivo sin extensión .ino | Medio | Trivial | Organización |
| 10 | Código de motor duplicado (~30 veces) | Medio | Medio | Legibilidad |
| 11 | Chequeo de línea duplicado (~15 veces) | Medio | Bajo | Legibilidad |
| 12 | APUNTAR_PELOTA duplicado 3 veces | Medio | Medio | Legibilidad |
| 13 | Constantes mágicas | Medio | Medio | Legibilidad |
| 14 | Delantero y arquero en el mismo archivo | Medio | Alto | Arquitectura |
| 15 | Orbitado sin elegir dirección óptima | Medio | Medio | Desempeño |
| 16 | print() en OpenMV reduce FPS | Bajo | Trivial | Desempeño |

---

## 8. PROPUESTA DE REFACTORING MODULAR

Para 2026, el código debería reorganizarse en módulos:

```
software/robot-delantero/
├── delantero.ino          # setup(), loop() y máquina de estados
├── motores.h / .cpp       # primitivas de movimiento (avanzar, girar, orbitar, patear)
├── sensores.h / .cpp      # lectura de línea, giróscopo, heading
├── comunicacion.h / .cpp  # protocolo UART con OpenMV
├── config.h               # pines, constantes, tolerancias, selección de robot
└── README.md
```

Esto permitiría:
- Que cada miembro del equipo trabaje en un módulo sin conflictos
- Reutilizar `motores.h` y `comunicacion.h` entre delantero y arquero
- Testear módulos individualmente
- Ajustar constantes en un solo lugar (`config.h`)
