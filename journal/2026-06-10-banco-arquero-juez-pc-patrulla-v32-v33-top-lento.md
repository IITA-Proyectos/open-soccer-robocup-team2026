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

## 4. Estado al cierre

- Pendiente de banco: checklist de cierre de la patrulla (7 puntos, en la
  conversación de la sesión y resumido acá): 2 min sin entrar al área; prueba de
  sensado caído (guard); arranque pisando línea; rebote en ambos costados ≥4
  ciclos; REACQ acotado; frente ±45°; 3 ciclos STOP→GO.
- Después: fase pelota (`central_robot2_arquero`) — INTERCEPT/CLEAR.
- Fix de raíz del TOP lento: esperando la medición Δloop del panel del TOP.
- Si el "no meterse al área" es además requisito de reglamento, verificarlo
  contra `competition/rules/` (no se afirma de memoria).
