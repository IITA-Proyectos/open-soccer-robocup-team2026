---
title: "arqueromix — banco: la vuelta de 180° al inicio + bajar potencia del despeje"
date: 2026-06-21
author: "Claude Opus 4.8 (1M context)"
requested-by: "Virginia Viollaz (banco)"
tipo: firmware-tuning
toca-competencia: NO (solo src/arqueromix/, build aislado)
---

# Sesión 2026-06-21 (banco, 3ª iteración) — inicio 180° + potencia despeje

## Reporte de Virginia
Con el fix de la cámara (por ángulo) el arquero YA reacciona a la pelota. Pero:
1. Al arrancar **da una vuelta de 180°** y la cámara frontal queda mirando NUESTRO arco, no el
   del rival. Se movía lateralmente muy bien, pero quedó al revés.
2. Como quedó al revés, **no pudo verificar si seguía la pelota para el lado correcto.**
3. El despeje pega con **mucha potencia** → bajarla un poco.

## Diagnóstico (180°)
El strafe lateral anda bien → no es la cinemática del strafe. El 180° es **deriva de rumbo**
(yaw parásito del omni-3 en strafe abierto, ver skill `dinamica-omni-3-ruedas`). El arquero 2025
lo contrarrestaba con la corrección de rumbo de 3 bandas (modula la trasera según el `error` de
heading). En este robot esa corrección está **al revés** (mismo tipo de bug que el `OMEGA_SIGN`
del mixer 2026) o **no tiene heading válido** del TOP para corregir → deriva hasta 180°. No es algo
que MI fix de cámara haya cambiado: antes el robot se congelaba (banda muerta) y nunca llegaba a
patrullar, así que la deriva recién se ve ahora que patrulla.

## Cambios
- **Arranque más suave:** `AMIX_IMP_INI_FRONT` 90→70, `AMIX_IMP_INI_REAR` 153→110 (153 rozaba el
  techo térmico y daba un tirón al iniciar).
- **Perilla del signo de corrección:** flag `-DARQMIX_FLIP_HEADING` (env nuevo
  `central_robot2_arqueromix_flip`) → invierte el signo de la corrección de las 3 bandas. Si el
  arquero se da vuelta patrullando, se flashea ese env. ⚠️ La corrección SOLO actúa con
  `heading_valid=1` del TOP; si el heading no llega, ningún signo corrige (revisar el monitor).
- **Potencia del despeje bajada** (pedido): `AMIX_PATAD_M1` 250→180, `AMIX_PATAD_M2` 150→120,
  `AMIX_ATRAS` 150→120.

## Verificación
- **Compilan SUCCESS** `central_robot2_arqueromix` + `central_robot2_arqueromix_flip`.
- Competencia byte-idéntica (solo `src/arqueromix/`, build aislado).
- ⚠️ No validado en HW. Plan: DOCUMENTACION §14 — flashear base; si se da vuelta, flashear `_flip`
  y comparar; confirmar `heading_valid` del TOP en el monitor; recién con el arquero derecho
  verificar el seguimiento izq/der.

## Pendiente (Virginia / banco)
- Decir QUÉ env deja el arquero derecho (base o `_flip`) → ese queda como bueno.
- Confirmar que el TOP manda heading válido (si no, el strafe deriva sin corrección posible →
  tema del TOP, no del arqueromix).
- Una vez derecho: verificar seguimiento izq/der de la pelota + tunear `AMIX_TOL_CERCANIA_MM` del
  despeje.

## Referencias
- `src/arqueromix/DOCUMENTACION.md §14`. Fix de cámara previo: §13 + journal del mismo día.
- Yaw parásito del strafe: skill `dinamica-omni-3-ruedas`. Signo de rotación: `cinematica-omni-3-120`.
