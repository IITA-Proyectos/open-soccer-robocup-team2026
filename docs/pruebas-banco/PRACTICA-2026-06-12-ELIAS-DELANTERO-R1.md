# PRÁCTICA 2026-06-12 — Elías: DELANTERO con ODOMETRÍA en ROBOT1

> **Tu misión de hoy (2 horas):** que el robot 1 juegue de DELANTERO **sin
> giroscopio**: busca la pelota girando, se acerca, **la rodea** hasta dejarla
> alineada con el arco rival, y cuando está alineada **empuja con fuerza en
> línea recta** — manteniendo el rumbo con los **odómetros del piso (OTOS)**
> que este robot lleva en la placa de abajo. Los giroscopios de robot 1 están
> desconectados (quedaron afuera tras la reparación): hoy el OTOS es el rumbo.

---

## 1. La idea en 30 segundos

Robot 2 usa el giroscopio para saber hacia dónde mira. Robot 1 hoy no tiene
giroscopio — pero tiene algo que robot 2 no tiene: **dos sensores OTOS** que
miran el piso (como el sensor de un mouse) y miden cuánto se movió y cuánto
GIRÓ. Con eso el programa nuevo hace tres cosas:

1. **Eje de ataque:** al encender, el robot mira al arco rival. El OTOS
   recuerda ese rumbo ("0°"). Aunque después gire, siempre sabe hacia dónde
   queda el arco. (Si la cámara VE el arco, la cámara manda — es más precisa.)
2. **Rodear la pelota:** con el eje conocido, el robot calcula el punto
   DETRÁS de la pelota (mirando del arco) y orbita hasta ahí.
3. **Empuje recto:** al comprometer el empuje, congela la dirección y usa el
   giro medido por el OTOS para no torcerse mientras empuja a fondo 0,5 s.

## 2. Lo que necesitás antes de empezar

- **Robot 1** (el recableado). Su placa TOP se alimenta de la **batería del
  robot**, igual que en robot 2. *(Chequeo rápido: al dar batería, el panel del
  TOP tiene que aparecer. Si la TOP solo enciende con USB → algo falla en la
  alimentación: anotar y avisar a Gustavo antes de seguir.)*
- **Batería cargada > 7,8 V** — medila ANTES y anotala.
- Laptop con repo actualizado: `git pull` antes de nada.
- Cancha con línea blanca y un arco "rival" definido.
- Pelota naranja. **Sacar todo lo naranja/rojo del entorno.**
- Una caja o robot apagado como "adversario" (para el paso 8, si llegás).

## 3. Qué placa lleva qué programa

| Placa | Programa (env) | ¿Flashear hoy? |
|---|---|---|
| CENTRAL | `central_robot1_delantero_practica_bb` | **SÍ** |
| TOP | `top_robot2_pri` (sí, el "robot2": R1 quedó recableado a esa arquitectura — los envs `top_robot1*` son del cableado VIEJO, no van) | No (quedó de la demo) |
| DOWN | `down` (la versión CON odómetros OTOS) | Solo si recalibrás línea |
| Cámaras ×2 | `main.py` v2 | No (NO tocarlas) |

```
pio run -e central_robot1_delantero_practica_bb -t upload
```

## 4. Cómo se arranca, se para y se sacan los datos

Monitor serie de la CENTRAL abierto (`pio device monitor`):

- **`g` o ENTER = GO** · **`s` = STOP**. (Ojo: en este programa es `g`/`s`,
  no `s`/`x` como en el diag del árbitro de la demo.) La app del árbitro
  también funciona.
- **Al dar GO el robot pega un envión corto al frente** (~¼ s): es el
  "kickoff" de partido. Es normal. Después se pone a buscar girando.
- **Caja negra:** graba todo 50 veces/segundo; **al dar STOP escupe el CSV
  solo**. `d` = volcar a demanda · `x` = borrar.
