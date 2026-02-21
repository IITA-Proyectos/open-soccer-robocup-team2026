---
title: "Análisis del repositorio open-soccer-robocup-team2026 (estado, riesgos y plan de acción)"
date: 2026-02-21
author: "ChatGPT-5.2 Pro (OpenAI)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "ChatGPT / GPT-5.2 Pro (OpenAI)"
status: final
tags: [analisis, repositorio, robocup2026, soccer-vision, planificacion, software, hardware, reglas, legado-2025]
---

# Análisis del repositorio `open-soccer-robocup-team2026`

**Este informe fue generado por _ChatGPT (modelo GPT-5.2 Pro, OpenAI)_ bajo la supervisión de _Gustavo Viollaz_.**  
Fecha del análisis: **2026-02-21**.

## 1. Objetivo y alcance

Este documento resume:

- El **estado actual del repositorio** `open-soccer-robocup-team2026` (estructura, intención, piezas clave).
- Implicancias de **RoboCupJunior Soccer 2026** (reglas y requisitos que impactan en el diseño).
- Hallazgos principales del **legacy 2025** y de los análisis internos ya publicados en este repo.
- Un **plan de acción** (pasos concretos) para transformar el sistema 2025 en una base 2026 robusta, depurable y escalable.

**Alcance técnico:** revisión de archivos del repo + referencias oficiales de reglas/fechas.  
**Fuera de alcance:** ejecución en hardware real, mediciones físicas, validación en cancha.

---

## 2. Qué es el repo (y cómo está organizado)

El repo está planteado como repositorio central de ingeniería para RoboCupJunior **Soccer Vision** (antes "Soccer Open") temporada 2026.

Estructura (ver `README.md` en raíz):

- `software/` — código por robot, visión, comunicación y librerías compartidas
- `hardware/` — electrónica/eléctrica/mecánica
- `research/` — pipeline de investigación (backlog / in-progress / completed / references)
- `testing/` — protocolos y resultados de pruebas
- `competition/` — reglas, cronograma, entregables de competencia
- `journal/` — diario de ingeniería
- `legacy/2025-season/` — referencia histórica (no modificar)

📌 Este repo preserva explícitamente el **código 2025** y lo usa como base para 2026 (ver `legacy/2025-season/` y el README).

---

## 3. Contexto de competencia 2026 que condiciona el diseño

### 3.1. Fechas y sede RoboCup 2026

RoboCup 2026 (mundial) está anunciado del **30 Jun 2026 al 06 Jul 2026** en **Songdo Convensia, Incheon, Corea del Sur**.  
Referencia oficial: https://www.robocup.org/events/80

### 3.2. Cambios/reglas 2026 con impacto directo en hardware y software

Reglas draft 2026 (referencia principal):  
https://robocup-junior.github.io/soccer-rules/2026-soccer-draft-rules/rules.html

Puntos que pegan fuerte:

- **Renombre de liga:** "Soccer Open" → **"Soccer Vision (formerly Soccer Open)"**.
- **Ball-capturing zone:** **1.5 cm**. Esto obliga a medir y ajustar el "frente" del robot (dribbler/cuna/defensas) para no exceder esa profundidad.
- **Handle obligatorio:** manija estable y accesible con clearances mínimos (impacta CAD/altura/armado).
- **Top marker obligatorio:** círculo blanco arriba (mínimo 4 cm de diámetro). Si no está, el robot **no puede jugar**.
- **Communication Module (internacional):** el comité provee un módulo para start/stop. Se espera interfaz por **GPIO 2.54 mm** (y posiblemente UART en el futuro).

**Consecuencia:** 2026 no es "solo mejorar código": hay requisitos físicos (handle/top marker/ball-capturing) + interfaz de control (communication module) que hay que cerrar temprano para evitar sorpresas en inspección.

---

## 4. Lo más valioso ya dentro del repo: auditoría forense del sistema 2025

Este repo ya contiene análisis internos muy fuertes y accionables. En particular:

- `research/completed/2026-02-21-arquitectura-sistema-2025.md`
- `research/completed/2026-02-21-revision-codigo-2025.md`

Esos documentos describen que el sistema 2025 fue competitivo (campeón nacional) pero arrastra fallas estructurales que, si no se corrigen, vuelven el 2026 frágil y difícil de depurar.

### 4.1. Problemas recurrentes detectados (síntesis técnica)

> Nota: lo siguiente es una síntesis; ver los documentos fuente para detalle y contexto.

**A) Protocolo UART (OpenMV → Teensy) frágil / versiones múltiples**  
Problemas típicos:
- sincronización frágil (si se pierde un byte, se desalinean paquetes),
- valores ambiguos ("0" como "no detectado" aunque puede ser valor válido),
- falta de checksum/CRC,
- falta de heartbeat / timeout formal.

**B) IMU/BNO055 presente pero sin integrarse bien al control**  
El análisis indica que el sensor existía pero terminó deshabilitado en competencia o no calibraba correctamente; esto deja al robot con heading "a la deriva".

**C) Código de arquero con estado no confiable**  
La revisión sugiere reescritura / refactor profundo para evitar bugs estructurales.

**D) Sensores disponibles no integrados en la estrategia final**  
Ejemplo: sensores IR de pelota disponibles, pero no usados para recuperar pelota fuera del FOV de la cámara.

