# TASK-122 — Validar en banco el CENTRADO FINO del arquero (arqueromix, 8º checkpoint)

- **Placa:** CENTRAL (R2, arquero — María). TOP `top_robot2_pri`, DOWN `down_robot2`.
- **Asignado:** equipo (banco) — María
- **Prioridad:** P2 (mejora de titración; los checkpoints validados NO se tocan hasta cerrar esto).
- **Estado:** abierta — **código listo, SIN banco.** Compila SUCCESS; los 7 checkpoints anteriores byte-idénticos (md5 verificados).
- **Build (banco):** `pio run -e central_robot2_arqueromix_centrado_fino -t upload`
- **Escape / rollback:** `pio run -e central_robot2_arqueromix_evita_lejos_rehome -t upload` (checkpoint #7, idéntico salvo este flag) o cualquier checkpoint anterior.
- **Flag:** `-DARQMIX_CENTRADO_FINO` (sobre los flags del #7).

## Qué hace (pedido María 2026-07-03)

Baja la banda muerta angular del seguimiento de pelota `AMIX_TOL_CENTRADO_DEG` de **8° a 5°** (`amix_config.h`,
gateado). Motivo (banco María con `_evita_lejos`): la banda es POR ÁNGULO, así que el corrimiento lateral que
acepta **crece con la distancia** (≈ distancia × tan(tol)): a 8° ≈ 14% → pelota a 1,5 m = el arquero podía
quedar hasta ~21 cm corrido del eje de la pelota y darse por "centrado". A 5° baja a ≈ 9% (~13 cm a 1,5 m).

**Qué NO arregla:** el "queda torcido / no de frente". En `esperar_quieto` nada re-orienta al robot (la
orientación solo se corrige en el despeje y en el homing/re-homing). El re-homing del #7 (heredado acá) lo
mitiga cada 15 s sin pelota. Si el torcido persiste con pelota a la vista → tema aparte (posible #9:
re-orientarse esperando; requiere diseño propio, el cero del heading del TOP deriva).

## Riesgo principal a mirar en banco

Banda angosta + piso de PWM de los motores = el arquero puede quedar **"cazando" la pelota lejana con
micro-strafes que nunca terminan** (oscilación alrededor de la banda muerta, ruido angular de la cámara
incluido). Eso gasta batería, calienta motores y lo puede correr de posición.

## Cómo validar (en orden)

1. **Centrado a distintas distancias:** pelota quieta a ~0,5 / 1 / 1,5 m, descentrada → el arquero sigue y
   para MÁS enfrentado que con el #7 (comparar A/B a ojo o con marcas en el piso).
2. **Que NO oscile:** pelota quieta lejos (1,5-2 m) apenas fuera del eje → el arquero debe PARAR y quedarse
   quieto (micro-strafes que no terminan = FALLO → subir a 6° y repetir).
3. **Titración** (en `amix_config.h`, bajo el `#ifdef`): oscila → 6°; sigue quedando corrido → 4° (no menos).
4. **NO-REGRESIÓN vs #7:** despeje, anti-choque, re-homing y homing iguales; en particular que el despeje
   no se demore (la tolerancia de KICK es otra, `AMIX_TOL_KICK_DEG=30`, no se tocó).

## Criterio de cierre (humano)

- A 1,5 m el arquero queda visiblemente mejor enfrentado a la pelota que con el #7, sin oscilar en ninguna
  distancia. Valor titrado anotado. Sin regresión del despeje/anti-choque/re-homing. Repetible.

## Perilla

- `AMIX_TOL_CENTRADO_DEG` (5° bajo el flag; 8° sin él) en `amix_config.h`. Subir = para antes (menos centrado,
  más estable); bajar = sigue más (mejor centrado, más riesgo de oscilar).

## Relación

- Hereda TODO el linaje del #7 (`_evita_lejos_rehome`, tag `arquero-rehome-checkpoint-2026-07-03`) y sus
  pendientes: chequeo BLOQUEANTE pelota vs anti-choque (TASK-121) + primer banco del re-homing.
- Origen: observación de María en banco con `_evita_lejos` (queda centrado lejos de la pelota y torcido).
