---
title: "Revisión de código — Temporada 2025"
date: 2026-02-21
updated: 2026-02-21
author: "Claude (Anthropic - Claude Opus 4.6)"
ai-assisted: false
ai-tool: "Claude (Anthropic - Claude Opus 4.6)"
status: final-verificado
tags: [bugs, codigo, revision, 2025, verificado]
nota: "Actualizado post-verificación cruzada contra código fuente real"
---

# Revisión de Código — Temporada 2025

## Estado post-verificación (21 feb 2026)

Esta lista original de 8 bugs fue ampliada a **25 puntos de falla** en el documento de arquitectura completo.
La verificación cruzada contra código fuente real confirmó la mayoría y agregó hallazgos nuevos.

Ver documentos actualizados:
- **[Arquitectura completa (28KB, verificada)](2026-02-21-arquitectura-sistema-2025.md)** — 23 puntos de falla + 3 nuevos, con estado de verificación
- **[Análisis cruzado de verificación](2026-02-21-analisis-cruzado-verificacion-hipotesis.md)** — metodología y resultados detallados
- **[Mapa de prioridades revisado](2026-02-21-mapa-prioridades-revisado.md)** — plan de acción P0-P3

## Resumen de la revisión original (8 bugs iniciales)

| # | Severidad | Componente | Bug | Estado verificación |
|---|-----------|-----------|-----|--------------------|
| 1 | CRITICAL | Striker (perseguir-pelota.ino) | Timeout de pateo invertido (`<=` en vez de `>=`) | ✅ Confirmado |
| 2 | CRITICAL | Striker | Ángulo de arco usa coordenadas de pelota | ✅ Confirmado |
| 3 | CRITICAL | Goalie (arquero-base.ino) | Código no compila (funciones no definidas, redeclaraciones) | ✅ Confirmado (peor de lo esperado) |
| 4 | HIGH | zirconLib | `compassCalibrated` nunca se activa → BNO055 inutilizado | ✅ Confirmado |
| 5 | HIGH | OpenMV | Thresholds con L_min > L_max | ✅ Confirmado |
| 6 | HIGH | Dribbler | Espera string "pelota detectada" que nadie envía | ✅ Confirmado |
| 7 | MEDIUM | General | 8 sensores IR instalados pero no usados en código de competencia | ✅ Confirmado |
| 8 | MEDIUM | Protocolo UART | Sin checksum ni mecanismo de resync | ✅ Confirmado |

## Corrección importante

⚠️ La hipótesis original #12 del documento de arquitectura ("colisión entre headers 201-204 y valores de datos") fue **REFUTADA** durante la verificación cruzada. El protocolo fue diseñado con separación intencional: datos en rango 0-200, headers en rango 201-204. Ver documento de arquitectura §5.4 para detalles completos.

---

*Actualizado por Claude (Anthropic — Claude Opus 4.6) bajo supervisión de Gustavo Viollaz (@gviollaz), 21 de febrero de 2026.*