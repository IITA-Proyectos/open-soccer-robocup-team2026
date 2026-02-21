---
title: "Mapa mental NotebookLM → Roadmap técnico aplicado al repositorio (Soccer Vision 2026)"
date: 2026-02-21
author: "ChatGPT (OpenAI - GPT-5.2 Pro)"
ai-assisted: true
ai-tool: "ChatGPT (OpenAI - GPT-5.2 Pro)"
supervision: "Gustavo Viollaz (@gviollaz)"
status: completed
priority: alta
tags: [roadmap, notebooklm, vision, control, comunicacion, estrategia, hardware, robocup, soccer-vision, testing]
---

# Mapa mental NotebookLM → Roadmap técnico aplicado al repositorio (Soccer Vision 2026)

## Propósito

Este documento convierte el **mapa mental de NotebookLM** ("Tecnología de Visión y Robótica en Fútbol") en un **marco de trabajo utilizable dentro del repositorio** `open-soccer-robocup-team2026`:

- como **taxonomía** (qué temas existen y cómo se relacionan),
- como **matriz de trazabilidad** (cada tema → carpetas/código/docs/tests del repo),
- como **checklist de ingeniería** (qué falta, qué se prioriza, cómo se valida en cancha).

La idea no es "decoración académica": es reducir el caos típico de robótica competitiva ("anda en mi mesa") y convertirlo en ingeniería reproducible ("anda en cancha, con logs y tests").

---

## Artefactos visuales de NotebookLM

Capturas del mapa mental original:

- [`notebooklm-mindmap-resumen.png`](../references/notebooklm/notebooklm-mindmap-resumen.png) — Vista general (5 ramas principales)
- [`notebooklm-mindmap-detalle-1.png`](../references/notebooklm/notebooklm-mindmap-detalle-1.png) — Detalle con sub-ramas
- [`notebooklm-mindmap-detalle-2.png`](../references/notebooklm/notebooklm-mindmap-detalle-2.png) — Detalle alternativo

Ver [README de la carpeta](../references/notebooklm/README.md) para más contexto sobre el origen.

> Nota: este documento **no depende** de las imágenes para ser útil, pero las imágenes ayudan como "mapa visual" en reuniones.

---

## Cómo usar este documento (modo práctico)

**Regla 1 — si algo cambia, tiene que quedar trazado.**  
Cuando se agregue o modifique un subsistema (visión, protocolo, control, mecánica), actualizar:

1. el/los archivos reales en `software/` / `hardware/`,
2. el protocolo o README correspondiente,
3. el test/protocolo en `testing/`,
4. y una nota corta en `journal/`.

**Regla 2 — un tema del mapa mental = un conjunto de artefactos.**  
Cada hoja del mapa mental debería terminar con:
- una implementación (o stub) en `software/`,
- un doc de diseño (en `docs/` o `research/`),
- y un protocolo de validación en `testing/`.

---

## Taxonomía del mapa mental (versión textual)

Tema central: **Tecnología de Visión y Robótica en Fútbol**

1) **Detección y Seguimiento**
- Algoritmos de IA  
- Métodos tradicionales  
- Objetos clave  

2) **Hardware y Plataformas**
- Computadoras integradas  
- Sensores  

3) **Localización y Navegación**
- Fusión de sensores  
- Técnicas espaciales  

4) **Sistemas de Control y Comms**
- Protocolos de comunicación  
- Actuadores y movimiento  

5) **Estrategia y Competición**
- Comportamiento  
- Ligas RoboCup  

---

## Matriz de trazabilidad (mapa mental → repo)

En cada sección:

- **Qué significa**: definición operativa (para el equipo).
- **Dónde vive en el repo**: carpeta(s) objetivo.
- **Qué hay hoy**: archivos existentes relevantes.
- **Huecos / riesgos**: por qué importa.
- **Acción 2026**: qué construir o refactorizar.
- **Validación**: test/protocolo mínimo.

### 1) Detección y Seguimiento

#### 1.1 Objetos clave (qué detectamos y qué exportamos)

**Qué significa**  
Definir explícitamente los objetos "de juego" que el sistema necesita detectar y en qué formato los comunica al controlador.

Mínimo en Soccer Vision 2026:
- Pelota (posición relativa + confianza)
- Arco propio y arco rival (ángulo/distancia aproximada + confianza)
- Línea / borde (para evitar salir)
- (Opcional) obstáculos/robots (si se usa en estrategia)

