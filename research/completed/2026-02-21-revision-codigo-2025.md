---
title: "Revisión completa del código 2025"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: true
ai-tool: "Claude (Anthropic)"
status: completed
priority: alta
tags: [software, revision, legado, analisis]
---

# Revisión completa del código - Temporada 2025

## Resumen ejecutivo

Se analizaron todos los archivos de código del repositorio 2025. El proyecto tiene una base funcional pero con problemas de organización, código incompleto y variables no definidas. La máquina de estados del delantero es el código más maduro. La librería zirconLib es sólida y reutilizable.

---

## 1. Librería zirconLib

**Archivos**: `zirconLib.h` + `zirconLib.cpp`

### Lo que funciona bien
- Abstrae 2 versiones de PCB (Mark1 y Naveen1) con auto-detección por pin 32
- API limpia: `motor1(power, direction)`, `readLine(n)`, `readBall(n)`, `readButton(n)`
- Límite de potencia hardcodeado (`motorLimit = 100`) como protección
- Soporta hasta 8 sensores de pelota IR y 3 sensores de línea

### Problemas encontrados
- **Compass deshabilitado**: Todo el código de `CalibrateCompass()` está comentado. `readCompass()` existe pero devuelve 0 si no está calibrado
- **Llave extra al final**: Hay un `}` suelto al final de `zirconLib.cpp` que causaría error de compilación
- **Sin `movilidad.cpp`**: Existe un archivo `movilidad.cpp.txt` separado que parece una librería de alto nivel (avanzar, girar) pero no está integrada
- **Strings para versiones**: Usar `String` de Arduino para comparar versiones de PCB es ineficiente; mejor usar `enum`

### Recomendación para 2026
Reutilizar la base de zirconLib pero: arreglar el bug del `}`, integrar el compass (BNO055), y considerar refactorear con `enum` para versiones.

---

## 2. Robot Delantero - Máquina de estados

**Archivo**: `para que persiga la pelota`

### Arquitectura
Máquina de 4 estados: `GIRANDO` → `AVANZANDO` → `CENTRANDO` → `PATEANDO_adelante`

### Protocolo UART con OpenMV
- 6 bytes: `[201][Xp][Yp][202][Xa][Ya]`
- Headers 201 (pelota) y 202 (arco) para sincronización
- Decodificación: `X = byte / 2.0`, `Y = (byte / 2.0) - 50.0`
- 0 en X/Y significa "no detectado"

### Lo que funciona bien
- La estructura de máquina de estados es clara y extensible
- Decodificación UART robusta con verificación de headers
- Transiciones lógicas entre estados

### Problemas encontrados
- **CENTRANDO usa coordenadas del arco para decidir, pero calcula anguloArco con datos de la pelota**: Línea `anguloRadArco = atan2(decodedYp, decodedXp)` debería ser `atan2(decodedYa, decodedXa)` — **BUG crítico**
- **PATEANDO_adelante tiene lógica invertida**: El `if(millis() - millis_inicio_estado <= 2000)` frena los motores *antes* de los 2 segundos en vez de *después*. Debería ser `>=`
- **Giroscopio comentado**: Todo el código del BNO055 está deshabilitado, el robot no corrige orientación
- **Sin detección de línea**: No hay verificación de línea blanca — el delantero puede salirse de la cancha
- **Velocidades de giro fijas**: `g = 0.4` hardcodeado, no se ajusta según situación
- **Serial1 sync**: Si se pierden bytes UART, el parser se desincroniza (no hay recovery)

### Recomendación para 2026
Corregir los 2 bugs críticos. Agregar estados adicionales (BUSCANDO, DEFENDIENDO, EVITANDO_LINEA). Habilitar giróscopo. Implementar recovery de UART.

---

## 3. Robot Arquero

**Archivo**: `Arquero`

### Arquitectura propuesta
Oscilación lateral en el arco usando: giróscopo (mantener orientación), sensores de línea (no salir del área), ultrasonido (no pasarse del arco en eje Y).

### Problemas encontrados
- **Código incompleto/duplicado**: Hay 2 bloques `setup()`/`loop()` en el mismo archivo. Parece un WIP con múltiples versiones pegadas
- **Variable `potencia` nunca definida**: Se usa en `girar()` pero no existe — **no compila**
- **Variables `s1`, `s2`, `s3` inicializadas en scope global con `readLine()`**: Esto se ejecuta antes de `InitializeZircon()`, por lo que lee pines no inicializados — **BUG**
- **Lógica de direcciones contradictoria**: Mezcla de `blanco_s1!` (sintaxis incorrecta) y variables no definidas (`verde_izq`, `verde_cen`, `verde_der`)
- **Ultrasonido implementado** pero nunca se usa en la lógica de decisión

### Recomendación para 2026
Este código necesita reescribirse casi desde cero, usando la máquina de estados del delantero como modelo. Mantener la idea de oscilación lateral + giróscopo + sensores de línea.

---

## 4. Visión OpenMV

### Scripts analizados
- `calcula coordenadas pelota.py`: Detección por color LAB + transformación homográfica
- `Calibrar_Treshold.py`: Herramienta interactiva de calibración
- `enviar coordenadas pelota(con redondez)`: Agrega filtro por redondez
- `enviar coordenadas 1 arco y pelota` / `2 arcos y pelota`: Diferentes versiones del sender

### Lo que funciona bien
- Detección por `find_blobs()` con thresholds LAB es el approach estándar para OpenMV
- La herramienta de calibración es útil y completa
- El protocolo de 6 bytes es compacto y eficiente

### Problemas encontrados
- **Threshold hardcodeado**: `(30, 60, 20, 60, 10, 50)` en el script de coordenadas — no se usa el calibrador
- **Homografía hardcodeada**: La matriz H fue calibrada para una posición específica de cámara. Si cambia la mecánica hay que recalibrar
- **Sin manejo de obstáculos**: No se detectan otros robots
- **FPS no reportado**: No se mide el impacto de la homografía en performance

### Recomendación para 2026
Crear un archivo de configuración de thresholds separado (no hardcodeado). Medir FPS. Evaluar si la homografía agrega precisión suficiente vs. el costo computacional.

---

## 5. Resumen de bugs críticos

| # | Ubicación | Bug | Severidad |
|---|-----------|-----|-----------|
| 1 | Delantero | `anguloRadArco` usa coordenadas de pelota en vez de arco | **Crítico** |
| 2 | Delantero | PATEANDO: `<=` debería ser `>=` en comparación de tiempo | **Crítico** |
| 3 | Arquero | Variable `potencia` no definida, no compila | **Bloqueante** |
| 4 | Arquero | `readLine()` llamado antes de `InitializeZircon()` | **Crítico** |
| 5 | Arquero | Código duplicado con 2 `setup()`/`loop()` | **Bloqueante** |
| 6 | zirconLib | Llave `}` extra al final del .cpp | **Bloqueante** |

## 6. Resumen de mejoras prioritarias para 2026

| Prioridad | Mejora | Área |
|-----------|--------|------|
| **P0** | Reescribir arquero con máquina de estados limpia | Software |
| **P0** | Corregir bugs críticos del delantero | Software |
| **P0** | Arreglar zirconLib (llave extra, habilitar compass) | Software |
| **P1** | Agregar detección de línea al delantero | Software |
| **P1** | Externalizar thresholds de visión | Software |
| **P1** | Implementar recovery de UART | Software |
| **P2** | Evaluar 4 motores omnidireccional | Hardware |
| **P2** | Evaluar upgrade de motores (N20/GA12) | Hardware |
| **P2** | Agregar detección de robots rivales | Visión |
