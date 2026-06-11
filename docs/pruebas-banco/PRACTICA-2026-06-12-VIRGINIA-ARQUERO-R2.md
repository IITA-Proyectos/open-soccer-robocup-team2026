# PRÁCTICA 2026-06-12 — Virginia: ARQUERO INTEGRAL en ROBOT2

> **Tu misión de hoy (2 horas):** que el robot 2 juegue de ARQUERO completo:
> patrulla moviéndose de lado a lado DELANTE del arco cubriéndolo, y cuando la
> pelota se le acerca, **sale a despejarla con fuerza y vuelve a su puesto**.
> La lógica ya está programada y la patrulla ya se validó en el banco — hoy se
> prueba la cadena completa CON pelota y se junta la evidencia (archivos CSV)
> para tunearla con datos en vez de a ojo.

---

## 1. Lo que necesitás antes de empezar

- **Robot 2** (el sano, el que fue delantero en la demo).
- **Batería cargada > 7,8 V** — medila ANTES. Batería floja = la línea se lee
  mal (`valid=0`) y los motores no rompen la inercia. Es la causa #1 de
  "el robot está roto" que después no era nada.
- Laptop con el repo actualizado: `git pull` en `soccer-main` antes de nada.
- La cancha con línea blanca visible y el arco.
- Una pelota (la naranja de siempre).
- **Sacá del entorno todo lo naranja/rojo** que no sea la pelota (botellas,
  logos, buzos): la cámara los confunde y "ve" pelotas fantasma.

## 2. Qué placa lleva qué programa

| Placa | Programa (env) | ¿Hay que flashearlo hoy? |
|---|---|---|
| CENTRAL | `central_robot2_arquero_patrol_bb` (paso 3) → después `central_robot2_arquero_bb` (paso 4) | **SÍ** (2 veces, según el paso) |
| TOP | `top_robot2_pri` | No (quedó de la demo) |
| DOWN | `down_robot2` | Solo si recalibrás línea (paso 1) |
| Cámaras ×2 | `main.py` v2 | No (NO tocarlas) |

Para flashear (desde `software/teensy/Soccer 2026/`, con el USB en la placa correcta):

```
pio run -e central_robot2_arquero_patrol_bb -t upload
```

## 3. Cómo se arranca y se para (y cómo se sacan los datos)

Con el monitor serie de la CENTRAL abierto (`pio device monitor`):

- **`g` o ENTER = GO** (como si el juez diera START). El arquero espera **2
  segundos** antes de moverse — es a propósito, para que te dé tiempo a
  acomodarlo y soltarlo.
- **`s` = STOP.** El robot para y la FSM vuelve a esperar. Con `g` arranca
  TODO de nuevo desde el principio.
- También funciona la **app del árbitro** (GO/STOP reales por radio), igual
  que en la demo.
- **Caja negra:** el robot graba TODO lo que ve y decide, 50 veces por
  segundo. **Al dar STOP el CSV se vuelca solo** por el monitor. También:
  `d` = volcar ya · `x` = borrar y grabar de cero.
- Para guardar el CSV prolijo: en otra terminal,
  `python tools\blackbox\leer_caja_negra.py COMx --espera`
  (queda esperando; cuando des STOP, guarda `corrida_*.csv` solo).

**Seguridad:** si el robot hace algo raro, `s` y listo. Levantarlo del piso
también vale (la placa de abajo detecta "levantado" y deja de confiar en la
línea). No es partido: intervenir está PERMITIDO.

## 4. La secuencia de pruebas (en este orden, sin saltear)

> La regla del día: **un paso no se da por bueno sin su criterio cumplido y su
> CSV guardado.** Si un paso falla, no avances: anotá QUÉ pasó (estado del
> panel, qué hacía el robot) y probá la fila de la tabla de problemas. El
> objetivo NO es "que ande": es saber POR QUÉ anda o no anda.

### PASO 0 — Preparación (15 min)
1. Batería medida y anotada: `____ V` (>7,8).
2. `git pull` en la laptop.
3. Flashear CENTRAL: `pio run -e central_robot2_arquero_patrol_bb -t upload`.
4. Abrir el monitor: tiene que decir `Role: GOALKEEPER (FORZADO...)`.