**Dónde vive en el repo**  
- `software/vision/` (implementación OpenMV)
- `software/robot*/` (decisión/control en Teensy)
- `docs/` (definición de interfaz/protocolo)
- `testing/` (tests de visión y latencia)

**Qué hay hoy (referencias)**  
- Punto de partida declarado: `software/vision/README.md`
- Código base 2025 (OpenMV): `legacy/2025-season/code/vision-openmv/`
  - Ejemplo: `calcula-coordenadas-pelota.py` (detección por blobs + homografía)
- Delantero 2025 (consumo de datos): `legacy/2025-season/code/delantero/perseguir-pelota.ino`
- Auditoría técnica del sistema 2025: `research/completed/2026-02-21-arquitectura-sistema-2025.md`

**Huecos / riesgos**
- Si no existe una definición única de "qué es un objeto y qué campos tiene", el protocolo UART y la estrategia se vuelven inestables.
- En 2025 se detectaron thresholds duplicados/inconsistentes y fragilidad de calibración.

**Acción 2026 (recomendación)**
- Definir un **VisionPacket v2** (documento + código):
  - timestamp (ms)
  - ball: x,y,conf
  - goals: lista de {id, angle, dist, conf}
  - line: flags (detected_left/center/right) o distancia a borde si existe
- Cada campo debe tener **unidades explícitas** (cm/mm/grados) y rangos.

**Validación**
- `testing/protocols/vision/`:
  - test de detección (escenas A/B/C con iluminación distinta)
  - medición de FPS y latencia cámara→UART→control

---

#### 1.2 Métodos tradicionales (baseline robusto)

**Qué significa**  
Pipeline de visión clásico (rápido, depurable) que funcione de manera consistente en sedes distintas.

**Dónde vive en el repo**
- `software/vision/openmv/traditional/`

**Qué hay hoy**
- En legacy: blob detection por LAB, homografía, configuración QVGA y white balance fijo.

**Huecos / riesgos**
- Thresholds hardcodeados y dispersos → falla por iluminación.
- Sin "score de confianza" → el controlador no sabe cuándo ignorar una detección dudosa.
- Sin filtro temporal → comportamiento "nervioso".

**Acción 2026**
- Centralizar thresholds en un `config.py` único.
- Agregar:
  - ROI (recorte) para evitar público/paredes
  - morfología (erode/dilate) para limpiar ruido
  - filtro temporal (EMA o Alpha-Beta) para suavizar pelota/arcos
  - validación automática de thresholds (Lmin<Lmax, etc.)
- Exportar *siempre* confianza.

**Validación**
- Protocolo de calibración de 60 s previo a partido.
- Test reproducible con "video/grabación" (si usan clips) o escenas estándar.

---

#### 1.3 Algoritmos de IA (cuando sumar, no por moda)

**Qué significa**  
Usar modelos ML en visión solo si el baseline tradicional no alcanza (por variabilidad extrema de luz/fondos).

**Dónde vive en el repo**
- `software/vision/openmv/ml/` (experimental)
- `research/backlog/` para dataset, métricas, trade-offs

**Huecos / riesgos**
- ML sin dataset propio = humo.
- Si no hay fallback tradicional, se vuelve frágil.

**Acción 2026**
- Mantener baseline tradicional como "sistema A".
- ML como "sistema B" activable por config y con métricas:
  - precision/recall de pelota
  - latencia total
  - consumo CPU/FPS

**Validación**
- Misma batería de tests del baseline + comparación A/B.

---

### 2) Hardware y Plataformas

#### 2.1 Computadoras integradas (Teensy + OpenMV como sistema)

**Qué significa**
Definir claramente qué corre en OpenMV (percepción) y qué corre en Teensy (control + estrategia + seguridad).

**Dónde vive**
- `software/vision/` (OpenMV)
- `software/robot*/` (Teensy)
- `hardware/` (BOM, wiring, PCB)

**Qué hay hoy**
- Stack declarado en `README.md` y en `AI-INSTRUCTIONS.md`
- Librería de hardware (HAL): `legacy/2025-season/code/libraries/zirconLib/`
- Documentación de reglas y constraints: `competition/rules/README.md`

**Huecos / riesgos**
- Variantes de PCB Zircon (Mark1/Naveen1) con pinouts distintos → riesgo de inicializar mal.
- Falta de "self-test" al boot (motores/sensores).