- Guardado prolijo: `python tools\blackbox\leer_caja_negra.py COMx --espera`
  en otra terminal; al STOP te deja `corrida_*.csv`.

**Seguridad:** `s` para todo; levantar el robot también vale. El robot además
frena solo si está por pisar la línea de la cancha (protección siempre activa).

## 5. La secuencia de pruebas (en orden — cada paso depende del anterior)

> Regla del día: **un paso no está "hecho" sin su criterio cumplido y su CSV
> guardado.** Si un paso falla, no avances: anotá qué viste y consultá la
> tabla de problemas. Saber POR QUÉ no anda vale más que "ande".

### PASO 0 — Preparación (15 min)
1. Batería: `____ V` (>7,8). Con batería puesta, las 3 placas encienden
   (si la TOP no enciende → avisar, es un fallo de alimentación).
2. `git pull` + flashear la CENTRAL (comando de arriba).
3. Monitor: tiene que decir `Role: ATTACKER (FORZADO por flag de banco...)`.

### PASO 1 — Calibrar la línea EN ESTA cancha (10 min)
1. USB a la placa **DOWN**: `pio run -e diag_down_calibracion -t upload`.
2. `c` → pasear por el blanco → `b` → pasear por el verde → `v` → `s`.
3. **Criterio: 32/32 sensores con margin ≥ 40.**
4. Volver a flashear DOWN: `pio run -e down -t upload`  ← **`down` a secas**
   (es la versión con OTOS; `down_robot2` NO va en este robot).

### PASO 2 — ⭐ Verificar el OTOS (10 min) — el corazón de todo el día
Robot quieto en el piso, monitor de la CENTRAL:
1. En el panel tiene que aparecer **`otos=` con un NÚMERO** (ej. `otos=0.3`).
   Si dice `otos=N`, el dato del piso no llega: tabla de problemas, fila 1.
2. **Girá el robot a mano, despacio, MEDIA VUELTA ANTIHORARIA** (visto desde
   arriba). El número debe SUBIR hasta ~+180.
3. Devolvelo girando HORARIO: debe BAJAR y volver ~a donde estaba.
4. Deslizalo de costado sin girarlo: `otos=` casi no debe cambiar.

**Criterio: sube con antihorario, baja con horario, estable al deslizar.**
⚠️ **Si se mueve AL REVÉS (baja cuando girás antihorario): PARÁ ACÁ y avisá**
— hay que invertir un signo en el programa antes de seguir (es un cambio de
1 línea, pero con el signo al revés el empuje saldría en curva, peor que sin
corrección).

### PASO 3 — Primer movimiento, robot EN EL AIRE (10 min)
Robot sobre una caja, ruedas sin tocar el piso, SIN pelota a la vista:
1. `g` → las ruedas hacen el envión de kickoff y después el panel dice
   `ATK_SEARCH`: el robot "busca girando" (las ruedas giran para rotar).
2. Mostrale la pelota con la mano a ~50 cm de una cámara → el panel cambia
   (`ATK_POSITION` o `ATK_APPROACH`). Escondela → vuelve a `ATK_SEARCH`.
3. `s` → todo quieto.

**Criterio: los estados del panel reaccionan a la pelota.** (Acá todavía no
importa si los movimientos son lindos — está en el aire.)

### PASO 4 — Buscar y llegar a la pelota (15 min)
En el piso. Pelota quieta a ~1 m, BIEN visible. Robot apuntando para
cualquier lado. `g`:
1. Busca girando → la ve → va hacia ella (la rodea si hace falta).
2. **Criterio: llega hasta la pelota (queda a menos de una mano de ella) en
   menos de 20 segundos, sin pasarse de largo ni perderla a mitad de camino.**
3. `s` antes de que empuje si querés repetir. 3 repeticiones, CSV de cada una:
   `elias_paso4_corridaN.csv`.