### PASO 1 — Calibrar la línea EN ESTA cancha (15 min)
La calibración vieja puede no valer (otra luz, otro piso). Siempre se calibra
en el lugar:
1. USB a la placa **DOWN**: `pio run -e diag_down_calibracion -t upload`.
2. En el monitor de DOWN: `c` (empezar) → pasear el robot por el **blanco** de
   la línea → `b` → pasearlo por el **verde** → `v` → `s` (guardar).
3. **Criterio: `32/32` sensores con `margin ≥ 40`.** Si no llega: batería,
   suciedad en los sensores, o luz directa rara.
4. Volver a flashear DOWN: `pio run -e down_robot2 -t upload`.

### PASO 2 — Chequeo del panel (10 min) — el robot quieto en la mano
En el monitor de la CENTRAL, cada medio segundo sale una línea. Verificá:
- `snap_fresh=Y` y `top[fr=...]` **subiendo** → el TOP habla.
- `hdg=` cambia cuando girás el robot en la mano y vuelve ~al valor original
  al devolverlo → el giroscopio vive.
- `line_fresh=Y` y `valid=Y` con el robot sobre el piso de la cancha.
- `match=` pasa de STOP a RUN con `g` y vuelve con `s`.
- (`otos=N` es **normal** en robot 2: este robot no lleva odómetros de piso.)

**Criterio: las 4 primeras OK.** Si alguna falla, a la tabla de problemas.

### PASO 3 — Patrulla SIN pelota (20 min) — esto ya se validó; es la base
Pelota GUARDADA (que no se vea). Robot en el centro de la cancha mirando al
arco rival. `g` y observar la secuencia completa:

1. Espera 2 s → **retrocede derecho** hacia su arco.
2. Toca la línea del área → **avanza un toque** (~3 cm) para despegarse.
3. **Patrulla**: tramos laterales de ~1 s, parada breve, a veces un pulso
   cortito de giro para enderezarse, y **rebota al tocar la línea lateral**.

**Criterios de aceptación (2 minutos de patrulla continua):**
- ✅ Nunca se mete adentro del área/arco.
- ✅ Nunca queda girando sobre sí mismo ni "se va de viaje" lejos del arco.
- ✅ Rebota al tocar la línea de los costados (se ve clarito).

Hacer **3 corridas**. Después de cada STOP, guardar el CSV y renombrarlo:
`virginia_paso3_corrida1.csv`, `..._corrida2.csv`, etc.

### PASO 4 — Arquero COMPLETO con pelota (30 min) — lo nuevo de hoy
Flashear: `pio run -e central_robot2_arquero_bb -t upload`.

**4a. Pelota quieta, lateral (10 min):** robot patrullando; apoyá la pelota
quieta a ~1 m, corrida hacia un costado. El arquero debe **desplazarse hacia
ese costado** (la sigue en X) SIN abandonar su línea ni salir corriendo hacia
ella. Movela de costado a costado: la sigue.
- **Criterio: se alinea con la pelota en <2 s y no avanza hacia ella.**

**4b. El despeje (20 min):** rodá la pelota suave hacia el arco (a la altura
del robot o un poco al costado). Cuando la pelota entra a **~25 cm**, el
arquero debe **salir con fuerza hacia ella, empujarla lejos, y al quedar la
pelota lejos (>40 cm) volver a su patrulla** (la línea de atrás lo guía de
vuelta).
- **Criterios: (1) sale SOLO cuando la pelota está cerca (no antes), (2) el
  contacto es franco (la empuja, no la acaricia), (3) después del despeje
  vuelve a patrullar SIN quedarse perdido en el medio de la cancha.**
- **5 tiros**, variando el costado. Cada uno con su CSV:
  `virginia_paso4_tiro1.csv` … `tiro5.csv`.
- Si con pelota a la vista el robot "se vuelve loco" (persigue fantasmas,
  oscila), NO es fracaso: volvé a `_patrol_bb`, guardá el CSV del desastre
  (ese CSV es ORO para el diagnóstico) y anotalo.

