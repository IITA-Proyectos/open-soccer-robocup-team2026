---
date: 2026-06-04
status: vivo
tipo: plan-port
titulo: Estrategias de programas anteriores vs firmware nuevo — plan de port priorizado
autor: editor tecnico (consolidacion de 4 analisis de frente)
fuentes:
  - TRACK 1 — Delantero 2025 (definitivo-delantero.cpp + delantero-sin-zirconLib.cpp + zirconLib)
  - TRACK 2 — Arquero legacy (docs/internal/analisis-arquero-legacy.md + analisis-definitivo-arquero.md)
  - TRACK 3 — FSM alternativa archivada (_archive/, diseno 2026-05-18, nunca integrado)
  - TRACK 4 — Arquitectura y lecciones del 2025 (research/completed/2026-02-21-arquitectura-sistema-2025.md)
---

# Estrategias anteriores vs firmware nuevo — plan de port priorizado

> **Para el coach, en una frase:** los programas viejos eran monoliticos y ad-hoc; el firmware
> nuevo ya es **tacticamente superior y mejor estructurado** en casi todo. NO hay que "recuperar
> la estrategia vieja". Lo unico que vale portar son **un punado de heuristicas y casos-borde
> concretos**, casi todos FUERA del cerebro (strategy.cpp) y de bajo riesgo. El bloqueante real
> para Incheon **no es la estrategia, es la vision sin recalibrar (TASK-022)**.

---

## 1. Resumen para el coach

### Que hacian tacticamente los programas anteriores

- **Delantero 2025** (TRACK 1): FSM plana de ~25 estados por timers. Giraba en el lugar buscando
  la pelota, frenaba por inercia al verla, apuntaba por el signo del angulo, avanzaba, y su **joya
  tactica** era el ORBIT-AROUND-THE-BALL (estados CENTRANDO): rotaba el conjunto robot+pelota
  alrededor de la pelota con impulsos anti-friccion hasta alinear el tiro con el arco rival, y
  recien ahi empujaba/pateaba. Manejo de borde replicado en casi todos los estados.

- **Arquero legacy** (TRACK 2): el "definitivo" (~900 lineas) patrullaba oscilando lateralmente
  entre lineas con correccion giroscopica del drift, subia la velocidad 50% al ver la pelota,
  reaccionaba desde mas lejos (tol 140 vs 50 del delantero) y ejecutaba un despeje temporizado al
  tener la pelota centrada y cerca. El sketch 2025 crudo NO tiene estrategia util (loop vacio, BNO
  nunca inicializado).

- **FSM archivada** (TRACK 3): rediseno 2026 de 3 modulos C++ puros, **nunca integrado**. Una FSM
  Moore simple (downgrade de la viva), una tabla de decision tactica (cuando NO tocar la pelota), y
  un evaluador de seguridad de borde que **rescata el escape_angle ya calculado por DOWN**.

- **Sistema 2025 campeon** (TRACK 4): 2 robots, todo en coordenadas relativas de camara, sin pose
  ni heading (el BNO estaba comentado y **ganaron asi**). FSM minima de 4 estados (GIRANDO ->
  AVANZANDO -> CENTRANDO -> PATEANDO). El analisis forense probo que el codigo del repo **no es el
  que gano** (lleno de bugs); lo que funciono fue una version mas simple y robusta.

### Veredicto: cuanto ya esta en el nuevo vs cuanto falta

