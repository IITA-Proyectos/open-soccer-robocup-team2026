# 2026-06-10 — Banco arquero robot2: juez desde la PC, patrulla v3.2/v3.3 pegada a la línea, y el hallazgo del TOP lento

> Sesión de banco con Gustavo (continuación del 2026-06-09). Robot2, env
> `central_robot2_arquero_patrol`. Iteración en vivo: log del monitor → fix →
> re-flash → re-test. Commits: `44b129b` (juez PC) y `c11d770` (patrulla v3.2+v3.3).

## 1. Juez desde la PC (`44b129b`)

La app del juez falló temprano (después volvió). Quedó implementado el respaldo
permanente: con `-DCENTRAL_ENABLE_MANUAL_START` (solo envs de banco del arquero),
el monitor serie de la CENTRAL ES el juez: **ENTER o `g` = GO, `s` = STOP**. STOP
devuelve la FSM a WAIT_START → un `g` nuevo re-corre la secuencia completa (delay
de 2 s incluido) sin tocar la batería. En competencia el flag NO se define (el
GO/STOP real llega de la app del juez por GPIO 5/6 del TOP — arrancar sin árbitro
desclasifica). Doc: `docs/pruebas-banco/ARQUERO-EN-ROBOT2-PLAN.md` § "Juez desde la PC".

Convivencia app+serie (env de banco flasheado): si se tipeó `g`, el STOP de la
app NO para el robot hasta mandar `s` (el override serie gana). Regla: si se usa
la app, no tocar `g`.

## 2. HALLAZGO ESTRUCTURAL: el snapshot del TOP llega a ~4 Hz (no 100 Hz)

Con el panel nuevo (sub-fases de patrulla) el log mostró `top[fr=...]` subiendo
**+2 por línea de panel** → ~4 WorldSnapshots/segundo llegando a la CENTRAL,
cuando `main_top.cpp` los manda cada 10 ms (100 Hz) por diseño. El enlace está
sano (230400 baudios, crc=0, resync=0): **el loop del TOP se arrastra ~25× más
lento de lo diseñado**.

Consecuencia directa medida en banco: todo lazo de rumbo de la CENTRAL trabaja
con heading de hasta 250-500 ms de atraso →

- Los pulsos de re-orientación (50-120 ms a ~300-600°/s reales con pisos)
  barrían 70-100° antes de que llegara UN dato fresco: el "corte en vivo" de la
  v3.1 estaba ciego → ping-pong violento (visto en log: SETTLE −41° → +28° →
  PULSE → −50°).
- Retroactivamente explica el J/U del retroceso del 2026-06-09 y gran parte de
  la inestabilidad de heading de toda la campaña de banco (se suma al
  OMEGA_SIGN ya corregido).

Sospechas para el fix de raíz (PENDIENTE, no bloquea la patrulla):
1. `getRangingData()` de los 4 VL53L7CX trae un bloque grande por `Wire` a
   100 kHz — posiblemente decenas de ms por sensor, varios por pasada.
2. En **robot2** el BNO secundario vive en `Wire` CON los 4 ToF y el
   anti-choque `TOP_BNO_TOF_DECONFLICT` está gateado SOLO a `top_robot1`
   (cuando se creó no se sabía que robot2 tenía el 2º BNO en `Wire`).

**Medición pendiente (equipo)**: USB a la placa TOP, ~6 líneas de su panel
(`[TOP] loop=N ...` cada 500 ms) → el Δloop entre líneas da la velocidad real
del loop y discrimina (1) vs (2). Fixes candidatos ya pensados: poll de ToF
round-robin (uno por pasada) + extender el deconflict a robot2. Esto destraba
el frente fino Y el intercept con pelota.

## 3. Patrulla v3.2 → v3.3: pegada a la línea (`c11d770`)

Iteración del día, guiada por pedido explícito de Gustavo: *"debería moverse en
base a la línea blanca que limita el arco"*.

- **v3.2 — rebote por línea**: la patrulla mantiene el sentido tramo tras tramo
  hasta tocar la línea LATERAL que ve DOWN (dato físico, rápido, no pasa por el
  TOP) y ahí invierte. Pose del TOP → límite secundario; tope de 3 tramos de
  fail-safe. El router "tocar línea → avanzar" ahora exige línea ATRÁS (al
  costado la resuelve el rebote).
- **Problema detectado en banco**: "ya no hace los giros pero sigue sin guiarse
  de la línea" → geometría: el avance post-línea (350 ms ≈ 10 cm) dejaba la
  línea FUERA del anillo para siempre → el rebote no tenía con qué disparar.
