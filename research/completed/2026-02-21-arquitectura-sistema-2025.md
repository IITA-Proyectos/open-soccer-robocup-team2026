# Arquitectura del Sistema Robótico — Temporada 2025

## Análisis Forense Completo del Software, Hardware y Comunicaciones

**Proyecto**: RoboCupJunior Soccer Vision — Equipo IITA Salta  
**Documento**: Auditoría técnica del sistema heredado (legacy 2025)  
**Autor**: Claude (Anthropic — Claude Opus 4.6) bajo supervisión de Gustavo Viollaz  
**Fecha**: 21 de febrero de 2026  
**Clasificación**: Documento interno de ingeniería

---

## 1. Resumen Ejecutivo

El sistema robótico de la temporada 2025 está compuesto por dos robots (delantero y arquero) construidos sobre la placa Zircon (Teensy 4.1) con visión por cámara OpenMV H7. Cada robot tiene 3 motores DC en configuración omnidireccional (120°), sensores de línea, giroscopio BNO055, y comunicación UART con la cámara.

**El sistema ganó el campeonato nacional 2025**, pero el análisis revela **23 deficiencias técnicas** que van desde bugs críticos que causan comportamiento errático hasta limitaciones arquitecturales que impiden escalar a nivel internacional. Este documento cataloga cada una, evalúa su severidad, y propone soluciones concretas para la temporada 2026.

---

## 2. Diagrama de Arquitectura del Sistema

```
┌─────────────────────────────────────────────────────────┐
│                    ROBOT (Delantero o Arquero)           │
│                                                         │
│  ┌──────────────┐     UART 19200 baud     ┌──────────┐ │
│  │   Teensy 4.1  │◄──────────────────────►│ OpenMV   │ │
│  │   (Zircon PCB)│  Serial1 (pins 0,1)    │ H7       │ │
│  │               │  Protocolo: headers +   │          │ │
│  │  ┌─────────┐  │  bytes escalados        │ Cámara   │ │
│  │  │ zirconLib│  │                        │ RGB565   │ │
│  │  │  .cpp/.h │  │                        │ QVGA     │ │
│  │  └─────────┘  │                        │ 320×240  │ │
│  │               │                        └──────────┘ │
│  │  Periféricos: │                                     │
│  │  ├─ 3 motores DC (omnidireccional 120°)             │
│  │  ├─ 3 sensores de línea (analógicos)                │
│  │  ├─ BNO055 giroscopio (I2C) [DESHABILITADO]        │
│  │  ├─ HC-SR04 ultrasonido [SOLO ARQUERO]              │
│  │  ├─ 8 sensores IR de pelota [NO USADOS EN CÓDIGO]  │
│  │  ├─ Dribbler (motor DC separado)                    │
│  │  └─ 2 botones de usuario                            │
│  └──────────────┘                                      │
└─────────────────────────────────────────────────────────┘
```

---

## 3. Inventario de Sensores

### 3.1 Sensores de Línea (Reflectancia)

**Cantidad**: 3 por robot (izquierdo, centro, derecho)  
**Tipo**: Analógico (lectura ADC 0–1023)  
**Interfaz**: `readLine(1)`, `readLine(2)`, `readLine(3)` vía zirconLib  
**Pines**: Mark1: A11, A13, A12 | Naveen1: A8, A9, A12

**Umbrales calibrados** (del código del arquero):

| Color | Sensor 1 (izq) | Sensor 2 (centro) | Sensor 3 (der) |
|-------|----------------|-------------------|-----------------|
| Blanco | 575–753 | 494–762 | 590–754 |
| Verde | 210–340 | 370–467 | 254–342 |
| Negro | 174–227 | 418–422 | 234–269 |

**En el código final de competencia** (`domingo 12-10`), se simplificó a un único umbral: `blanco = 710`.

**Problema**: Umbrales hardcodeados. Cada cancha, iluminación y superficie produce valores distintos.

### 3.2 Giroscopio BNO055

**Chip**: Bosch BNO055 (IMU 9-DOF con fusión de sensores integrada)  
**Interfaz**: I2C, dirección 0x28  
**Librería**: Adafruit_BNO055 + Adafruit_Sensor  
**Dato usado**: `orientation.x` (yaw, 0°–360°)