| Capacidad | Estado en el nuevo |
|---|---|
| Kickoff / arranque | YA (boost 250 ms) — superior |
| Busqueda por giro | YA (SEARCH avanza+gira simultaneo) |
| Behind-the-ball / alinear antes de empujar | YA y MEJOR (POSITION = target geometrico detras de la pelota; el viejo lo hacia con orbita fisica) |
| Aproximacion + empuje por inercia | YA (APPROACH/CLEAR) |
| Anticipacion del arquero | YA y MEJOR (ball_predict + bt_classify de amenaza) |
| Posicionamiento del arquero por arco propio | YA (goal_own v3 + gk_own_goal_orient + cross_track) |
| Manejo de linea | YA y MAS RICO (line_angle/cross_track de DOWN + LINE_AVOID con histeresis) |
| Control de heading | YA y MAS SEGURO (HeadingPID con clamp <=327 anti-overflow int16) |
| **Despeje DIRECCIONAL (a la banda, no al centro)** | **NO — gap real compartido** |
| **Consumir escape_angle precalculado de DOWN en LINE_AVOID** | **NO — DOWN lo calcula y el cerebro lo tira (usa line_angle+180)** |
| **Ventana de gracia al perder la pelota** | **NO — salta a SEARCH al primer frame sin deteccion** |
| **Boost anti-friccion al arrancar movimientos lentos** | **NO — emite vx/vy/omega sin breakaway** |
| Profundidad del arquero respecto de su arco | PARCIAL (solo via pose absoluta nivel 3, no cableada) |
| Politica "no tocar la pelota si ya va al arco rival" | NO (gap conceptual, riesgo alto) |
| **Redondez/circularidad de la pelota en camara** | **verificar en TASK-022 (vision)** |

**Conclusion honesta:** la arquitectura nueva (CENTRAL decide vx/vy/omega; modulos puros
host-testeados; PIDs con clamp; fallbacks exactos; ~566 tests verdes) **gana por estructura**. El
valor a portar son HEURISTICAS y CASOS-BORDE puntuales, **no la estructura ni la FSM vieja**.
Reintroducir cualquier FSM monolitica seria un downgrade.

---

## 2. Tabla por funcionalidad / estrategia

Leyenda "tiene?": SI / PARCIAL / NO. Recomendacion: PORTAR / YA-CUBIERTO / OBSOLETO.

