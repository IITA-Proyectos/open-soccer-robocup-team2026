# Instrucciones para asistentes de IA

> Este archivo está dirigido a cualquier modelo de lenguaje (LLM) o asistente de IA que trabaje con este repositorio.

## Contexto del proyecto

Este repositorio pertenece a un equipo de estudiantes del IITA (Instituto de Innovación y Tecnología Aplicada) en Salta, Argentina, que participa en la competencia **RoboCupJunior Soccer Open League 2026**.

El equipo construye y programa **2 robots autónomos** (un arquero y un delantero) que juegan fútbol en una cancha regulada. Los robots usan:

- **Arquitectura**: distribuida en **3 placas por robot** conectadas por UART — CENTRAL (Teensy 4.1, PCB custom "Zircon") + TOP/ARRIBA (Teensy 4.0) + DOWN/ABAJO (Teensy 4.0). Detalle: [`docs/ARQUITECTURA-3-PLACAS-2026.md`](docs/ARQUITECTURA-3-PLACAS-2026.md).
- **Microcontroladores**: 3× Teensy por robot (1× 4.1 + 2× 4.0) + ESP32-C6 en la placa COMM (señales de árbitro RCJ)
- **Visión**: 2× cámaras **OpenMV N6** (PAG7936) con MicroPython — frontal + trasera. ⚠️ NO son H7: en fw 4.8.1 los scripts deben usar el módulo `sensor` + `pyb.UART` (`machine.UART` y `pyb.LED` crashean). Script vivo: `hardware/electronics/camaras-openmv/main.py`; calibración: `docs/firmware/CALIBRACION-VISION-N6.md`.
- **Comunicación**: UART entre placas (CENTRAL↔TOP↔DOWN) y OpenMV↔Teensy; ESP32-C6 para árbitros
- **Sensores**: línea (anillo de 32 sensores ópticos), 2× OTOS (odometría), IMU/giróscopo (BNO055), ToF VL53L7CX, ultrasonido HC-SR04
- **Actuadores**: motores TT con drivers H-bridge (base omni-3), dribbler (opcional). **Sin kicker físico**: el delantero empuja la pelota por inercia.
- **Estructura**: impresión 3D (Tinkercad/OpenSCAD) + construcción manual
- **Placa custom**: PCB "Zircon" (CENTRAL) con librería propia (zirconLib)

## Reglas que debés seguir

### 1. Atribución

Todo cambio que hagas **debe** incluir en el mensaje de commit:
```
Author: [Tu nombre de IA] ([Proveedor])
Requested-by: [Nombre del humano que te lo pidió]
```

### 2. Estructura

- Respetar la estructura de carpetas existente
- Archivos nuevos: convención `kebab-case`
- Archivos con fecha: `YYYY-MM-DD-descripcion.md`
- Incluir frontmatter YAML en todo archivo `.md`

### 3. Journal de ingeniería

Si documentás trabajo o análisis, crear una entrada en `journal/` con el formato:
```
journal/YYYY-MM-DD-descripcion.md
```

### 4. Investigación

- Temas nuevos a investigar → `research/backlog/`
- Análisis en curso → `research/in-progress/`
- Análisis terminados → `research/completed/`
- Referencias externas → `research/references/`

### 5. Testing

- Protocolos de prueba → `testing/protocols/`
- Resultados → `testing/results/YYYY-MM-DD-descripcion.md`

### 6. Código

- **No modificar** archivos en `legacy/` (son referencia histórica)
- Código nuevo va en `software/` en la subcarpeta correspondiente
- Siempre comentar el código
- Indicar si el código fue probado en hardware real

### 7. Hardware

- Esquemáticos y PCB → `hardware/electronics/`
- Baterías, motores, potencia → `hardware/electrical/`
- Diseños 3D y ensamblaje → `hardware/mechanical/`

## Archivos clave a leer primero

> ⚠️ **Antes de tocar nada, hacé `git pull`** y leé los dos índices vivos (1 y 2). Es obligatorio — ver [`CLAUDE.md`](CLAUDE.md) → "Protocolo de sesión".

1. **[`docs/ESTADO-ACTUAL.md`](docs/ESTADO-ACTUAL.md)** — ⭐ estado vivo del robot: qué módulos corren, qué TASKs bloquean, qué deudas hay. **1ª lectura obligatoria.**
2. **[`docs/FUENTES-DE-VERDAD.md`](docs/FUENTES-DE-VERDAD.md)** — ⭐ qué doc/módulo es canónico por tema (no editar un doc superado).
3. [`README.md`](README.md) — visión general del proyecto y estructura.
4. [`CLAUDE.md`](CLAUDE.md) — frame del coach + ubicación del repo y reglas de trabajo en paralelo.
5. [`CONTRIBUTING.md`](CONTRIBUTING.md) — reglas de contribución y atribución.
6. [`competition/timeline.md`](competition/timeline.md) — cronograma y deadlines.
7. [`legacy/2025-season/README.md`](legacy/2025-season/README.md) — qué se hizo el año pasado.

## Convenciones de tags

Usar estos tags en el frontmatter de archivos `.md`:

**Área**: `vision`, `movilidad`, `control`, `dribbler`, `comunicacion`, `electronica`, `mecanica`, `bateria`, `sensores`  
**Tipo**: `analisis`, `tutorial`, `protocolo`, `resultado`, `decision`, `comparacion`  
**Robot**: `arquero`, `delantero`, `ambos`  
**Prioridad**: `alta`, `media`, `baja`