**Acción 2026**
- Agregar un modo "diagnóstico" (firmware) que:
  - imprime versión Zircon detectada
  - testea cada motor
  - lee sensores (línea, IMU, etc.) con valores en crudo
- Estandarizar conectores/pines críticos en documentación (BOM + wiring).

**Validación**
- Checklist pre-partido: versión PCB + test motores + test sensores + test comm module.

---

#### 2.2 Sensores (qué sensores importan y cuáles son ruido)

**Qué significa**
Definir cuáles sensores son "críticos para ganar partidos" y cuáles se eliminan o quedan como experimentales.

**Qué hay hoy**
- En `zirconLib` hay soporte para:
  - línea (3 sensores analógicos)
  - IMU BNO055 (I2C)
  - "readBall()" con 8 entradas analógicas (ver nota)
  - botones, etc.
- La auditoría 2025 marca:
  - calibración de línea hardcodeada
  - BNO055 con inicialización/calibración inconsistente

**Nota importante (a verificar)**
En Soccer Vision la pelota es una golf ball naranja. En el código/librería existe `readBall()` que sugiere un "ring" tipo IR (típico de Soccer Infrared).
Esto debe confirmarse físicamente:
- si esos sensores no aportan, pueden introducir ruido y complejidad.
- si se repurponen (ej. detección de luz/reflectancia), documentar claramente.

**Acción 2026**
- Declarar "sensores tier 1" (obligatorios) vs "tier 2" (experimentales).
- Resolver IMU (BNO055): una sola API coherente y calibración clara.

**Validación**
- Test de repetibilidad:
  - heading estable en 360°
  - detección de línea consistente en 3 iluminaciones

---

### 3) Localización y Navegación

#### 3.1 Técnicas espaciales (geometría y marcos de referencia)

**Qué significa**
No es SLAM: es tener geometría consistente:
- píxel → (X,Y) relativo al robot (pelota)
- heading estable (IMU)
- "dónde está el borde" (línea)

**Qué hay hoy**
- Homografía en OpenMV (legacy).
- BNO055 parcialmente integrado (ver auditoría).

**Acción 2026**
- Documentar marcos:
  - frame cámara (u,v)
  - frame robot (X adelante, Y izquierda, yaw)
- Documentar la homografía:
  - cómo se calibra
  - cómo se versiona
  - cómo se valida (error promedio en cm)

**Validación**
- Test "grid" en cancha: puntos conocidos (X,Y) vs estimación.

---

#### 3.2 Fusión de sensores (lo mínimo viable)

**Qué significa**
Combinar:
- visión (pelota/arcos)
- IMU (heading)
- línea (escape)
para que el robot sea estable.

**Qué hay hoy**
- Estrategia 2025 con estados simples (delantero).
- Falta un control omnidireccional unificado y un yaw-hold consistente.

**Acción 2026**
- Control de movimiento con `drive(vx, vy, omega)` + PID heading.
- Política de confianza:
  - si visión conf baja → buscar
  - si línea detectada → override de seguridad

**Validación**
- Repetibilidad de trayectorias y "escape de línea" en 20 repeticiones.

---

### 4) Sistemas de Control y Comms

#### 4.1 Protocolos de comunicación (UART visión→control, y módulo árbitro)

**Qué significa**
Dos comunicaciones distintas:

1) **OpenMV ↔ Teensy (UART interno)**: percepción y telemetría.
2) **Communication Module (start/stop)**: seguridad/reglamento en internacional.

**Qué hay hoy**
- UART legacy frágil (6 bytes con headers 201/202).
- El repo ya marca el **Communication Module obligatorio** (ver rules).

**Acción 2026**
- Diseñar **protocolo UART v2**:
  - SOF + len + type + payload + CRC8 + seq
  - re-sincronización y timeout
- Integrar start/stop como "interlock":
  - robot deshabilitado por defecto hasta start
  - stop → motores off inmediato

**Validación**
- Test de ruido/pérdida de bytes: el parser se recupera.
- Test de stop/start con módulo (o señal simulada).

---

#### 4.2 Actuadores y movimiento (la base de todo)

**Qué significa**
Si el movimiento no es repetible, la estrategia y la visión "se vuelven poesía".

**Qué hay hoy**
- `zirconLib` controla motores, pero el código 2025 re-implementa combinaciones por todos lados (según auditoría).