**Estado en competencia**: **DESHABILITADO** — Todo el código del giroscopio en el delantero final está comentado. El robot compitió sin corrección de orientación. En `lateral_con_giróscopo` existe un controlador proporcional funcional (Kp=0.3) con compensación de wrap-around ±180°, pero nunca se integró al código final.

**En zirconLib**: `readCompass()` requiere `compassCalibrated == true`, pero no hay código que la active — `CalibrateCompass()` está comentada en el header.

### 3.3 Sensor Ultrasónico HC-SR04

**Cantidad**: 1 (solo en el arquero)  
**Pines**: TRIG=9, ECHO=10

**Problema**: `pulseIn()` es **bloqueante** — detiene toda ejecución hasta 1 segundo. Pin ECHO=10 colisiona con `buttonpin2` en Mark1.

### 3.4 Sensores IR de Pelota

**Cantidad**: 8 por robot  
**Interfaz**: Analógico, invertido: `1024 - analogRead(pin)`  
**Pines**: Mark1: 14,15,16,17,20,21,22,23 | Naveen1: 20,21,14,15,16,17,18,19

**Estado**: `readBall()` existe en zirconLib pero **no se usa en ningún código de competencia**. 8 sensores de hardware desperdiciados.

### 3.5 Botones de Usuario

**Cantidad**: 2 por robot  
**Sin evidencia de uso** en código de competencia.

---

## 4. Sistema de Visión (OpenMV H7)

### 4.1 Configuración de Cámara

| Parámetro | Valor |
|-----------|-------|
| Resolución | QVGA (320×240 píxeles) |
| Formato | RGB565 |
| Balance de blancos | Deshabilitado |
| Auto-ganancia | Deshabilitado |
| FPS estimados | ~15-25 (QVGA + find_blobs×3 colores) |

### 4.2 Detección de Objetos

La cámara detecta 3 objetos por color usando `find_blobs()` en espacio LAB:

| Objeto | Threshold LAB (L,A,B min/max) | Observación |
|--------|-------------------------------|-------------|
| Pelota (naranja) | (76, 18, 13, 88, 6, 127) | Rango muy amplio en B |
| Arco amarillo | (0, 79, -22, -8, 46, 127) | L_min=0 incluye negro |
| Arco azul | (31, 19, -36, 60, -61, 5) | Rango estrecho |

**Problema grave**: Existen **al menos 3 versiones diferentes** de thresholds para naranja en el repositorio, sin documentación de cuál se usó en competencia.

### 4.3 Transformación Homográfica

Convierte coordenadas de píxel (u,v) a centímetros (X,Y) con corrección por altura:

```
X = x * (h - r) / h   donde h=18.7cm (altura cámara), r=2.15cm (radio pelota)
```

**Existen 2 matrices H completamente diferentes** en el repo — indica recalibración sin documentar proceso ni puntos de referencia.

### 4.4 Herramienta de Calibración

`Calibrar_Treshold.py` — herramienta interactiva que toma muestras por UART y calcula thresholds LAB con márgenes de ±1.5×. Requiere USB (no usable en competencia).

---

## 5. Protocolo de Comunicación OpenMV → Teensy

### 5.1 Capa Física

| Parámetro | Valor |
|-----------|-------|
| Interfaz | UART (Serial) |
| Baud rate | 19200 bps |
| OpenMV | UART(3) → P4(TX), P5(RX) |
| Teensy | Serial1 → pin 0(RX1), 1(TX1) |
| Dirección | Unidireccional: OpenMV → Teensy |

### 5.2 Formato del Paquete (versión final, 9 bytes)

```
[201][Xp][Yp] [202][Xam][Yam] [203][Xaz][Yaz]
 └─pelota──┘    └─arco amar.─┘   └─arco azul──┘
```

**Codificación (OpenMV)**: `byteX = int(X * 2)`, `byteY = int((Y + 50) * 2)`  
**Decodificación (Teensy)**: `Xp = codedXp` (directo), `Yp = codedYp - 100`

### 5.3 Bugs Críticos del Protocolo

**BUG #1 — Escala inconsistente X**: OpenMV multiplica ×2, Teensy NO divide ÷2. Distancias son el doble de las reales. `tolerancia_cercania=30` equivale a 15cm reales.

**BUG #2 — Escala inconsistente Y**: OpenMV codifica `(Y+50)*2` (0–200), Teensy decodifica `codedYp - 100` (rango -100..+100 en vez de -50..+50).