- **v3.3 — pegada a la línea**: margen de avance 350→100 ms (línea al borde
  trasero del anillo) + sub-fase **RE-ENGANCHE** (`GK_PATROL_REACQ`): si en una
  parada no ve la línea, retrocede suave (200 mm/s, tope 700 ms) hasta re-verla.
  Efecto emergente: si algo lo saca del puesto, vuelve solo a la profundidad del arco.
- **Guard anti-"caminar al arco"**: 2 re-enganches seguidos sin hallar línea
  (sensado caído: batería floja/calibración → `valid=0`, lección 2026-06-06) →
  deja de retroceder hasta volver a ver línea. El requisito duro del banco es
  que NUNCA se meta al área/arco.
- **Pulsos domesticados** para el heading a 4 Hz: umbral 20→35°, pulsos 40-80 ms,
  máx 2 por parada, settle 700 ms (≥2 muestras frescas a 4 Hz). Resultado de
  banco: **giros violentos eliminados** (confirmado por Gustavo).

## 4. RESUELTO el mismo día: el TOP lento (sesión tarde, Gustavo en banco sin cancha)

El §2 quedó resuelto el mismo día, con medición antes/después por el contador
`loop=` del panel `[TOP]` (Δloop entre líneas de 500 ms):

| | loop del TOP | snapshot a CENTRAL |
|---|---|---|
| Antes | **~6 Hz** (Δ=+3/línea) | ~4 Hz |
| Round-robin ToF (`a6c0366`) | ~16 Hz (Δ=+8) | ~16 Hz |
| + payload recortado | **~190.000/s** (Δ≈+95k) | **100 Hz (diseño)** |

- **Causa raíz confirmada:** los 4 `getRangingData()` del VL53L7CX por pasada,
  cada uno trayendo el bloque COMPLETO de resultados por `Wire`@100 kHz (~60 ms
  por sensor — medido: con round-robin de a 1, el loop quedó en ~60 ms/pasada).
- **Fix 1 — round-robin** (`a6c0366`): UN ToF por tick de 30 ms; cada sensor se
  refresca cada ~120 ms (ranging interno 15 Hz; P1-TOF-STALE 250 ms, margen 2×).
- **Fix 2 — payload**: `-DVL53L7CX_DISABLE_{AMBIENT_PER_SPAD, NB_SPADS_ENABLED,
  SIGNAL_PER_SPAD, RANGE_SIGMA_MM, REFLECTANCE_PERCENT, MOTION_INDICATOR}` en
  `top_robot1`/`top_robot2` (solo usamos `distance_mm` + `target_status`).
  Los diags NO llevan los flags (siguen viendo el struct completo).
- **Validación de banco (Gustavo):** hdg trackea el giro a mano (0→+18.6→−23.3→+15.7,
  sin congelarse), ToF dinámicos (95→161 mm moviendo la mano), `resync=0`,
  cámaras F/B vivas (hasta detectó pelota). TASK-014 baja P0→P2 (resta medir
  CENTRAL/DOWN y formalizar presupuesto I²C del BNO).
- **Implicancia para la patrulla:** los pulsos quedaron tuneados para heading a
  4 Hz (umbral 35°, settle 700 ms). Con 100 Hz reales se pueden RE-APRETAR en un
  próximo banco con cancha (umbral 35→20, settle 700→400, máx pulsos 2→3) — el
  corte en vivo ahora sí ve el giro en tiempo real. NO cambiar sin banco.
- ⚠️ ROBOT1 hereda ambos fixes (mismo código/env) — A VERIFICAR a su regreso.

## 5. Estado al cierre

- Pendiente de banco: checklist de cierre de la patrulla (7 puntos, en la
  conversación de la sesión y resumido acá): 2 min sin entrar al área; prueba de
  sensado caído (guard); arranque pisando línea; rebote en ambos costados ≥4
  ciclos; REACQ acotado; frente ±45°; 3 ciclos STOP→GO.
- Después: fase pelota (`central_robot2_arquero`) — INTERCEPT/CLEAR.
- Fix de raíz del TOP lento: ✅ RESUELTO el mismo día (ver §4). Queda para el próximo
  banco: verificar `top[fr]` subiendo ~+50 por línea en el panel de la CENTRAL, y que
  ROBOT1 herede los fixes (A VERIFICAR) al volver de reparación.
- Si el "no meterse al área" es además requisito de reglamento, verificarlo
  contra `competition/rules/` (no se afirma de memoria).