### PASO 5 — La órbita: rodearla hasta alinear (20 min)
La prueba clave del posicionamiento. Armá esta foto: **arco rival – robot –
pelota** en ese orden (el robot ENTRE el arco y la pelota, pelota a ~70 cm
del robot). Así, para patear al arco, el robot está del lado EQUIVOCADO: tiene
que dar la vuelta. `g`:

1. Ve la pelota → `ATK_POSITION`: empieza a desplazarse en arco alrededor de
   ella (sin chocarla) buscando el lado opuesto al arco.
2. Cuando queda del lado correcto (pelota entre robot y arco) → `ATK_APPROACH`
   → pelota cerca y sobre el eje → **`ATK_PUSH`: empuja**.

**Criterios: (1) NO embiste la pelota directo de entrada (eso la alejaría del
arco), (2) la rodea por UN lado sin tocarla, (3) el empuje final sale hacia el
lado del arco** (no hace falta gol acá — la dirección general cuenta).
3 repeticiones (variá de qué lado ponés la pelota). CSVs `elias_paso5_*.csv`.
Si orbita para el lado largo o se marea, anotalo — se tunea con el CSV.

### PASO 6 — ⭐ El empuje RECTO medido con cinta (20 min)
El experimento estrella del día. Armá: robot → pelota a ~30 cm → arco, ya
alineados (que no necesite orbitar). Marcá con cinta de papel el punto de
partida de la pelota. `g`:

1. El robot avanza, llega a la pelota, `ATK_PUSH`: 0,5 s a fondo.
2. La pelota sale despedida. Medí con cinta métrica: a **1 m** de recorrido,
   ¿cuánto se desvió DE COSTADO respecto de la línea robot→arco?

**Criterio: desviación lateral < 15 cm a 1 m, en al menos 4 de 5 intentos.**

**5 repeticiones.** Tabla en papel: intento, desviación (cm), ¿hacia qué
lado?, y su CSV (`elias_paso6_intentoN.csv`). El análisis automático compara
el rumbo OTOS al inicio y al final de cada empuje: si el robot se torció
>10° empujando, lo detecta solo — por eso importa UN CSV POR INTENTO.

### PASO 7 — El gol completo (15 min)
Pelota al centro de la cancha, robot saliendo de su mitad, posición al azar.
`g` → buscar → alinear → empujar → ¿entró?
**3 intentos. Criterio: al menos 1 gol o 2 tiros que pegan en el arco.**
CSVs `elias_paso7_*.csv`. (Si la deriva lo desvía, es esperable sin pose
absoluta — anotá hacia dónde se desvió.)

### PASO 8 — (Solo si sobró tiempo) El freno anti-choque por ultrasonido
El sensor de ultrasonido frontal evita embestir adversarios. Es la "segunda
instancia" — solo si los pasos 4-7 quedaron bien:
1. Verificá el sensor: en el monitor del **TOP**, con la mano a ~15 cm del
   frente, el `min_obst=` del panel debe bajar a ~150.
2. Flashear la CENTRAL: `pio run -e central_robot1_delantero_practica_obst_bb -t upload`.
3. Caja/adversario entre el robot y la pelota. `g`: el robot avanza y debe
   **cortar el avance a ~25 cm del obstáculo** (puede seguir moviéndose de
   costado: solo corta el "para adelante").
**Criterio: 3 de 3 frenadas sin tocar la caja.** CSV `elias_paso8_*.csv`.

### Cierre (10 min)
Carpeta `corridas-2026-06-12-elias/` con todos los CSVs + la tabla del paso 6
+ el mini-reporte (sección 7).

## 6. Si algo falla — tabla de problemas

