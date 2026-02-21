---
title: "Mapa de Prioridades Revisado — Post-Verificación Cruzada"
date: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: false
ai-tool: "Claude (Anthropic - Claude Opus 4.6)"
status: final-verificado
tags: [prioridades, planificacion, roadmap, 2026, verificado]
---

# Mapa de Prioridades Revisado — RoboCup 2026

**Fecha**: 21 de febrero de 2026
**Autor**: Claude (Anthropic — Claude Opus 4.6)
**Supervisión**: Gustavo Viollaz (@gviollaz)
**Base**: Verificación cruzada de hipótesis Claude + ChatGPT contra código fuente real
**Competencia**: RoboCup 2026 Incheon, Corea del Sur — 30 junio a 6 julio 2026

---

## Resumen de Verificación

| Métrica | Resultado |
|---------|----------|
| Hipótesis confirmadas | 19/23 (83%) |
| Parcialmente confirmadas | 3/23 (13%) |
| Refutadas | 1/23 (4%) — #12 colisión header/dato |
| Hallazgos nuevos | 3 (N1: Pin 0/RX1, N2: migración truncada, N3: código competencia ausente) |
| Total problemas activos | 25 |

---

## Prioridad 0 — Reconstrucción (ANTES de todo lo demás)

> Sin completar P0, cualquier trabajo posterior se basa en información incompleta.