**Acción 2026**
- Una sola capa de cinemática:
  - `drive(vx, vy, omega)` o `move(angle, speed, omega)`
  - saturación y normalización
  - calibración por rueda
- Evitar funciones bloqueantes (ej. `pulseIn()` en arquero) o aislarlas.

**Validación**
- Test de velocidad lineal y rotacional con error medible.
- Logs por Serial/SD (al menos durante desarrollo).

---

### 5) Estrategia y Competición

#### 5.1 Ligas RoboCup (constraints que mandan)

**Qué significa**
Las reglas condicionan mecánica, electrónica y software (no al revés).

**Qué hay hoy**
- Reglas resumidas en `research/completed/2026-02-21-analisis-reglas-2026.md`
- Checklist ampliada en `competition/rules/README.md`

**Acción 2026 (recordatorio crítico)**
- Soccer Vision:
  - diámetro/altura 18 cm
  - **ball-capturing zone: 1.5 cm**
  - handle y top marker obligatorios
  - comm module obligatorio internacional

**Validación**
- Inspección interna "modo juez": medir todo antes de viajar.

---

#### 5.2 Comportamiento (delantero/arquero como producto)

**Qué significa**
Definir roles, transiciones, y estados de recuperación.

**Qué hay hoy**
- Delantero 2025: FSM simple.
- Arquero 2025: requiere reescritura/estabilización (según revisión).

**Acción 2026**
- FSM jerárquica (HFSM) o Behavior Tree simple:
  - capa "seguridad" (línea/stop)
  - capa "juego" (buscar, atacar, defender, recolocar)
- Coordinación mínima entre robots (aunque sea sin radio):
  - zonas de cancha y reglas de prioridad.

**Validación**
- Scrimmage con checklist:
  - no se estorban
  - arquero no abandona área
  - delantero no hace "loop infinito" buscando

---

## Backlog sugerido (nombres de archivo recomendados)

Estos son nombres propuestos para temas en `research/backlog/` (crear cuando corresponda):

- `2026-02-21-protocolo-uart-vision-teensy-v2.md`
- `2026-02-21-calibracion-vision-thresholds-y-homografia.md`
- `2026-02-21-control-omni-drive-vx-vy-omega.md`
- `2026-02-21-heading-hold-bno055.md`
- `2026-02-21-line-sensors-coverage-y-escape.md`
- `2026-02-21-arquero-rewrite-no-bloqueante.md`
- `2026-02-21-estrategia-2v2-roles-y-coordinacion.md`
- `2026-02-21-vision-ml-experimentos.md`

---

## Definition of Done (DoD) por área (mínimo)

- **Visión (baseline)**:
  - detecta pelota en ≥90% de escenarios de test (A/B/C)
  - latencia estable (medida y registrada)
  - thresholds + homografía versionados y recalibrables in-situ

- **UART v2**:
  - CRC detecta errores
  - parser se recupera tras pérdida de bytes
  - documentación del protocolo + ejemplos

- **Control/movimiento**:
  - función omnidireccional unificada
  - heading-hold funcional (IMU)
  - sin bloqueos (o con watchdog)

- **Reglamento**:
  - ball-capturing zone 1.5 cm cumplida
  - handle/top marker/comm module integrados y validados

---

## Conclusiones

1) El mapa mental es una **estructura útil** para ordenar el trabajo: visión → control → hardware → estrategia → compliance.  
2) En el repo actual, la mayor parte del "código real" todavía vive en `legacy/2025-season/`, mientras que 2026 está en fase de estructuración y análisis.  
3) Para competir internacionalmente, la prioridad técnica es: **robustez** (protocolo, calibración, control) + **compliance 2026** (1.5 cm + comm module + handle + top marker).

---

## Recomendaciones

- Mantener este documento como "mapa del sistema": actualizarlo cuando se cierre un tema grande (UART v2, nueva visión, nuevo control).
- Convertir cada hoja del mapa mental en un conjunto de artefactos: código + doc + test.
- No subir complejidad (IA/estrategias) antes de resolver determinismo: protocolo robusto, control unificado y calibración reproducible.

---

*Documento creado por ChatGPT (OpenAI — GPT-5.2 Pro) bajo supervisión de Gustavo Viollaz (@gviollaz). Basado en capturas del mapa mental de NotebookLM provistas por el equipo.*