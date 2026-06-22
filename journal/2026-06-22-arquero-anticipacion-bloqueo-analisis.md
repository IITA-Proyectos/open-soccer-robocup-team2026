---
title: "Arquero — anticipar y bloquear la pelota con su velocidad/dirección (análisis + plan)"
date: 2026-06-22
status: vivo (análisis; S0 implementado, S1/S2/S3 pendientes)
placa: CENTRAL (ROBOT2, arquero)
autor: "Claude Opus 4.8 (1M context), vía Claude Code — pedido Virginia"
analisis: "workflow paralelo 6 agentes (web otros equipos + código + planta + síntesis + crítica adversarial)"
---

# Que el arquero anticipe y bloquee: análisis integral + soluciones SIMPLES

## Problema (Virginia, banco)

El arquero anda bien con la pelota LEJOS, pero CERCA a veces **pasa por al lado** y no la bloquea, o
**intenta patear y no acierta**. Hipótesis de Virginia: usar la **velocidad y dirección** de la pelota
(que cree que manda el TOP) para anticiparse; y cuando está cerca al costado, **strafe más fuerte** para
atajar.

## Verificación (código real, no de memoria)

- **El TOP SÍ manda la velocidad de la pelota:** `ball_vx_mm_s` / `ball_vy_mm_s` en el `WorldSnapshot`
  (`types.h:110-111`), poblados en `main_top.cpp` desde `cameras_get_ball_vx/vy`. Pero **`amix_comm` la
  DESCARTABA** (`g_aio` no tenía campos de velocidad). → **S0 la expone** (este commit).
- **Está filtrada:** `ball_velocity.cpp` aplica EMA α=0.4 + `valid` + expiry por gap (200 ms). No es
  derivada cruda.
- **CORRECCIÓN DE LATENCIA (load-bearing):** la premisa "~250 ms / 4 Hz" es FALSA para la pelota — ese
  número era de frescura del HEADING (`amix_config.h`). El env de competencia `top_robot2_pri` trae el
  snapshot por ISR a **100 Hz**; la pelota se refresca a ~30 Hz. **Latencia real sensor→g_aio ≈ 40-80 ms**,
  no 250. → la latencia NO mata la anticipación; lo que limita es la **velocidad del arquero** (~200 mm/s).
- **⚠️ La MAGNITUD de la velocidad NO está calibrada** (misma escala cruda que la posición). → usar
  **SÓLO el SIGNO/dirección** (robusto), nunca el valor en mm/s.

## Qué hacen otros equipos (fuentes públicas)

- **Posicionamiento "cubrir el ángulo":** el arquero se para sobre la línea pelota→centro del arco; lejos
  más adelantado, cerca más sobre la línea de gol (MSL, fútbol).
- **Predicción reactiva barata:** con 2 posiciones se extrapola linealmente dónde cruzará la línea de gol
  (paper Wheeled MSL, 90% acierto). **La latencia de cámara es la causa raíz de "llegar tarde / pasar al
  lado"; la predicción la compensa.**
- **Por qué pasa al lado (TIGERs, campeón SSL 2023):** frenar en seco en el punto de intercepción
  desperdicia tiempo; ellos usan *overshoot* (destino virtual más allá, no frenar antes) → casi duplica el
  alcance. Punto de intercepción más simple = pie de la perpendicular del robot a la línea de vuelo.
- **Pro (NO portable a este micro):** Kalman/EKF, RANSAC, parábola 3D con visión estéreo. Descartado.

## Factibilidad (honesta, con cálculos)

- Pelota CERCA (250 mm) a 1.5 m/s → tiempo de vuelo 167 ms. El arquero a 200 mm/s + kickstart recorre
  ~20-40 mm en esa ventana. **Con la pelota YA cerca y rápida casi no hay margen** → la única forma de
  ganar es estar al lado correcto ANTES de que esté cerca.
- La anticipación útil = el **signo** de la velocidad le dice a qué lado moverse **1-2 frames (33-66 ms)
  antes** que esperar a que el ÁNGULO de posición cruce ±8°. Ese atraso es justo lo que hoy la hace pasar
  por al lado.

## Soluciones SIMPLES (ordenadas; gateadas, sin tocar el candidato)

- **S0 — exponer la velocidad a `g_aio`** ✅ IMPLEMENTADA (este commit). ~6 líneas, **cero cambio de
  conducta**. Habilitador. **Paso de banco obligatorio: verificar el SIGNO** de `vx` (y qué `vy` es
  "acercándose") moviendo la pelota a mano, ANTES de cablear lógica.
- **S2 — arrancar a moverse por el SIGNO de `vx`** (pelota centrada pero que viene con componente lateral)
  ANTES de que el ángulo cruce la banda muerta. Gateado por flag nuevo + `ball_vel_valid`, fallback
  automático a la conducta de hoy. ~15 líneas. **Recomendado primero** (no toca el `pd` ni el lazo de rumbo).
- **S1 — strafe más fuerte cuando la pelota está CERCA y descentrada** (idea directa de Virginia). ~10
  líneas. ⚠️ **Riesgos (crítica adversarial):** (a) **gol en contra** — el umbral "cerca" se solapa con la
  zona de patada (250 mm); si el strafe fuerte sobrepasa, abre la boca del arco → capar para que SÓLO actúe
  FUERA de la zona de patada y techo de `pd`≤2.0 (no 2.2); (b) `pd` en `ad/aiproporcional` **amplifica
  también la corrección de rumbo** → a `pd` alto, serpenteo (skill control-pid-zona-muerta).
- **S3 — `ball_predict` (reusar el módulo PURO)** para apuntar a la X futura; usar sólo el signo del ángulo
  predicho. Opt-in, último, solo si S1/S2 no alcanzan.

## Descartado (con motivo)

Kalman/EKF/partículas; parábola 3D (cámara 2D); confiar en la MAGNITUD (sin calibrar); overshoot tipo
TIGERs (requiere reescribir el control de movimiento — 2027); tocar el candidato/competencia.

## Plan de banco (un cambio por vez — lo cierra el equipo)

1. **S0 (ya):** flashear el candidato (`central_robot2_arqueromix_quieto`) — **conducta idéntica**. Verificar
   el SIGNO de `vx`/`vy` en el monitor del TOP (`top_robot2_pri` lo trae despierto): mover la pelota izq→der
   y ver `vx` cambiar de signo; acercarla y ver `vy`. Anotar qué signo es "derecha" y qué es "acercándose".
2. Con los signos confirmados → **S2** gateada (env nuevo `..._anticipa`), con flag de inversión de signo.
3. Si hace falta → **S1** con el cap de distancia + techo de `pd`.

## Verificación

- `pio run -e central_robot2_arqueromix_quieto` → **SUCCESS** (S0). Candidato byte-idéntico en conducta.
- NO testeado en hardware (regla #1).
