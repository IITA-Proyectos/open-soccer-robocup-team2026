# TASK-122 — Validar en banco el CENTRADO FINO + POTENCIA +6% del arquero (arqueromix, 8º checkpoint)

- **Placa:** CENTRAL (R2, arquero — María). TOP `top_robot2_pri`, DOWN `down_robot2`.
- **Asignado:** equipo (banco) — María
- **Prioridad:** P2 (mejora de titración; los checkpoints validados NO se tocan hasta cerrar esto).
- **Estado:** abierta — **código listo, SIN banco.** Compila SUCCESS; los 7 checkpoints anteriores byte-idénticos (md5 verificados).
- **Build (banco):** `pio run -e central_robot2_arqueromix_centrado_fino -t upload`
- **Escape / rollback:** `pio run -e central_robot2_arqueromix_evita_lejos_rehome -t upload` (checkpoint #7, idéntico salvo estos 2 flags) o cualquier checkpoint anterior.
- **Flags:** `-DARQMIX_CENTRADO_FINO` + `-DARQMIX_POWER_106` (sobre los flags del #7).

## Qué hace (pedido María 2026-07-03)

Baja la banda muerta angular del seguimiento de pelota `AMIX_TOL_CENTRADO_DEG` de **8° a 5°** (`amix_config.h`,
gateado). Motivo (banco María con `_evita_lejos`): la banda es POR ÁNGULO, así que el corrimiento lateral que
acepta **crece con la distancia** (≈ distancia × tan(tol)): a 8° ≈ 14% → pelota a 1,5 m = el arquero podía
quedar hasta ~21 cm corrido del eje de la pelota y darse por "centrado". A 5° baja a ≈ 9% (~13 cm a 1,5 m).

**Qué NO arregla:** el "queda torcido / no de frente". En `esperar_quieto` nada re-orienta al robot (la
orientación solo se corrige en el despeje y en el homing/re-homing). El re-homing del #7 (heredado acá) lo
mitiga cada 15 s sin pelota. Si el torcido persiste con pelota a la vista → tema aparte (posible #9:
re-orientarse esperando; requiere diseño propio, el cero del heading del TOP deriva).

## Qué hace además: POTENCIA +6% PROPORCIONAL (pedido María 2026-07-04, `-DARQMIX_POWER_106`)

TODAS las potencias del camino vivo del modo quieto suben un **6% en la misma proporción** (la relación entre
ruedas y entre movimientos se conserva — solo cambia la velocidad general). Factor único `AMIX_POWER_SCALE`
(`amix_config.h`), aplicado UNA vez por valor: en los PWM fijos de las primitivas y en los `pd` de los strafes
(NO en los `AMIX_PROP_*`, que ya multiplican por `pd`).

| Movimiento | Antes | Con +6% |
|---|---|---|
| Seguir pelota (delanteras/trasera, rumbo centrado) | 75 / 133 | 79 / 141 |
| Strafes suaves (pd base) | 0.85 | 0.901 |
| Avanzar (buscar pelota) | 100 | 106 |
| Homing: retro / avance | 100 / 75 | 106 / 80 |
| Golpe de despeje (pico rampa) | 180 | 191 |
| Retro post-pateo (quieto) | 80 | 85 ⚠️ |
| Freno de patada | 200 | 212 |
| Giro alinear arco / orientarse | 90 / 50 | 95 / 53 |

NO escala (a propósito): `AMIX_ATRAS`=120 e impulso inicial (código muerto en quieto), `FORWARD_BIAS` (off),
`ROT_MAX`/`KP_RUMBO` (sin uso en el camino quieto).

## Riesgos principales a mirar en banco

1. Banda angosta + piso de PWM de los motores = el arquero puede quedar **"cazando" la pelota lejana con
   micro-strafes que nunca terminan** (oscilación alrededor de la banda muerta, ruido angular de la cámara
   incluido). Eso gasta batería, calienta motores y lo puede correr de posición. El +6% en el strafe de
   seguimiento AGRAVA este riesgo (más velocidad al cruzar la banda = más sobrepaso).
2. **Retro post-pateo 80 → 85:** el 80 se había bajado A PROPÓSITO (a 120 cruzaba la línea por inercia).
   Mirar que con 85 siga parando JUSTO en la línea sin meterse al área.
3. **Golpe 180 → 191 + `KICK_FAR` (550 ms):** más envión — confirmar que el freno de patada (ahora 212)
   sigue conteniéndolo cuando pisa línea pateando.

## Cómo validar (en orden)

1. **Centrado a distintas distancias:** pelota quieta a ~0,5 / 1 / 1,5 m, descentrada → el arquero sigue y
   para MÁS enfrentado que con el #7 (comparar A/B a ojo o con marcas en el piso).
2. **Que NO oscile:** pelota quieta lejos (1,5-2 m) apenas fuera del eje → el arquero debe PARAR y quedarse
   quieto (micro-strafes que no terminan = FALLO → subir a 6° y repetir).
3. **Retro post-pateo a 85 (riesgo #2):** provocar 3-4 despejes y mirar que el retroceso pare en la línea
   sin cruzarla. Si se mete → probar sin `-DARQMIX_POWER_106` para aislar si fue el +6%.
4. **Titración banda** (en `amix_config.h`, bajo el `#ifdef`): oscila → 6°; sigue quedando corrido → 4° (no menos).
5. **NO-REGRESIÓN vs #7:** despeje, anti-choque, re-homing y homing iguales; en particular que el despeje
   no se demore (la tolerancia de KICK es otra, `AMIX_TOL_KICK_DEG=30`, no se tocó).

## Criterio de cierre (humano)

- A 1,5 m el arquero queda visiblemente mejor enfrentado a la pelota que con el #7, sin oscilar en ninguna
  distancia. Valor titrado anotado. El +6% no lo hace cruzar la línea (ni en el retro post-pateo ni pateando).
  Sin regresión del despeje/anti-choque/re-homing. Repetible.

## Perillas

- `AMIX_TOL_CENTRADO_DEG` (5° bajo el flag; 8° sin él) en `amix_config.h`. Subir = para antes (menos centrado,
  más estable); bajar = sigue más (mejor centrado, más riesgo de oscilar).
- `AMIX_POWER_SCALE` (1.06 bajo `-DARQMIX_POWER_106`; 1.0 sin él) en `amix_config.h`. Es EL factor global:
  cambiarlo mueve todas las potencias en proporción. Para aislar problemas, quitar el flag del env (vuelve
  a las potencias históricas sin perder el centrado fino).

## Relación

- Hereda TODO el linaje del #7 (`_evita_lejos_rehome`, tag `arquero-rehome-checkpoint-2026-07-03`) y sus
  pendientes: chequeo BLOQUEANTE pelota vs anti-choque (TASK-121) + primer banco del re-homing.
- Origen: observación de María en banco con `_evita_lejos` (queda centrado lejos de la pelota y torcido).