| Funcionalidad | Que hacia el viejo | Tiene? | Recomendacion | Valor | Riesgo |
|---|---|---|---|---|---|
| Ventana de gracia al perder pelota | Delantero: 500 ms antes de re-buscar | NO | **PORTAR** | alto | bajo |
| Boost anti-friccion (breakaway) | Delantero: IMPULSO_INICIAL 70 ms@PWM150 al arrancar lento | NO | **PORTAR** | medio | bajo |
| Filtro de redondez/circularidad de pelota | 2025/Delantero: rechazar blobs naranjas no esfericos en camara | verificar | **PORTAR (vision)** | alto | bajo |
| Consumir escape_angle de DOWN en LINE_AVOID | FSM archivada (field_safety.fs_eval) | NO | **PORTAR** | alto | medio |
| Despeje DIRECCIONAL del arquero (a la banda) | Arquero (sugerido D3, no implementado) | NO | **PORTAR (gated)** | alto | alto |
| Despeje corto hacia adelante al tocar linea si apunta al arco | Delantero (CENTRANDO->PATEANDO_corto) | NO | **PORTAR (gated)** | medio | alto |
| Velocidad adaptativa al ver pelota (boost ~50%) | Arquero (pd 1.0->1.5) | PARCIAL (kp_scale) | **PORTAR (gated)** | medio | medio |
| Avance exploratorio por tiempo en busqueda larga | Delantero (AVANZANDO_POR_TIEMPO, 9 s) | PARCIAL | **PORTAR (gated)** | medio | medio |
| Commit de empuje por alineacion lateral pelota-arco | Delantero (|Yp-Ycontrincante|<=30) | PARCIAL (is_aligned_to_push calculado pero solo documenta) | **PORTAR (gated)** | medio | medio |
| Control de profundidad del arquero (HC-SR04) | 2025/Arquero: mantener distancia a su arco | PARCIAL (solo pose nivel 3) | **PORTAR (modulo puro)** | medio | medio |
| Reposicion explicita post-despeje | Arquero: retroceder a linea + avanzar 1 s | PARCIAL (cross_track) | PORTAR post-Incheon | medio | medio |
| Espera por inercia al detectar pelota | Delantero: frena 700-1000 ms antes de apuntar | NO | EVALUAR (solo si deteccion ruidosa) | bajo | bajo |
| Degradacion conservative ante data_valid==0 fresco | FSM archivada (fs_eval) | PARCIAL | PORTAR si sobra tiempo | bajo | bajo |
| Politica LET_CIRCULATE (no tocar si va al arco rival) | FSM archivada (play_decision) | NO | post-Incheon (riesgo alto) | medio | alto |
| Estado DEFEND del delantero (pelota detras) | FSM archivada (ball_y<=-400) | NO | post-Incheon | bajo | alto |
| LEDs de deteccion en camara para debug en cancha | 2025 | verificar | PORTAR (vision, fw N6) | medio | bajo |
| Anti-jam en extremos de la patrulla | Arquero (impulso_izq/der 350 ms) | YA (cross_track + OSCILLATE_PERIOD) | YA-CUBIERTO | bajo | bajo |
| Orbita fisica alrededor de la pelota | Delantero (CENTRANDO + impulsos) | YA y MEJOR (behind_ball/POSITION) | YA-CUBIERTO | — | — |
| Correccion giroscopica del drift | Delantero/Arquero (aiproporcional/adproporcional) | YA (HeadingPID + drive_straight OTOS) | YA-CUBIERTO | — | — |
| Centrado pelota+arco antes de comprometer | 2025 (CENTRANDO) | YA y MEJOR (ball_is_in_attack_line + POSITION) | YA-CUBIERTO | — | — |
| Retroceso direccional por sensor de linea | Delantero (s1/s2/s3 -> diagonal distinta) | YA y MAS RICO (line_angle geometrico) | YA-CUBIERTO | — | — |
| Eleccion de arco por color | Hardcodeado a amarillo | YA (strategy_set_attack_color / goal_own/opp v3) | YA-CUBIERTO | — | — |
| Kicker / solenoide / estado PATEANDO | Todos (full power temporizado) | N/A (robot empuja por inercia) | **OBSOLETO** | — | — |
| zirconLib / acceso directo a pines | Delantero/2025 | N/A (CENTRAL emite vx/vy/omega) | **OBSOLETO** | — | — |
| Protocolo camara por headers 201-204, datos 0-200 | Todos | N/A (WorldSnapshot v3 + camara->TOP v2 con CRC8) | **OBSOLETO** | — | — |
| Convencion de ejes/signos vieja | Todos (atan2(Yp,Xp), signos H/AH por rueda) | N/A (convencion nueva: +Y=frente, +X=der, omega CCW+) | **OBSOLETO** | — | — |
| Heading PID kp=0.3 sin clamp | Todos | N/A (HeadingPID con clamp <=327) | **OBSOLETO (peligroso)** | — | — |
| FSM strategy_core completa (Moore, sin PID) | FSM archivada | N/A (downgrade de la viva) | **OBSOLETO** | — | — |
| Deteccion variante Zircon Mark1/Naveen1 por pin 32 | 2025 | N/A (config_central.h: MOTOR_INVERT/WHEEL_ANGLES) | **OBSOLETO** | — | — |
| 8 IR en anillo como pelota 360 | 2025 (instalados, sin usar) | NO (HW no presente) | post-Incheon (HW nuevo) | bajo | alto |

---

## 3. Plan de port priorizado (orden valor/riesgo)

> Regla transversal (CLAUDE.md + memoria): cualquier cosa que toque `strategy.cpp` (el cerebro) a
> ~26 dias de Incheon va como **modulo puro host-testeable**, **gated OFF por default** y con
> **fallback EXACTO byte-identico** al comportamiento actual, y se valida en banco antes de confiar.
> NO copiar el sketch Arduino: extraer el CONCEPTO y reimplementarlo al estilo nuevo.

### AHORA SEGURO (no toca el cerebro)

1. **Ventana de gracia al perder la pelota** — origen: Delantero 2025.
   - Valor alto / riesgo bajo. No saltar a SEARCH al primer frame sin deteccion.
   - Como: helper `world_model_ball_recently_seen(ms)` (o extender `world_model_ball_visible()`)
     usando el timestamp del ultimo snapshot con pelota; ventana 300-500 ms. APPROACH/POSITION/
     INTERCEPT ya consultan ball_visible, asi que mejora sin tocar la FSM. Solo extiende la verdad
     de "veo pelota" unos ms; tunear la ventana en banco.