### P0.1 — Reconstruir código de competencia
- **Qué**: Determinar qué código REALMENTE CORRIÓ en el campeonato nacional dic 2025
- **Por qué**: El repositorio NO refleja lo que funcionó — bugs críticos (#18 striker nunca patea, #15 arquero no compila) son incompatibles con un equipo campeón
- **Cómo**: Sesión con María Virginia Viollaz y Elías Cordero para revisar computadoras del equipo, extraer versiones exactas cargadas en los robots
- **Entregable**: Archivos `.ino` y `.py` exactos que corrieron, commiteados como `legacy/2025-season/competition-actual/`
- **Refs**: Hallazgo N3

### P0.2 — Migrar TODOS los archivos del repo 2025
- **Qué**: Copiar contenido completo (no stubs) de todos los archivos relevantes del repo 2025
- **Por qué**: Archivos críticos ausentes o truncados (hallazgo N2): la versión OpenMV más avanzada (6.7KB), el único código BNO055 funcional (2.4KB), la herramienta de calibración (7.1KB), archivos STL, etc.
- **Cómo**: Script de migración desde `IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025`
- **Entregable**: `legacy/2025-season/code/` con archivos completos, no stubs
- **Refs**: Hallazgo N2, listado completo de archivos ausentes en arquitectura §7

---

## Prioridad 1 — Bugs que impiden funcionamiento (Febrero-Marzo)

> Corregir bugs que hacen que el código actual sea disfuncional.

### P1.1 — Rediseñar protocolo UART
- **Bugs**: #13 (sin checksum), #14 (sin resync)
- **Nota**: #12 (colisión header/dato) fue REFUTADO — la separación header≥201 / dato≤200 es correcta por diseño
- **Diseño propuesto**: Start byte 0xFF, longitud, payload, XOR checksum, timeout 100ms, resync por start byte
- **Entregable**: `software/communication/protocol.md` + implementación OpenMV y Teensy

### P1.2 — Corregir zirconLib
- **Bugs**: #6 (BNO055 compassCalibrated siempre false), #21 (sin moveOmni), N1 (Pin 0/RX1 en Naveen1)
- **Acciones**:
  - Arreglar calibración BNO055 o eliminar wrapper roto y usar acceso directo
  - Agregar `moveOmni(angle, speed, rotation)`, `stop()`, etc.
  - En modo Naveen1: no llamar `pinMode()` sobre motorXpwm no asignados (valor 0 = RX1)
- **Entregable**: `zirconLib2.cpp/h` con tests

### P1.3 — Corregir delantero
- **Bugs**: #18 (timeout pateo ≤ → ≥), #19 (ángulo arco usa Yp,Xp en vez de Ya,Xa), #20 (avanzarAlFrente diagonal)
- **Entregable**: `software/teensy/striker.ino` funcional y testeado

### P1.4 — Reescribir arquero desde cero
- **Bugs**: #15 (no compila — múltiples errores de sintaxis, funciones no definidas, declaraciones duplicadas), #16 (sensores leen basura a nivel global)
- **No hay estructura salvable** en el código actual — reescritura completa necesaria
- **Entregable**: `software/teensy/goalie.ino` compilable y testeado

---

## Prioridad 2 — Mejoras que cambian el nivel (Marzo-Abril)

> De "funciona" a "compite al nivel internacional".

### P2.1 — Integrar sensores IR para detección 360° de pelota
- **Bug**: #5 (8 sensores instalados pero no usados)
- **Objetivo**: Fusión sensorial cámara (primaria) + IR (360° respaldo)
- **Entregable**: Módulo de fusión sensorial en código del delantero

### P2.2 — Centralizar configuración
- **Bugs**: #3 (thresholds hardcodeados), #8-9 (L_min > L_max), #10 (dos homografías)
- **Objetivo**: Un solo `config.h` (Teensy) y `config.py` (OpenMV) como fuente de verdad
- **Incluir**: Validación programática de thresholds (L_min < L_max)
- **Entregable**: Archivos de config + script de validación

### P2.3 — Control de heading con BNO055
- **Bug**: #6 (BNO055 no integrado — fue comentado antes de competencia)
- **Objetivo**: Control PID de yaw como capa base de todos los movimientos
- **Referencia**: Archivo `lateral_con_giróscopo` tiene implementación funcional con kp=0.3
- **Entregable**: Módulo de heading control integrado en zirconLib2

### P2.4 — Calibración exprés pre-partido
- **Bug**: #3 (thresholds fijos para una cancha), #10 (homografía fija)
- **Objetivo**: Procedimiento de calibración < 5 minutos previo a cada partido
- **Referencia**: `Calibrar_Treshold.py` (7.1KB) existe como base
- **Entregable**: Protocolo documentado + scripts de calibración

---

## Prioridad 3 — Requisitos internacionales 2026 (Abril-Junio)

> Obligatorios para pasar inspección técnica en Incheon.

### P3.1 — Communication Module
- **Regla**: OBLIGATORIO en internacional. GPIO header 2.54mm + 500mA de alimentación
- **Acción**: Diseñar/adquirir módulo, integrar en Zircon PCB, software de comunicación
- **Entregable**: Hardware + `software/teensy/comm_module.h`

### P3.2 — Verificar zona de captura 1.5cm
- **Regla**: Ball-capturing zone reducida de 3.0cm a 1.5cm
- **Bug**: #23 (dribbler puede ser demasiado agresivo)
- **Acción**: Medición física con dribbler activo, ajustar mecanismo si necesario
- **Entregable**: Mediciones documentadas + ajuste mecánico si necesario

### P3.3 — Handle y top marker
- **Regla**: Handle estable obligatorio con clearances mínimos. Top marker: círculo blanco ≥4cm diámetro (robot NO puede jugar sin él)
- **Acción**: Diseño mecánico, verificar cumplimiento de dimensiones
- **Entregable**: Diseños actualizados, checklist de inspección

### P3.4 — Documentación de competencia
- **Regla**: Documentación puntuada en internacional
- **Componentes**: BOM con precios, poster A1, video técnico, portfolio, preparación para interview
- **Entregable**: `competition/documentation/` con todos los materiales

---

## Timeline

```
Feb 2026          Mar 2026          Abr 2026          May 2026          Jun 2026
│                 │                 │                 │                 │
├─ P0 ───────────┤                 │                 │                 │
│ Reconstrucción  │                 │                 │                 │
│ + Migración     │                 │                 │                 │
│                 ├─ P1 ───────────┤                 │                 │
│                 │ Bugs críticos   │                 │                 │
│                 │ UART/zircon/    │                 │                 │
│                 │ striker/goalie  │                 │                 │
│                 │                 ├─ P2 ───────────┤                 │
│                 │                 │ IR 360°, BNO055 │                 │
│                 │                 │ Config central  │                 │
│                 │                 │ Calibración     │                 │
│                 │                 │                 ├─ P3 ───────────┤
│                 │                 │                 │ Comm Module     │
│                 │                 │                 │ 1.5cm verify    │
│                 │                 │                 │ Handle/marker   │
│                 │                 │                 │ Documentación   │
│                 │                 │                 │                 │
                                                                    30 Jun
                                                                 INCHEON
```

---

## Concordancia entre Análisis

| Aspecto | Claude | ChatGPT | Acuerdo |
|---------|--------|---------|--------|
| Protocolo UART frágil | #12-14 (detalle granular) | Categoría A (síntesis) | ✅ Ambos identifican como crítico |
| BNO055 no integrado | #6 (mecanismo preciso) | Categoría B (observación general) | ✅ Ambos correctos |
| Arquero roto | #15-16 (bugs específicos) | Categoría C (menciona reescritura) | ✅ Ambos correctos |
| IR no usados | #5 (verificado en código) | Categoría D (mencionado) | ✅ Ambos correctos |
| pulseIn bloqueante | #7 (mecanismo) | Categoría E (mencionado) | ✅ Ambos correctos |
| Calibración frágil | #3, #8-9 (thresholds) | Categoría F (mencionado) | ✅ Ambos correctos |

| Diferencia | Claude | ChatGPT |
|-----------|--------|--------|
| Profundidad técnica | 23 puntos de falla con código citado | 6 categorías de alto nivel |
| Error factual | 1 (#12 colisión header/dato — REFUTADO) | 0 detectados |
| Foco | Diagnóstico forense/exhaustivo | Visión estratégica/plan de acción |
| Reglas 2026 | Mencionadas, foco en software | Mayor énfasis en handle/top marker/inspección |

**Conclusión**: Los análisis son COMPLEMENTARIOS. Claude provee diagnóstico técnico granular; ChatGPT provee visión estratégica. La combinación cubre más terreno que cualquiera solo.

---

*Documento generado por Claude (Anthropic — Claude Opus 4.6) bajo supervisión de Gustavo Viollaz (@gviollaz), 21 de febrero de 2026.*
*Basado en verificación cruzada completa contra código fuente real de ambos repositorios.*