### PASO 5 — Cierre (10 min)
- Juntar TODOS los CSVs en una carpeta `corridas-2026-06-12-virginia/`.
- Completar el mini-reporte (sección 6).
- Si hubo video (aunque sea celular apoyado): mejor todavía.

## 5. Si algo falla — tabla de problemas

| Síntoma | Causa probable | Qué hacer |
|---|---|---|
| `valid=N` o `CALIB?` en línea | Batería floja o calibración vieja | Medir batería; repetir PASO 1 |
| Persigue "pelotas" que no existen | Objeto naranja/rojo en el entorno | Sacar TODO lo naranja (¡y manos cerca de la cámara!) |
| `hdg=` clavado al girarlo a mano | El BNO se congeló | Apagar y prender TODO el robot (power-cycle completo) |
| `top[fr]` no sube | Cable TOP→CENTRAL o TOP sin alimentar | Revisar USB/cables del TOP |
| No arranca con `g` | Monitor en la placa equivocada | El `g` va en el monitor de la CENTRAL |
| Se mete al área en patrulla | — | STOP, guardar CSV, anotar — eso es hallazgo, no error tuyo |
| Gira violento en las paradas | — | Ídem: CSV + nota (hay perillas de pulso para tunear) |

## 6. Lo que el coach espera recibir de vos

1. La carpeta con los **CSVs nombrados** (paso 3 ×3, paso 4 ×5+).
2. Estas respuestas (cortas, lo que VISTE):
   - ¿El rebote de patrulla ocurre EN la línea lateral o antes/después?
   - ¿Cuántos pulsos de giro hace por parada, más o menos? ¿Se ve tranquilo o nervioso?
   - En el despeje: ¿salió cuando la pelota estaba cerca o salió antes de tiempo?
   - ¿Volvió a la patrulla después de despejar? ¿Cuánto tardó?
   - Batería al inicio y al final.
3. Lo que te llamó la atención y no estaba en esta lista. **Eso suele ser lo
   más valioso.**

Con los CSVs corremos `python tools\blackbox\analizar_corrida.py <archivo>` y
salen los diagnósticos automáticos (estados, falsos naranjas, flapping,
frenos de emergencia). Podés correrlo vos misma si querés ver los gráficos.

## 7. Qué NO tocar hoy

- ❌ Constantes del código (umbrales, velocidades) — se tunean DESPUÉS, con los CSVs.
- ❌ Las cámaras y su `main.py`.
- ❌ Los envs `top_robot1*` (cableado viejo; este robot usa `top_robot2_pri`).
- ❌ Nada de robot 1 — ese es de Elías hoy.

## 8. Para entender qué hace el programa (los estados que ves en el panel)

```
GK_WAIT_START  → esperando GO (2 s de cortesía después del GO)
GK_GOTO_LINE   → retrocediendo derecho hasta tocar la línea del área
GK_ADVANCE     → avanzando un toque para despegarse de la línea
GK_PATROL_*    → patrullando (MOVE=tramo, STOP=pausa, PULSE=girito de
                 corrección, SETTLE=asentando, REACQ=buscando la línea atrás)
GK_INTERCEPT   → vio la pelota: se alinea con ella moviéndose de costado
GK_CLEAR       → ¡pelota cerca! sale a despejarla con fuerza
(GK_LINE_AVOID / EMERGENCY = protecciones anti-salirse de la cancha)
```

El ciclo que Gustavo pidió — *"cubre el arco, sale a despejar cuando la ve
cerca, y vuelve"* — es exactamente `PATROL → INTERCEPT → CLEAR → PATROL`.
Tu trabajo de hoy es verificar cada flecha de ese ciclo con tus propios ojos
y dejar la evidencia grabada.

---

## ⭐ ¿Terminaste antes de las 2 horas?

Hay una prueba OPCIONAL de 25-30 min (de banco, sin cancha): la **"cámara
pegajosa"** — un fix nuevo para que dos objetos naranjas no inventen una pelota
fantasma. El primero de los dos que termine su secuencia la puede probar:
[`PRACTICA-2026-06-12-OPCIONAL-CAMARA-PEGAJOSA.md`](PRACTICA-2026-06-12-OPCIONAL-CAMARA-PEGAJOSA.md)