2. **Filtro de redondez/circularidad de la pelota en la camara** — origen: 2025 / Delantero.
   - Valor alto / riesgo bajo. Es trabajo de VISION (TASK-022), NO de strategy.cpp.
   - Como: al recalibrar el LAB en las 2 N6, conservar el score de circularidad
     (pixels/area del circulo equivalente) y descartar bajo umbral. Reduce falsos positivos de
     pelota que envenenan toda la FSM. Cero riesgo para el cerebro.

3. **Boost anti-friccion (breakaway) al arrancar movimientos lentos** — origen: Delantero 2025.
   - Valor medio / riesgo bajo. Portar el concepto IMPULSO_INICIAL a la CAPA BAJA
     (motors_zircon / kinematics), NO a strategy.cpp.
   - Como: si |comando de rueda| esta entre 0 y un minimo de breakaway durante N ms tras arrancar
     desde reposo, elevarlo al minimo. Asi SEARCH, POSITION y GK PATROL se benefician sin tocar el
     cerebro. Implementar como funcion pura aplicada al wheel command (host-testeable). Tunear el
     piso en banco.

4. **LEDs de deteccion en la camara para debug en cancha** — origen: 2025.
   - Valor medio / riesgo bajo. Trabajo de VISION. LED por objeto detectado para diagnostico
     instantaneo en Incheon sin laptop.
   - Como: usar el metodo de LED soportado por la fw 4.8.1 de la N6 (OJO: `pyb.LED` crashea en esa
     fw — ver memoria vision_openmv_n6). NO copiar el pyb.LED del H7.

### TRAS BANCO (toca el cerebro; gated OFF + fallback exacto; activar solo con banco)

5. **Consumir el escape_angle precalculado de DOWN en LINE_AVOID** — origen: FSM archivada
   (field_safety.fs_eval).
   - Valor alto / riesgo medio. DOWN ya envia `escape_angle_centideg` en LineStatusV2
     (`src/shared/types.h:140`), pero el world_model de CENTRAL no lo expone; LINE_AVOID usa
     `line_angle+180` (perpendicular ingenua, falla en esquinas/curvas del ring).
   - Como: agregar `world_model_get_escape_angle_deg()` + `world_model_escape_valid()` que lean el
     campo que ya llega. En `strategy.cpp` LINE_AVOID (ATK ~:432-444 y GK ~:630-641): SI
     escape_valid -> retreat = escape_angle (rumbo geometrico real de DOWN); ELSE fallback EXACTO a
     line_angle+180 (no-regresion byte-identica). Extraer la regla como modulo puro espejo de
     fs_eval (NO portar el struct completo). Validar en el ring real antes de confiar el borde.

6. **Commit de empuje por alineacion lateral pelota-arco** — origen: Delantero 2025
   (|Yp-Ycontrincante|<=30).
   - Valor medio / riesgo medio. El nuevo ya calcula `is_aligned_to_push()` en APPROACH pero solo
     lo usa para documentar (void).
   - Como: usar el bool ya calculado para, cuando alineado+cerca, COMPROMETER el empuje (escalar la
     velocidad de avance / bloquear re-transicion a POSITION por unos ms con histeresis temporal),
     para no abortar el empuje justo al conectar. Cambio minimo, reusa logica existente; tunear en
     banco.

7. **Velocidad adaptativa al ver la pelota (boost ~50%)** — origen: Arquero legacy (pd 1.0->1.5).
   - Valor medio / riesgo medio. Ya parcialmente cubierto por kp_scale de bt_classify -> valor
     incremental medio.
   - Como: en GkState::INTERCEPT agregar un factor de velocidad CONSTANTE adicional cuando
     `world_model_ball_visible()`. Es una sola constante multiplicativa con tope por clamp; tunear
     conservador en banco para no sobre-pasar la pelota.

