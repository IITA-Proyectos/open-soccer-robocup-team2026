# Revisión arquero STRAFE — 2026-06-15, 9:00 (con Virginia + Elías)

> **Qué es esto:** registro de lo que probamos en el banco el **2026-06-14** con el arquero
> en modo strafe (robot 2) y la **lista de pendientes** para resolver mañana. Léanlo antes de
> arrancar; al final está cómo reproducirlo y cómo sacar los datos.

---

## 1. Qué cambiamos (firmware, 2026-06-14)

Se combinó en un solo programa lo que ya teníamos en envs separados (commit `23f9fc6`):
- **Retroceso al arco + avance** (lo que hacía `_patrol_bb`): retrocede recto hasta tocar su
  línea de fondo y avanza ~10 cm para despegarse.
- **Strafe lateral** (lo que hacía `_strafe_bb`): barre de lado a lado, rebota en los laterales,
  y mantiene el frente al arco con control de rumbo PI+PFM (giroscopio del BNO).

**Programa a flashear (solo la CENTRAL):** `central_robot2_arquero_strafe_bb`.
**TOP:** `top_robot2_pri`. **DOWN:** `down_robot2` (ya calibrado).

## 2. Qué pasó en el banco (2026-06-14)

| # | Qué probamos | Resultado |
|---|---|---|
| 1 | 1ª corrida del strafe | La secuencia FSM anduvo (`GOTO_BACK → ADVANCE → MOVE ↔ ESCAPE`), **pero salió en DIAGONAL, sin control de giroscopio**. En el log: `hdg=0.0` SIEMPRE. |
| 2 | Diagnóstico del heading | El `hdg=0.0` venía de la **TOP**: su BNO no estaba dando rumbo (heading muerto). R2 **no tiene OTOS**, así que el único rumbo posible es el BNO de la TOP → sin eso, el strafe es a ciegas (diagonal). |
| 3 | Re-flasheamos la TOP (`pio run -e top_robot2_pri -t upload`) | **El heading ARRANCÓ** — ahora `hdg` cambia al girar el robot (era un flasheo viejo de la demo). |
| 4 | 2ª corrida (heading andando) | **Anduvo un poco mejor, pero a veces queda PARADO y hay que empujarlo** para que vuelva a moverse. El heading **NO se mantiene**: oscila ~±37°. |

**Log de la 2ª corrida (evidencia):** el rumbo saltaba
`hdg=-0.5 → -0.9 → -4.2 → -37.4 → -16.5 → +17.1` en ~4 s, con muchos `GK_SIMPLE_ESCAPE` seguidos.

## 3. Diagnóstico (por qué queda parado)

La raíz es que **el control de rumbo (PI+PFM) no está sosteniendo el frente** — oscila ±37°.
Eso encadena tres cosas:
1. **Diagonal:** si el robot está rotado, el strafe (lateral en el marco del robot) sale torcido
   respecto a la cancha.
2. **Rebotes erráticos:** el rebote ignora la línea "de atrás" (±135°), pero **si el robot está
   rotado, su propia línea de arco le aparece en otro ángulo → la confunde con un lateral y rebota
   contra su propio arco** (por eso tantos `ESCAPE` seguidos). Cada rebote invierte el strafe y
   patea más la rotación → lazo vicioso.
3. **El "parado":** cuando el error de rumbo pasa 45°, entra a `RESQUARE` (deja de barrer y trata
   de girar en el lugar). Si el giro queda flojo, parece clavado; **al empujarlo le corregís el
   rumbo → sale de RESQUARE → vuelve a moverse.** Eso es exactamente lo que viste.

⚠️ Esto es el problema **conocido-difícil** del acople strafe↔giro que el propio código documenta:
con los pisos `{70,70,107}`, en el strafe las ruedas delanteras van chicas y cualquier giro las
desborda; por eso María usó PFM (giro por pulsos) en vez de PID continuo. **El PFM se tuneó el
06-12 pero nunca había corrido hasta ayer** — recién ahora se ve cómo se comporta.

## 4. PENDIENTES para mañana (agenda 9:00)

- [ ] **Batería primero:** medir **>7,8 V** antes de cualquier conclusión. Batería floja empeora las
      dos cosas (motores no rompen inercia = más "parado"; y se mueve raro = más deriva de rumbo).
- [ ] **Sacar el CSV de la caja negra** de una corrida con 2-3 stalls (tiene, a 50 Hz: estado +
      `hdg` + **PWM real por motor** + comando). Es el dato que falta para no adivinar.
      → `python tools\blackbox\leer_caja_negra.py COM17 --espera` (da STOP `s` y guarda `corrida_*.csv`).
- [ ] **Con el CSV, decidir el ajuste (con datos, no a ojo):**
      - ¿El rumbo oscila por gains del PFM? → titrar deadband (5→8°) / ki (0.4→0.8) / kp (2→3).
      - ¿Se traba en RESQUARE? → revisar el umbral 45° y la autoridad de giro parado.
      - ¿Se traba por PWM del strafe bajo el piso? → subir `GK_PATROL_SPEED_MM_S` (200→~280) o el piso.
- [ ] **Idea de diseño (a discutir):** hacer el **rebote robusto al rumbo** — descontar el heading
      del robot al clasificar la línea, para que NUNCA confunda su propia línea de arco (la de atrás)
      con un lateral aunque esté rotado. Eso cortaría el lazo vicioso de rebotes.

## 5. Cómo reproducir

```powershell
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
pio run -e top_robot2_pri -t upload                  # TOP (heading) — si hdg vuelve a 0, re-flashear
pio run -e central_robot2_arquero_strafe_bb -t upload # CENTRAL (el strafe)
pio device monitor -b 115200                          # ver estado + hdg
# GO = 'g' o ENTER · STOP = 's' (vuelca el CSV). Confirmar hdg cambia al girar el robot a mano.
```

## 6. Lo que NO es el problema (ya descartado)
- La CENTRAL y el cambio del strafe están bien (la secuencia retroceso→avance→strafe funciona).
- El botón ya está deshabilitado por fail-safe (GO/STOP por teclado `g`/`s` o árbitro).
- El heading de la TOP **ahora anda** (tras re-flashear) — el bloqueante era ese, ya resuelto.