**BUG #3 — Sin resincronización**: Si se pierde 1 byte, el receptor lee basura. No hay start-of-frame, checksum, ni mecanismo de recuperación.

**BUG #4 — 0 = no detectado = posición válida**: Cuando no se ve un objeto se envía 0, pero 0 también es una posición real.

**BUG #5 — Sin flow control**: Buffer de 64 bytes en Teensy puede saturarse.

**BUG #6 — Sin heartbeat**: No se sabe si la OpenMV está funcionando.

### 5.4 Evolución del Protocolo

4 versiones incompatibles en el repo, sin documentación de cuál va con cuál:
1. Solo print (sin UART)
2. 8 bytes: X, Y, ángulo, sentido
3. 6 bytes: pelota + 1 arco
4. 9 bytes: pelota + 2 arcos (competencia)

---

## 6. Software Teensy — Delantero (código de competencia)

### 6.1 Máquina de Estados

```
GIRANDO ──► AVANZANDO ──► CENTRANDO ──► PATEANDO_adelante
   ▲            │              │                │
   │       (sin pelota)   (timeout 5s)    PATEANDO_pausa
   │            │              │                │
   │            ▼              ▼          PATEANDO_atras
   │        GIRANDO        GIRANDO              │
   │                                        GIRANDO
   │
   ├── AVANZANDO_POR_TIEMPO (5s sin pelota → avanza 500ms)
   ├── DETECTA_LINEA_1/2/3 (retrocede 400ms según sensor)
```

### 6.2 Problemas Identificados

| # | Severidad | Problema |
|---|-----------|----------|
| 1 | CRÍTICA | Giroscopio deshabilitado — sin corrección de orientación |
| 2 | CRÍTICA | Fall-through: `GIRANDO_TIEMPO` no tiene `break;` → cae a `PATEANDO_adelante` |
| 3 | ALTA | CENTRANDO gira siempre en la misma dirección, ignora posición relativa del arco |
| 4 | ALTA | Movimiento por tiempo (`delay()`) — impreciso con batería variable |
| 5 | ALTA | `avanzar_patear()` usa PWM=250, bypasea zirconLib y su límite de 100 |
| 6 | MEDIA | Detección de línea duplicada en cada estado (9 líneas × 8 estados) |
| 7 | MEDIA | Timeout de 5 segundos excesivo (equipos competitivos reaccionan en <100ms) |

---

## 7. Software Teensy — Arquero

### Estado: BORRADOR NO FUNCIONAL

El código del arquero tiene **múltiples errores de compilación**:
- Variables leídas antes de `setup()` (pines sin inicializar)
- `potencia` no definida, `leerGiroscopio()` no implementada
- Dos bloques `setup()`, variables duplicadas
- Errores de sintaxis: `blanco_s1!` (→ `!blanco_s1`), `}` seguido de `.`

**Conclusión**: Se usó una versión diferente no presente en el repositorio, o el arquero no compitió.

---

## 8. Librería zirconLib

| Función | Estado |
|---------|--------|
| `InitializeZircon()` | ✅ Funcional |
| `motor1/2/3()` | ⚠️ Bypasseada en código final |
| `readLine(1-3)` | ✅ Usado |
| `readBall(1-8)` | ❌ No usado |
| `readCompass()` | ❌ Flag nunca activada |
| `readButton(1-2)` | ❌ No usado |

**Problema de diseño**: Detección de versión PCB por `String` (ineficiente). Array `ballpins[8]` declarado pero nunca llenado. Código final bypasea la librería completamente para control de motores.

---

## 9. Dribbler

Motor DC en pines 8, 9, 10. Activado por serial con `"pelota detectada"` (bloqueante).

**Pines 8/9/10 colisionan** con motor2 (Mark1), botones, y HC-SR04 del arquero. No integrado en código de competencia.

---

## 10. Mapa de Conflictos de Pines

| Pin | Mark1 Motor | Naveen1 | HC-SR04 | Dribbler |
|-----|------------|---------|---------|----------|
| 8 | motor2dir1 | button1 | — | IN1 ⚠️ |
| 9 | button1 | — | TRIG ⚠️ | IN2 ⚠️ |
| 10 | button2 | button2 | ECHO ⚠️ | ENA ⚠️ |

---

## 11. Catálogo de 23 Deficiencias

### CRÍTICAS (6)

