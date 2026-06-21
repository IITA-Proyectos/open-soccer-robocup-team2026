---
title: "arqueromix — salida de la línea lateral a ciegas + commit (no quedarse enganchado)"
date: 2026-06-21
author: "Claude Opus 4.8 (1M context)"
requested-by: "Virginia Viollaz (banco)"
tipo: firmware-feature
toca-competencia: NO (solo src/arqueromix/, build aislado)
---

# Sesión 2026-06-21 (banco, 8ª iteración) — no quedarse enganchado en la línea lateral

## Reporte de Virginia
Patrullando hacia un costado, al llegar a la **línea lateral** el arquero se queda **enganchado**
(oscila ahí). Pide: hacer lo mismo que cuando va para atrás y detecta el blanco — un **movimiento
sin leer los sensores** — al tocar la línea lateral, y después seguir el movimiento que corresponde.
Ejemplo: detecta línea a la derecha → se mueve a ciegas (la misma distancia que el avance del homing)
→ luego patrulla hacia la izquierda, NO vuelve a la derecha.

## Causa
El rebote viejo (`impulso_der/izq`) no despegaba del todo de la línea (o la pelota la tiraba de
vuelta vía el seguimiento por lado), entonces el `moverce` siguiente re-detectaba la línea y
rebotaba de nuevo → oscilación ("enganchada").

## Fix
Estados nuevos (reemplazan `impulso_der/izq`):
- **`salir_linea_izq`** (tocó línea DERECHA): strafe IZQUIERDA **a ciegas** (sin leer ningún sensor)
  durante `AMIX_T_SALIR_LINEA`≈450 ms → `moverce_izquierda`.
- **`salir_linea_der`** (tocó línea IZQUIERDA): strafe DERECHA a ciegas → `moverce_derecha`.

**Commit ("no vuelve enseguida"):** al terminar la salida se arma `s_commit_until_ms = now +
AMIX_T_PATRULLA_COMMIT`(≈1 s). Durante esa ventana la patrulla IGNORA el lado de la pelota (no
flipea de dirección), así no se vuelve a meter en la línea que dejó. El despeje (pelota
cerca+centrada) y el rebote de la OTRA línea siguen activos. Pasada la ventana, vuelve a seguir la
pelota normal.

## Verificación
- **Compila SUCCESS** `central_robot2_arqueromix`.
- Competencia byte-idéntica (build aislado). NO validado en HW.

## Pendiente (Virginia / banco)
- Probar: al tocar la línea lateral, ¿se despega a ciegas y patrulla el otro lado sin engancharse?
- Tunear si hace falta: `AMIX_T_SALIR_LINEA` (subir si sigue enganchándose), `AMIX_T_PATRULLA_COMMIT`
  (subir si vuelve muy rápido hacia la línea).

## Referencias
- `src/arqueromix/DOCUMENTACION.md §17`. Journals del día: `2026-06-21-arqueromix-*`.