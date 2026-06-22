# TASK-118 — Validar en banco la jugada "pelota atrás" (cámara trasera) del centralmix

- **Placa:** CENTRAL (R1, delantero) + depende del TOP con las 2 cámaras (frontal + trasera) dando pelota.
- **Asignado:** equipo (banco) — Elías
- **Prioridad:** P1 (sin esto, cada vez que la pelota cae atrás el delantero queda clavado ~10 s).
- **Estado:** abierta
- **Build:** `pio run -e central_robot1_mix_bno -t upload` (✅ compilado por Claude 2026-06-22, ambos envs SUCCESS).

## Por qué
Cuando la cámara TRASERA ve la pelota (queda detrás, `ball_y_cm<0`, `angulo≈±180`), el apuntado fino
2025 giraba a 35 PWM (bajo el piso `{70,70,107}` → zumba sin girar) y el ángulo saltaba ±180 → el
delantero quedaba **clavado ~10 s**. Ahora gira EN EL LUGAR a `MIX_ATRAS_PWM` (sobre el piso) hasta
encarar la pelota. Diseño + red-team: journal `2026-06-22-centralmix-jugada-pelota-atras.md`.

## Debug USB (115200, USB de la CENTRAL)
`pio device monitor -b 115200`. Campos nuevos en la línea: `giro_atras=` (sentido latcheado:
0=inactivo, +1/-1=girando). Mirar también `x`, `y` (ball_y_cm), `ang` (anguloPelota), `estado`.

## Cómo validar (en orden — una perilla por vez)
1. **SIGNO / camino corto (robot LEVANTADO, ruedas al aire):** poné la pelota ATRÁS (que la vea la
   cámara trasera). Confirmá que las 3 ruedas giran al MISMO sentido y `giro_atras` se LATCHEA en +1 o
   −1 (no cambia). Soltá la pelota hacia el frente y mirá: ¿el robot la encara por el lado **CORTO**?
   - Si encara por el lado **LARGO** (~340°) → poné `MIX_ATRAS_DIR_SIGN = -1` en `mix_config.h`.
   - Anotá qué sentido físico (CW/CCW) da +pwm (hoy está `<RE-VERIFICAR>` en el código).
2. **PISO / zumbido (robot EN EL PISO):** pelota atrás → ¿gira de VERDAD a 120, o zumba clavado como
   con 35? Si zumba → subir `MIX_ATRAS_PWM` 120→130→140. Si el regulador hace **brownout** (se
   resetea/parpadea) → bajar y revisar corriente.
3. **GIRO PURO (no traslada):** durante el giro, ¿el robot rota EN EL LUGAR o se desplaza? Si la
   trasera M3 (piso 107) no rompe inercia, las delanteras lo arrastran en arco. Importante cerca de la
   línea propia. Si se desplaza → subir `MIX_ATRAS_PWM` (más margen para M3).
4. **ANTI-DITHER (clave):** pelota JUSTO atrás y centrada (ball_x≈0, ruidoso). Confirmá por USB que
   `giro_atras` queda FIJO aunque `ang` salte +180↔−180. Éxito = gira en un solo sentido, sin temblar.
5. **ENCARE COMPLETO:** barrer la pelota de atrás al frente. Cronometrar pelota-atrás → apuntada:
   objetivo **<2 s** (vs ~10 s actual), SIN re-clavarse en el rango medio (140°→15°). Al quedar
   apuntada (`|ang|<15`) debe pasar a AVANZANDO, no a IMPULSO_INICIAL.
6. **REBOTE LATERAL:** pelota atrás-pero-de-costado (ball_y≈−7, ball_x grande) → debe girar igual
   (entra por `ball_y`, no por el ángulo).
7. **NO se va a IMPULSO con la pelota a la vista atrás:** si la cámara trasera la ve estable,
   `millis_pelota` se refresca → no debería saltar a GIRANDO. Si la cámara PARPADEA (tapar/destapar),
   puede entrar en churn APUNTAR↔GIRANDO; anotar si pasa (fix: subir umbral de pérdida para el caso atrás).
8. **A-B / KILL-SWITCH:** compilar con `MIX_ATRAS_Y_ENTRA = 9999` → debe volver al comportamiento de
   hoy (clavado 10 s). Prueba que el gate es la única causa de la mejora.
9. **NO-REGRESIÓN:** pelota al FRENTE → APUNTAR/AVANZAR/CENTRAR/**PATADA** idénticos (la patada del
   2026-06-21 NO debe cambiar; el giro-atrás solo dispara con `ball_y<−6`).

## Criterio de cierre (humano)
- Encara pelota-atrás en **<2 s** sin oscilar, sin re-clavarse, **5/5** repeticiones, sin brownout, giro
  puro (no se desplaza). El signo (`MIX_ATRAS_DIR_SIGN`) confirmado. La patada que sigue NO sale al
  arco propio (CENTRANDO alinea). Kill-switch revierte.

## Escape / rollback
`MIX_ATRAS_Y_ENTRA = 9999` (kill-switch, sin recompilar lógica) o cualquier env de competencia. La
jugada vive 100% dentro de `APUNTAR_PELOTA` del centralmix (build aislado).

## Mejora futura (2027, detrás de flag — NO ahora)
Sesgo **goal-aware**: elegir el sentido del giro con `goal_opp_angle` para quedar del lado correcto del
arco rival. Requiere el signo físico fijado en banco (paso 1) ANTES de encadenar el signo del arco.

## Relación
- TASK-113/115/116/117 (misma rama centralmix). El giro-atrás NO toca la patada (TASK-117).
