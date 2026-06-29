---
title: "arqueromix: se eliminó el modo PATRULLA — el arquero queda SOLO QUIETO (limpieza)"
date: 2026-06-29
status: COMPILA + revisión adversarial SEGURO · NO validado en banco (smoke-test pendiente)
placa: CENTRAL (ROBOT2, arquero)
env: central_robot2_arqueromix_quieto (definitivo) + central_robot2_arqueromix (ahora también quieto)
autor: "Claude Opus 4.8 (1M context) + María/Virginia (decisión), vía Claude Code"
testeado-en-hardware: NO (compila + equivalencia verificada; el cierre lo hace el equipo)
---

# El arquero queda SOLO QUIETO — se sacó toda la patrulla

## Decisión (equipo, 2026-06-29)
María: "el quieto es el programa más confiable que tenemos, dejá solamente la lógica del quieto". Se
elimina el modo PATRULLA del arqueromix (ya no se vuelve a patrulla sin revertir). Objetivo: que el
programa no tenga código viejo que confunda.

## Qué se eliminó (todo inalcanzable cuando AMIX_QUIETO=true, que es como se flasheaba)
- **5 estados** (`amix_fsm.h`/`.cpp`): `moverce_derecha`, `moverce_izquierda`, `salir_linea_der`,
  `salir_linea_izq`, `avanzar_despues_de_patear`.
- **Helpers** de rebote por arco: `borde_arco_der/izq`, `rear_goal_dev`, `wrap180_local`.
- **Variables** estáticas `pd` y `s_commit_until_ms` (solo patrulla).
- **Flag** `AMIX_QUIETO` + las 5 ternarias `AMIX_QUIETO ? quieto : patrulla` → hardcodeadas a la rama quieto.
- **Constantes** de patrulla (`amix_config.h`): `AMIX_PD_SALIR`, `AMIX_T_SALIR_LINEA`, `AMIX_T_PATRULLA_COMMIT`,
  `AMIX_T_AVANCE_POST`, `AMIX_PATRULLA_POR_ARCO`, `AMIX_PROFUNDIDAD_POR_LINEA`, `AMIX_TOL_ARCO_OWN_DEG`,
  `AMIX_ARCO_OWN_SIGN` (+ flags `ARQMIX_PATRULLA_LINEA`/`NO_PROFUNDIDAD`/`FLIP_ARCO_OWN`).

## Qué quedó (intacto, el camino del quieto)
`inicio_lateral_izq → inicio_retroceder → inicio_avanzar → acomodar_linea → acomodar_orientar →
esperar_quieto`; despeje: `PATEANDO_pausa_inicial → ALINEAR_arco_opp → PATEANDO_adelante → [frenar_patada] →
PATEANDO_pausa → orientar_frente → PATEANDO_atras → acomodar_linea` (cierra). `esperar_quieto` conserva sus
4 ramas (despejar / buscar descentrada / anticipar S2 / quieto). Las primitivas `ad/aiproporcional` y sus
constantes (`AMIX_PROP_*`, `AMIX_PD_BASE/BALL`, `AMIX_HEADING_CORRECT_SIGN`) las usa el quieto → SIGUEN.

## Verificación
- `pio run -e central_robot2_arqueromix_quieto -e central_robot2_arqueromix` → **SUCCESS** (FLASH code 16680,
  más chico). El compilador confirma 0 referencias rotas.
- **Revisión adversarial (workflow, 3 revisores + síntesis): SEGURO.** Camino del quieto byte-idéntico en
  lógica (la rama true de cada ternaria = lo que ejecutaba el binario de competencia); solo se borró
  patrulla/inalcanzable; cero refs colgadas (el `pd` de `amix_motors` es el *parámetro* de las primitivas,
  otro scope — intacto).

## ⚠️ Pendiente (lo cierra el equipo — regla 1)
El binario CAMBIÓ (más chico) → ya no es bit-a-bit el validado. **Smoke-test en banco:** flashear
`central_robot2_arqueromix_quieto`, GO, confirmar que hace la MISMA secuencia (lateral → homing → quieto →
sigue la pelota → despeja → post-patada → acomoda → quieto). La conducta NO debería cambiar (solo se quitó
código inalcanzable) → es confirmar, no re-tunear. Si algo difiere = bug a reportar.

## Notas
- `central_robot2_arqueromix` (base) ahora es **igual** al `_quieto` (la patrulla ya no existe). El fallback
  real distinto sigue siendo `central_robot2_arquero` (FSM v3.3 vieja, intacta).
- `patear_atras()` en `amix_motors` quedó **sin uso** (era del retroceso de patrulla); se dejó (primitiva
  inocua) — se puede sacar en una limpieza menor posterior.
- Docs `DOCUMENTACION.md` / `FSM-ARQUERO-MIX-EXPLICADA.md`: banner agregado avisando que las secciones de
  patrulla describen código que ya no existe (historia del port 2025). Reescritura completa = post-Incheon.