**E) Bloqueos y conflictos**  
Uso de funciones bloqueantes (ej. `pulseIn()`) y/o conflictos de pines según versiones de PCB y firmware.

**F) Visión dependiente de calibración manual frágil**  
Thresholds hardcodeados y falta de un procedimiento reproducible "rápido en cancha".

---

## 5. Riesgos principales si se continúa sin cambios estructurales

1) **Depuración imposible en competencia**: sin protocolo robusto + logging, cualquier ruido UART o falsa detección "parece magia negra".

2) **Regresión silenciosa**: múltiples versiones de scripts/código (OpenMV/Teensy) sin una fuente de verdad formal de interfaces.

3) **Desalineación hardware ↔ software**: cambios en pines o piezas mecánicas rompen comportamiento porque no hay contratos claros (pinout + protocolos + calib).

4) **Inspección 2026**: ball-capturing zone / handle / top-marker / communication module pueden dejar al robot fuera por detalles mecánicos.

---

## 6. Recomendaciones priorizadas (alineadas al forense 2025)

Este repo ya propone recomendaciones R1–R12 en el informe de arquitectura 2025. Las más "high leverage" (impacto grande, costo razonable):

1) **Formalizar y robustecer el protocolo UART** (SOF + len + type + payload + checksum/CRC + resync)  
2) **Integrar BNO055 como parte del control de orientación** (PID heading con manejo de wrap-around)  
3) **Validar ball-capturing zone 1.5 cm** con medición física + ajuste de dribbler/cuna  
4) **Refactor de FSM/estados** para eliminar duplicación y priorizar seguridad (línea, límites, stop)  
5) **Calibración reproducible** (visión + homografía + thresholds) con guardado de parámetros  
6) **Testing + logging**: procedimientos mínimos y registro de fallas con fecha (carpetas `testing/`)

---

## 7. Plan de acción propuesto (pragmático, orientado a "estabilidad primero")

### Paso 1 — "Contrato" de comunicación (1 fuente de verdad)
- Crear una especificación en `software/communication/` o `docs/` con:
  - definición de paquetes,
  - unidades,
  - rangos válidos,
  - valores "no detectado" fuera de rango real,
  - checksum/CRC,
  - comportamiento ante pérdida de bytes (resync),
  - timeout/heartbeat.

### Paso 2 — Parser robusto en Teensy (ring buffer)
- Implementar un parser que:
  - busque SOF,
  - valide len,
  - valide CRC,
  - descarte basura sin quedar desalineado.

### Paso 3 — Heading control obligatorio (IMU)
- Integrar control de orientación (PID de yaw) como capa base de movilidad:
  - el robot se mueve en vector **manteniendo heading** salvo orden contraria,
  - calibración explícita (procedimiento documentado),
  - fallback seguro si IMU falla.

### Paso 4 — FSM jerárquica
Separar "decisión" de "actuación":

- **Alto nivel:** buscar pelota / atacar / defender / reposicionarse / salir de línea / stop
- **Bajo nivel:** ir a vector, girar a heading, dribbler on/off, disparo

### Paso 5 — Calibración express pre-partido
- Visión: thresholds + homografía + validación rápida (procedimiento de 1 minuto)
- Línea: calibración de sensores y test de detección de borde
- Guardar parámetros (JSON/archivo) y registrar fecha/lugar.

### Paso 6 — Checklist de inspección 2026 (hardware)
- handle ok,
- top marker ok,
- ball-capturing zone ok,
- interface a communication module ok,
- seguridad eléctrica/puntos de medición ok.

---

## 8. Entregables recomendados a producir en el repo

1) `software/communication/protocol.md` (especificación formal UART)
2) `testing/protocols/` con al menos:
   - test de UART (ruido/desync),
   - test de heading control,
   - test de línea,
   - test de visión (falsos positivos/negativos)
3) `competition/inspection-checklist.md` (alineado a reglas 2026)
4) Un "mínimo demo" reproducible: robot en modo test que imprime telemetría a serial/SD.

---

## 9. Registro de instrucciones dadas a la IA (trazabilidad)

- Pedido original (humano): "quiero que analice el repositorio …"
- Pedido posterior (humano): "bajar todo el informe al repositorio … indicando que lo hiciste vos chatgpt 5.2 con mi supervisión".

Este documento refleja ese alcance y debe ser revisado por el equipo antes de tomarlo como "fuente de verdad".

---

## 10. Referencias (internas y externas)

### Internas (este repo)
- `README.md`
- `AI-INSTRUCTIONS.md`
- `CONTRIBUTING.md`
- `research/completed/2026-02-21-arquitectura-sistema-2025.md`
- `research/completed/2026-02-21-revision-codigo-2025.md`
- `legacy/2025-season/` (referencia histórica; no modificar)

### Externas (oficiales)
- RoboCup 2026 Incheon (fechas/sede): https://www.robocup.org/events/80
- Reglas RoboCupJunior Soccer 2026 (draft):  
  https://robocup-junior.github.io/soccer-rules/2026-soccer-draft-rules/rules.html
- Communication Module (RCJ): https://github.com/robocup-junior/soccer-communication-module
- IR golf ball (cambio Soccer Infrared 2026): https://github.com/robocup-junior/ir-golf-ball
