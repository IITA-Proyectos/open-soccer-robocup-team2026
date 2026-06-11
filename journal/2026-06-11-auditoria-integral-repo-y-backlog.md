---
title: "Auditoría integral del repo (multi-agente, verificación adversarial) + backlog consolidado Incheon"
date: 2026-06-11
author: "Claude (sesión coach, noche post-demo) — pedido de Gustavo: analizar programas y docs, simplificar haciéndolos más confiables, y consolidar los pendientes para priorizar juntos"
status: completa — backlog en docs/BACKLOG-INCHEON.md
---

# Auditoría integral 2026-06-11

## Método

48 agentes en 2 fases: 8 auditores por área (CENTRAL, TOP, DOWN, shared, docs,
pendientes, envs/builds, visión+tools) con lentes fail-safe/simplificación/
legibilidad, y verificación ADVERSARIAL de los hallazgos de código y P0/P1 (cada
verificador intenta REFUTAR leyendo el código real). Resultado: **122 hallazgos,
35 confirmados, 0 refutados, 5 "parcial"** (correcciones valiosas: p.ej. el
hallazgo del freno de borde era correcto pero YA estaba documentado como B1 en
el journal 2026-06-10 — el verificador evitó duplicar el tema).

## La foto

- **Firmware estructuralmente sólido**: gate 59 suites / 810 tests / 0 fallos;
  TODOS los módulos de src/shared tienen suite host; DOWN es la placa más madura
  (data_valid de punta a punta, calib EEPROM con CRC, salud OTOS por tick); los
  fail-safes principales de CENTRAL existen y están bien pensados.
- **Lo que bloquea Incheon NO es código nuevo**: deliverables de jueces (video,
  póster, BOM, deadline desconocido = TASK-041), visión en sede (TASK-022), y
  ~10 bugs P1 conocidos de fix chico.
- **El P1 técnico dominante sigue siendo B1** (freno de borde eclipsa el router
  de línea: robot puede quedar clavado frenando sobre la línea) — decisión de
  Gustavo pendiente entre 3 opciones ya analizadas.
- **Hallazgos nuevos de esta auditoría que más importan**: cap térmico de motores
  ausente en TODO el camino del delantero (quemado real >70% duty) · el único BNO
  vivo no tiene detector de muerte en runtime · la trilateración publica conf=70
  con heading inválido y el arquero la consume · la calib por app monitor-base no
  refresca el DownModel hasta reboot · un OTOS que NACKea 3 ticks muere para todo
  el partido (sin re-arme) · el detector "heading congelado" del analizador de
  caja negra tenía umbral en unidades equivocadas (3000 vs 30 dps) y NUNCA podía
  disparar · `default_envs` apuntaba al cableado viejo de R1 · el tutorial de
  build mandaba al clon señuelo `futbol2026` · main.py de cámaras sin try/except
  ni modo competencia (exposición fija).
- **Simplificaciones identificadas** (post-práctica, nunca a costa de fail-safes):
  ~70 líneas de arquero v1/Capa 3 muerto en strategy.cpp, `imu_zircon.cpp` sin
  gate (mete Adafruit BNO en binarios de una placa sin BNO), 8 copias del wrap de
  ángulo con bordes inconsistentes, `motion_target` auto-condenado, espejo
  `strategy_transitions` desincronizado (sin PUSH/PUSH_BACK). ⚠️ Veto del
  verificador: NO tocar `attack_color` hasta resolver D5 (TASK-024 polaridad de
  arco la ordena llamar; verificar la cadena real con goal_polarity en banco).

## Entregables de esta sesión

1. **`docs/BACKLOG-INCHEON.md`** — el backlog consolidado P0(7)/P1(D1-5, F1-10,
   V1-6, H1-2, C1-5)/P2, deduplicado y verificado, PARA PRIORIZAR CON GUSTAVO.
2. **`docs/pruebas-banco/QUE-FLASHEO-HOY.md`** — mapa estable de envs vigentes
   vs lista negra (gap P1 de navegabilidad).
3. **Fixes seguros aplicados esta noche (cero firmware de robot):**
   - `tools/blackbox/analizar_corrida.py`: umbral del detector de heading
     congelado 3000→30 (estaba en centideg vs columna en dps — inalcanzable).
   - `platformio.ini`: `default_envs` top_robot1→top_robot2_pri (un `pio run`
     pelado ya no puede flashear el cableado viejo).
   - `docs/firmware/SETUP-ENTORNO-BUILD-WINDOWS.md`: path del señuelo corregido.
   - `docs/pruebas-banco/TOP.md`: banner del recableado (cards IMU viejas).
   - `docs/ESTADO-ACTUAL.md`: bullet kickstart/rear-brake "pendiente" → ✅ HECHO
     (053fd0a); índice del backlog y del mapa de envs.
   - `CLAUDE.md`: moratoria (regla 8) CERRADA con evidencia; nota "7 semanas".
   - `AI-INSTRUCTIONS.md`: cámaras H7→N6 con los gotchas reales.
   - `docs/pruebas-banco/DOWN.md`: filas "falta crear" de envs que ya existen.
   - `docs/FUENTES-DE-VERDAD.md`: filas nuevas para los 2 docs creados.

## Qué NO se tocó a propósito

Ningún archivo que cambie un binario de robot (mañana es la práctica — regla de
oro). Todos los patches de firmware del backlog llevan plan de banco individual.
El digest completo de los 122 hallazgos quedó fuera del repo (es un artefacto de
sesión); los que valen están en el backlog con archivo:línea.