8. **Avance exploratorio por tiempo cuando la busqueda no encuentra pelota** — origen: Delantero
   2025 (AVANZANDO_POR_TIEMPO).
   - Valor medio / riesgo medio. SEARCH ya avanza+gira simultaneo -> valor marginal menor.
   - Como: si en banco SEARCH se queda barriendo el mismo sector, agregar un sub-timer que cada T s
     cambie el patron (mas avance, menos giro) para cubrir otra zona. Modulo puro
     `search_pattern(elapsed_ms) -> (vy, omega)`. Acotado al estado SEARCH (bajo blast radius).

9. **Control de profundidad del arquero respecto de su arco** — origen: 2025 (HC-SR04).
   - Valor medio / riesgo medio.
   - Como: modulo puro (estilo src/shared) que tome la distancia al arco propio (mejor del
     `goal_own` del WorldSnapshot v3 ya existente, o del ToF) y produzca un componente vy que
     mantenga la profundidad objetivo. Cablearlo en GK_PATROL/INTERCEPT detras de un gate con
     fallback exacto (igual patron que cross_track/ball_predict). Sumar un eje vy ponderado, NO
     reescribir la FSM. Tunear en banco.

10. **Despeje DIRECCIONAL del arquero (mandar la pelota a la banda, no al centro)** — origen:
    Arquero legacy (sugerido D3, nunca implementado; el nuevo tampoco lo hace).
    - Valor alto / riesgo alto. Toca GkState::CLEAR a 26 dias.
    - Como: modulo PURO host-testeable `clear_aim.h`: dado (bx, by, goal_own_angle) -> vector de
      empuje deseado tal que robot->pelota apunte a la banda mas lejana del centro del arco propio.
      Fallback EXACTO si goal_own NO visible -> empuje derecho actual byte-identico. Gated OFF;
      activar SOLO si hay banco para validar. Si no hay banco -> dropear.

11. **Despeje corto hacia adelante al tocar la linea si el robot ya apunta al arco** — origen:
    Delantero 2025 (CENTRANDO -> PATEANDO_corto).
    - Valor medio / riesgo alto. Toca LINE_AVOID y choca con el freno de emergencia de
      main_central (frena antes de la FSM).
    - Como: modulo puro `decide_line_action(...)`: si imminent_exit Y el frente apunta al arco
      rival dentro de tolerancia, empuje corto hacia adelante ANTES de retroceder. Gated OFF por
      default; activar solo con tiempo de banco.

### POST-INCHEON (riesgo alto / cambia conducta nuclear / requiere HW)

12. **Reposicion explicita post-despeje (volver al centro del arco)** — origen: Arquero legacy.
    Modelar como setpoint de cross_track + termino que lleve goal_own_angle->0 al re-entrar a
    PATROL. Toca la transicion CLEAR->PATROL. Mejor como mejora separada.

13. **Politica LET_CIRCULATE en el delantero (no perseguir si ya va al arco rival)** — origen: FSM
    archivada (play_decision). Cambia la conducta ofensiva nuclear; un umbral mal calibrado deja al
    delantero pasivo. Prototipar como flag OFF, espejo de pd_decide (ya escrito y testeado), NO
    meter a ciegas antes de Incheon.

14. **Estado DEFEND del delantero (pelota detras, ball_y<=-400)** — origen: FSM archivada
    (strategy_core). Comportamiento de robot unico (el delantero ayuda a defender). Concepto valido
    pero fuera de alcance ahora.

15. **8 IR en anillo como ball_angle 360** — origen: 2025 (instalados, sin usar). Cubre el angulo
    muerto trasero de la camara, pero requiere HW/cableado nuevo no presente en la arquitectura de
    3 placas. Mejora futura: el TOP fundiria ese ball_angle al WorldSnapshot.

16. **Espera por inercia al detectar pelota / degradacion conservative ante data_valid==0** —
    origen: Delantero / FSM archivada. Bajo valor; solo si en banco se observa flapping o basura de
    linea. El nuevo ya filtra bastante (line_data_fresh, heading_valid gate).

---

## 4. Que es OBSOLETO y NO se porta

Esto se dropeo a proposito o quedo superado; **no reintroducir**:

