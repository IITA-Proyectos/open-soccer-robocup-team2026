---
title: "arqueromix — salida de línea: más impulso + rebote inteligente en commit (no meterse de vuelta)"
date: 2026-06-21
author: "Claude Opus 4.8 (1M context)"
requested-by: "Virginia Viollaz (banco)"
tipo: firmware-tuning
toca-competencia: NO (solo src/arqueromix/, build aislado)
---

# Sesión 2026-06-21 (banco, 9ª iteración) — la salida de línea a veces se mete de vuelta

## Reporte de Virginia
Mejoró, pero a veces toca la línea, sale, y se vuelve a meter. No sabe si es que no mantiene el
rumbo hacia el lado opuesto o si no llega a salirse bien. Pide: ponerle un poco más de impulso al
movimiento a ciegas, y comprobar que no haya un problema con los tiempos.

## Diagnóstico
Dos causas:
1. La salida a ciegas usaba el mismo `pd` de la patrulla → poco impulso para despegarse.
2. **El DOWN NO distingue qué línea es** (da una sola señal `line_present`). Si la salida no
   despejaba del todo, el `moverce` re-detectaba la MISMA línea y, por la lógica de rebote
   (siempre al lado opuesto), se iba DE VUELTA hacia la línea → "toca, sale, vuelve a meterse".

Tiempos: revisados — cada estado usa su propio timer (`millis_inicio_estado` al entrar), el commit
es `millis() < s_commit_until_ms`. NO hay bug de tiempo; el problema era la dirección del rebote.

## Fix
- **Más impulso:** la salida (`salir_linea_der/izq`) usa `AMIX_PD_SALIR=1.9` (vs `pd=1.0` de la
  patrulla) → empuja más fuerte a ciegas para despegarse.
- **Rebote inteligente en commit:** mientras está en commit (recién salió de una línea), si el
  `moverce` vuelve a ver línea, asume que es la MISMA (no despejada) y **sigue saliendo para el
  mismo lado** (no rebota de vuelta). El commit se extiende hasta despejar. Pasado el commit, el
  rebote vuelve a ser normal (lado opuesto). Implementado en los dos `moverce` con el ternario
  `(millis() < s_commit_until_ms) ? mismo_lado : opuesto`.

## Verificación
- **Compila SUCCESS** `central_robot2_arqueromix`.
- Competencia byte-idéntica (build aislado). NO validado en HW.

## Pendiente (Virginia / banco)
- Probar: al tocar la línea lateral, ¿ahora se despega bien y NO se vuelve a meter?
- Tunear si hace falta: `AMIX_PD_SALIR` (1.9, subir si aún no se despega), `AMIX_T_SALIR_LINEA`
  (450 ms, más distancia a ciegas), `AMIX_T_PATRULLA_COMMIT` (1000 ms).

## Referencias
- `src/arqueromix/DOCUMENTACION.md §17 + §17.1`. Journals del día: `2026-06-21-arqueromix-*`.