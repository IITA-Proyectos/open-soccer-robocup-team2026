# TASK-123 — Validar en banco el PATEO RECTO (trim) + CORTE DE RETROCESO POR PELOTA (arqueromix, 9º checkpoint)

- **Placa:** CENTRAL (R2, arquero — María). TOP `top_robot2_pri`, DOWN `down_robot2`.
- **Asignado:** equipo (banco) — María
- **Prioridad:** P2 (mejora de titración; los checkpoints validados NO se tocan hasta cerrar esto).
- **Estado:** abierta — **código listo, SIN banco.** Compila SUCCESS; los 8 checkpoints anteriores byte-idénticos (md5 verificados).
- **Build (banco):** `pio run -e central_robot2_arqueromix_recto_cortaretro -t upload`
- **Escape / rollback:** `pio run -e central_robot2_arqueromix_centrado_fino -t upload` (checkpoint #8, idéntico salvo estos 2 flags).
- **Flags:** `-DARQMIX_KICK_TRIM` + `-DARQMIX_RETRO_CUT_BALL` (sobre los flags del #8).

## Contexto (banco María 2026-07-04 con el #8)

- La velocidad +6% del #8 **"anda bastante bien"** (veredicto María — cierre formal de TASK-122 pendiente).
- Problema nuevo observado: **el despeje se desvía SIEMPRE a la IZQUIERDA.**
- Pedido nuevo: que durante el **retroceso post-pateo** también mire la pelota, y si la ve **corte el
  retroceso** y vuelva a buscarla / posicionarse.

## Qué hace (1): PATEO RECTO — `-DARQMIX_KICK_TRIM`

El golpe (`avanzar_patear`, M1=+vel / M2=−vel) es simétrico en PWM pero el robot curva a la izquierda
(asimetría física: eficiencia de motores / fricción / peso). Mismo remedio que usó el delantero centralmix
(`MIX_KICK_FWD_TRIM`, banco 2026-06-22): **PWM extra en la delantera IZQUIERDA (M1)** para que empuje más y
enderece. `AMIX_KICK_TRIM_PWM = 15` (valor en el PICO de la rampa; a media rampa aplica la mitad → corrección
pareja en todo el golpe). En el pico: M1 = 191+15 = 206 (lejos del tope 255).

**Titración:** sigue yéndose a la izquierda → subir de a 5 (20, 25…). Ahora tira a la DERECHA (sobre-corrigió)
→ bajar. El signo también acepta negativo (corrige un desvío a la derecha) por si el síntoma se invierte.

## Qué hace (2): CORTE DE RETROCESO POR PELOTA — `-DARQMIX_RETRO_CUT_BALL`

En `PATEANDO_atras` (el retroceso post-despeje, en sus 3 variantes: por línea / por tiempo / base), si
`ball_visible` y la pelota está a ≤ `AMIX_RETRO_CUT_DIST_MM` (default **9999 = cualquier pelota vista**,
pedido literal de María), el retroceso se corta (`parar()`) y vuelve a `esperar_quieto`, que ya sabe
seguirla / posicionarse / despejar.

**NO aplica al homing del GO** (`inicio_retroceder`): al arrancar el partido la pelota está en el centro y
SIEMPRE se ve → el corte habría matado el homing y el arquero nunca llegaría a su arco. Tampoco al re-homing
(que reusa ese estado) — el re-homing ya se dispara solo tras 15 s SIN pelota.

## Riesgos a mirar en banco

1. **Arquero adelantado:** con pelota LEJANA visible, corta el retroceso y espera adelantado sin volver a su
   línea (el re-homing del #7 lo devuelve recién a los 15 s sin pelota — pero con pelota A LA VISTA no).
   Si se ve en banco → bajar `AMIX_RETRO_CUT_DIST_MM` a ~800-1000 mm (corta solo con pelota cerca).
   ⚠️ La escala mm del snapshot está sin calibrar — titrar el número EN BANCO, no confiar en el valor nominal.
2. **Loop de despeje rápido:** pelota que no se va (contra un rival/pared) → patea → corta retro → patea…
   El anti-choque (heredado) cubre el caso rival; mirar que no quede pateando en bucle contra una pared.
3. **Trim vs +6%:** el trim se titró conceptualmente sobre el golpe de 191; si después se cambia la potencia
   global, re-mirar el trim (la asimetría física puede no escalar igual).

## Cómo validar (en orden)

1. **Pateo recto:** 4-5 despejes desde el centro del arco, pelota al frente → el robot debe salir RECTO
   (marcar en el piso la línea de salida; comparar A/B con el #8 si hay duda). Titrar el trim hasta que
   la desviación sea despreciable en la distancia del golpe (~550 ms).
2. **Corte del retroceso:** despeje normal, y DURANTE el retroceso tirarle la pelota a la vista →
   debe frenar el retroceso e ir a seguirla/posicionarse (y despejar si está cerca).
3. **Que el homing NO se corte:** GO con pelota visible en el centro → el homing del arranque debe
   completarse igual que siempre (retrocede hasta la línea + escape).
4. **NO-REGRESIÓN vs #8:** seguimiento, centrado fino, anti-choque, re-homing y despeje sin obstáculo,
   iguales. Riesgos heredados de TASK-122 siguen vigentes (retro a 85 no cruza línea; no oscila a 5°).

## Criterio de cierre (humano)

- El despeje sale recto (trim titrado y anotado). El retroceso corta al ver pelota y el arquero la juega.
  El homing del GO intacto. Sin regresión del #8. Repetible.

## Perillas

- `AMIX_KICK_TRIM_PWM` (15) en `amix_config.h` — PWM extra de M1 en el pico del golpe. Subir si sigue a la
  izquierda; bajar/negativo si tira a la derecha.
- `AMIX_RETRO_CUT_DIST_MM` (9999 = cualquiera) en `amix_config.h` — distancia máx. de pelota para cortar
  el retroceso. Bajar si el arquero queda adelantado.

## Relación

- Hereda TODO el linaje del #8 (`_centrado_fino`, TASK-122 — la parte de velocidad ya con veredicto
  "anda bastante bien" de María 2026-07-04) y sus pendientes (TASK-121 anti-choque vs pelota).
- Precedente del trim: centralmix `MIX_KICK_FWD_TRIM` (journal 2026-06-21/22, patada recta con OTOS).