| Síntoma | Causa probable | Qué hacer |
|---|---|---|
| `otos=N` en el panel | DOWN sin el env `down` (con OTOS), o cable DOWN→CENTRAL | Reflashear DOWN con `down`; revisar panel de DOWN |
| `otos=` se mueve al revés | Signo del yaw | **PARAR y avisar** (paso 2) — 1 línea de fix |
| No encuentra la pelota girando | Cámaras o falsos naranjas | Sacar lo naranja; verificar `ball=` en el panel del TOP |
| Embiste la pelota sin rodearla | El eje de ataque no está | ¿`otos=N`? ¿Encendiste el robot mirando al arco rival? (el 0° se captura al encender) |
| Nunca llega a `ATK_PUSH` | Tolerancias del gatillo | Guardar CSV y avisar — son 2 perillas (`ATK_NOGYRO_PUSH_*`) |
| Empuje torcido siempre al mismo lado | Pisos de motores asimétricos o signo del yaw-hold | CSVs del paso 6 + avisar (el detector lo va a confirmar) |
| `valid=N` línea / robot no frena en el borde | Batería o calibración | Medir batería; repetir PASO 1 |
| No arranca con `g` | Monitor en placa equivocada | El `g` va en el monitor de la CENTRAL |

## 7. Lo que el coach espera recibir de vos

1. **Los CSVs nombrados por paso** (4 ×3, 5 ×3, 6 ×5, 7 ×3, 8 si llegaste).
2. **La tabla del paso 6 en papel/foto** (desviaciones medidas con cinta).
3. Respuestas cortas:
   - ¿El OTOS pasó el chequeo de signo del paso 2 a la primera?
   - En la órbita: ¿rodea por el lado corto o da vueltas de más?
   - El empuje: ¿recto, curvo siempre al mismo lado, o curvo aleatorio?
   - ¿Cuántos goles/tiros al arco en el paso 7?
   - Batería al inicio y al final.
4. Lo que te llamó la atención fuera de esta lista.

Con eso corremos `python tools\blackbox\analizar_corrida.py <archivo>` —
detecta solo los empujes torcidos, los falsos naranjas y los huecos de datos.
Podés correrlo vos para ver los gráficos de tus propias corridas.

## 8. Qué NO tocar hoy

- ❌ Constantes del código — las perillas se tocan DESPUÉS, con los datos.
- ❌ Las cámaras y su `main.py`.
- ❌ Los envs `top_robot1*` (cableado viejo) y `down_robot2` (sin OTOS) — en
  ESTE robot van `top_robot2_pri` y `down`.
- ❌ Los BNO desconectados: no reconectarlos hoy (es otra prueba, otro día).
- ❌ Nada de robot 2 — ese es de Virginia hoy.

## 9. Para entender qué hace el programa (estados del panel)

```
ATK_WAIT_START → esperando GO
ATK_KICKOFF    → envión inicial de ¼ s (usa el OTOS para salir derecho)
ATK_SEARCH     → buscando la pelota GIRANDO en el lugar
ATK_POSITION   → la vio: orbita hasta el punto DETRÁS de la pelota
ATK_APPROACH   → alineado: va derecho hacia la pelota
ATK_PUSH       → ¡empuje! 0,5 s a fondo, rumbo sostenido por OTOS
ATK_PUSH_BACK  → retrocede un toque para despegarse
(ATK_LINE_AVOID / EMERGENCY = protecciones anti-salirse de la cancha)
```

La secuencia que Gustavo pidió — *"busca girando, se acerca, gira a su
alrededor hasta alinearla con el arco, y avanza con fuerza derecho usando los
sensores del piso"* — es `SEARCH → POSITION → APPROACH → PUSH`. Hoy validás
cada flecha, y el paso 6 mide con cinta métrica la palabra "derecho".

---

## ⭐ ¿Terminaste antes de las 2 horas?

Hay una prueba OPCIONAL de 25-30 min (de banco, sin cancha): la **"cámara
pegajosa"** — un fix nuevo para que dos objetos naranjas no inventen una pelota
fantasma. El primero de los dos que termine su secuencia la puede probar:
[`PRACTICA-2026-06-12-OPCIONAL-CAMARA-PEGAJOSA.md`](PRACTICA-2026-06-12-OPCIONAL-CAMARA-PEGAJOSA.md)
