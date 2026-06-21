---
title: "arqueromix — banco: el inicio va para adelante; safety 50s + retroceso dedicado/flippable"
date: 2026-06-21
author: "Claude Opus 4.8 (1M context)"
requested-by: "Virginia Viollaz (banco)"
tipo: firmware-debug
toca-competencia: NO (solo src/arqueromix/, build aislado)
---

# Sesión 2026-06-21 (banco, 6ª iteración) — el inicio va para adelante

## Reporte de Virginia
Apenas le da el GO, el robot **se mueve hacia ADELANTE** (debería ir hacia atrás a buscar la línea
del área). Pide: sacar los 4 s del safety y poner **50 s** (temporal, después se baja). Y recuerda
que **los motores deben moverse con PWM** "por si no lo tenés así".

## Diagnóstico (dos causas posibles, sin poder ver el HW)
El retroceso del homing usaba `patear_atras()` (M1=-, M2=+), que es la MISMA dirección que el
retroceso del despeje — y el despeje "quedó dentro de todo bien" (vuelve), así que ese patrón
debería ir hacia ATRÁS. Si igual va para adelante, es por una de dos:
- **(a)** el robot arranca **sobre una línea** → `line_present`=true al GO → `inicio_retroceder`
  saltea el retroceso y pasa directo a `inicio_avanzar` (que sí va adelante). Encaja con "apenas le
  doy el go se mueve hacia adelante".
- **(b)** el retroceso está **invertido** en este robot (las convenciones de este robot ya
  resultaron distintas: el signo de rumbo hubo que darlo vuelta).

## Cambios
- **Safety 50 s** (pedido): `AMIX_T_INICIO_RETRO_SAFETY` 4000→50000 (TEMPORAL; bajar luego).
- **Retroceso de inicio = primitiva DEDICADA** `retroceder_inicio()` (`amix_motors`): PWM propio
  `AMIX_INICIO_RETRO_PWM=100` (controlado, no a tope — confirma que va por PWM) y **dirección
  flippable** `AMIX_INICIO_RETRO_SIGN`. Ya no comparte velocidad/sentido con el retroceso del despeje.
- **Env nuevo `central_robot2_arqueromix_retroflip`** (`-DARQMIX_FLIP_INICIO_RETRO`): si al GO va
  para adelante, se flashea ese para invertir el retroceso del homing.
- Confirmado: TODOS los motores se mueven con PWM (`analogWrite` vía `amix_set_motor`).

## Verificación
- **Compilan SUCCESS** `central_robot2_arqueromix` + `_retroflip`.
- Competencia byte-idéntica (build aislado). NO validado en HW.

## Pendiente (Virginia / banco) — test A/B con STOP a mano
1. Flashear `central_robot2_arqueromix`. Dar GO con el dedo en STOP (`s`): ¿va para atrás o adelante?
2. Si va para **adelante** → flashear `central_robot2_arqueromix_retroflip` y repetir.
   - Si con el flip va para **atrás** = era el sentido (caso b) → lo dejamos así.
   - Si con ambos arranca yendo adelante apenas hay línea = arranca sobre el blanco (caso a) →
     se resuelve distinto (avisame y lo hago).
3. Decirme el resultado para cerrar el sentido y después bajar el safety de 50 s a ~4 s.

## Referencias
- `src/arqueromix/DOCUMENTACION.md §16`. Iteraciones previas del día: journals `2026-06-21-arqueromix-*`.