| ID | Componente | Deficiencia | Impacto |
|----|-----------|-------------|---------|
| C1 | Protocolo | Codificación ×2 sin decodificación ÷2 | Distancias al doble |
| C2 | Delantero | Giroscopio deshabilitado | Sin corrección de heading |
| C3 | Arquero | Código no compila | Arquero no funcional |
| C4 | Protocolo | Sin resincronización | Datos corruptos por ruido |
| C5 | Delantero | Fall-through GIRANDO_TIEMPO→PATEANDO | Patea sin pelota |
| C6 | Movilidad | `angulo=0` en vez de `==0` | Giro unidireccional |

### ALTAS (8)

| ID | Componente | Deficiencia |
|----|-----------|-------------|
| A1 | Visión | 3 versiones de thresholds sin documentar |
| A2 | Delantero | CENTRANDO gira en una sola dirección |
| A3 | Protocolo | 0 = no detectado = posición válida |
| A4 | General | Movimiento por tiempo (delay) |
| A5 | General | Código bypasea zirconLib |
| A6 | Dribbler | No integrado en competencia |
| A7 | Sensores | 8 IR sin usar |
| A8 | Arquero | pulseIn() bloqueante |

### MEDIAS (9)

| ID | Componente | Deficiencia |
|----|-----------|-------------|
| M1 | zirconLib | String para versión PCB |
| M2 | zirconLib | readCompass() inutilizable |
| M3 | Homografía | 2 matrices sin documentación |
| M4 | General | Sin logging en competencia |
| M5 | General | Detección de línea duplicada ×8 |
| M6 | Pines | Conflictos pin 8/9/10 |
| M7 | OpenMV | Sin delay entre envíos UART |
| M8 | Protocolo | Sin heartbeat bidireccional |
| M9 | General | Múltiples versiones sin versionado |

---

## 12. Recomendaciones para 2026

### PRIORIDAD INMEDIATA

**R1. Habilitar giroscopio BNO055** — Corrección de heading con PID. El código base ya existe. Solo software, impacto enorme.

**R2. Rediseñar protocolo UART** — Propuesta: `[0xAA][longitud][tipo][datos...][checksum]` con 0xFF para "no detectado".

**R3. Verificar ball-capturing zone 1.5cm** — Medir dribbler actual. Reglamento 2026 reduce de 3.0 a 1.5cm.

### PRIORIDAD ALTA

**R4. Refactorizar máquina de estados** — Detección de línea global, agregar break faltante, eliminar duplicación.

**R5. Calibración automática de thresholds** — Guardar en JSON en SD de OpenMV. Rutina con botón al encender.

**R6. Integrar sensores IR** — Complemento para detección cercana cuando la pelota sale del FOV de la cámara.

**R7. Integrar Communication Module 2026** — GPIO pin para start/stop + 500mA power.

### PRIORIDAD MEDIA

**R8. Control PID omnidireccional** — Moverse a cualquier ángulo manteniendo heading.

**R9. Nueva librería unificada** — Reemplazar zirconLib con versión por enum, PID integrado, movimiento omnidireccional.

**R10. Comunicación bidireccional** — Teensy envía comandos a OpenMV.

**R11. Documentar calibración de homografía** — Puntos de referencia, fotos, script.

**R12. Logging a SD card** — Datos de sensores y estados para diagnóstico post-partido.

---

## 13. Comparación con Equipos de Referencia

El equipo **ducc** (Singapur, Hwa Chong) tiene: PID de heading automático, protocolo con checksums, 16 sensores IR primarios, cámara secundaria, movimiento omnidireccional verdadero, logging a SD, tests unitarios.

**Ventaja IITA**: Sistema de visión funcional con detección de 3 objetos simultáneos. Muchos equipos no logran esto.

**Brecha**: Giroscopio, comunicación robusta, integración de sensores, modularización de código.

---

## 14. Conclusión

El hardware existente (BNO055, 8 IR, OpenMV H7) **soporta todas las mejoras sin compras adicionales**. El trabajo es principalmente de software e integración. Las 3 prioridades inmediatas (giroscopio, protocolo, ball-zone) deben resolverse antes de cualquier otra cosa.

---

*Análisis forense basado en revisión línea por línea del repositorio [IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025](https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025). Documento generado por Claude (Anthropic — Claude Opus 4.6) bajo supervisión de Gustavo Viollaz (@gviollaz).*