- **Kicker / solenoide / dribbler / estado PATEANDO** (full power temporizado, avanzar_patear con
  rampa a 240, retroceder_patear, secuencias adelante/pausa/atras, PATEANDO_corto del arquero): el
  robot NUEVO **no tiene pateador fisico**, empuja por inercia. Toda la logica de disparo y los
  tiempos magicos de pateo son obsoletos. El nuevo ya lo modela (APPROACH/CLEAR empujan y listo).
- **zirconLib y acceso directo a pines** (motor1/2/3, readLine/readBall/readCompass, InitializeZircon,
  analogWrite/digitalWrite INA/INB/PWM): la arquitectura nueva separa CENTRAL (decide vx/vy/omega) de
  motors_zircon (cinematica omni-3 + PWM). No portar nada de zirconLib.
- **Protocolo camara por headers 201-204 con datos 0-200** (Y=coded-100, X=0 => no detectado): el
  nuevo usa WorldSnapshot v3 (31 B) y camara->TOP v2 (11 B) con CRC8/resync. No reintroducir esquema
  sin checksum.
- **Convencion de ejes/signos vieja** (atan2(Yp,Xp), signos H/AH por rueda individual): el nuevo
  tiene su convencion documentada (+Y=frente, +X=derecha, omega CCW+, marco robot). Re-derivar, NO
  portar signos ni angulos crudos.
- **Heading PID crudo kp=0.3 sin clamp** (error*kp directo a PWM de giro): peligroso (desbordaba
  int16). El nuevo tiene HeadingPID con clamp <=327 y omega_degps_to_centideg que satura.
- **Arco rival hardcodeado a amarillo** (ARCO_CONTRINCANTE=hayarco_amarillo; Ycontrincante=Yam):
  obsoleto; el nuevo usa AttackColor + goal_own/goal_opp del v3.
- **FSM strategy_core completa del archivo** (Moore sin PID, RUSH/SEEK/DRIVE, GK por umbral unico):
  es un DOWNGRADE de la FSM viva (sin ball_predict, behind_ball, cross_track, OTOS, KICKOFF). El
  motivo del archivo sigue 100% vigente. Solo se extrae el concepto de field_safety (port #5).
- **Deteccion variante Zircon Mark1/Naveen1 por pin 32** y doble esquema de motores: el nuevo tiene
  su mapeo (MOTOR_INVERT, WHEEL_ANGLES en config_central.h). Obsoleto.
- **Thresholds LAB y rangos de linea hardcodeados del 2025** (algunos con L_min>L_max): invalidos;
  la vision se recalibra in-situ (TASK-022) y la linea la maneja DOWN (line_ring).
- **Estilo de control de tiempo monolitico** (while(1) bloqueante si no hay BNO en setup, delays por
  estado con millis fijos): el nuevo es tick 100 Hz no bloqueante con world_model fresco/stale
  gateado. No portar el estilo.
- **Bugs del legacy que NO son funcionalidades**: zona muerta 3<|Yp|<5, currentYaw RAW (solo anda al
  norte magnetico), s3 no chequeado, retroceso sin timeout, pausa de 1 s que deja el arco descubierto,
  timeout de pateo invertido, angulo de arco calculado con coords de la pelota. Son DEFECTOS.

**Lo que SI vale como leccion (no como codigo):** "ganaron con poco, sin heading ni IR" -> el robot
puede jugar **degradado** (modo de contingencia util para Incheon: hoy corre con 1 solo BNO 0x28,
el 0x29 fallo) y **la robustez por simplicidad** importa. El linaje conceptual CENTRANDO 2025 ->
POSITION/attack-line 2026 conviene documentarlo para el equipo, pero NO se porta el CENTRANDO viejo.

---

## Recordatorio final (prioridad real)

A ~26 dias de Incheon el cerebro (strategy.cpp) ya esta en verde (~566 tests) y es superior. NO
invertir riesgo en la estrategia: el **bloqueante #1 real es la vision sin recalibrar (TASK-022)**.
Los dos ports de mas valor y menos riesgo (redondez de pelota, LEDs de debug) caen justamente en
VISION. Priorizar ahi.
