# PRÁCTICA 2026-06-12 — OPCIONAL: la "cámara pegajosa" (¿terminaste tu secuencia? esto es para vos)

> **Para quién:** Virginia O Elías, el que termine primero su secuencia
> principal. Sirve en CUALQUIERA de los dos robots (las dos placas TOP corren el
> mismo programa). **Duración: ~25-30 min.** Es una prueba de BANCO (robot quieto
> en la mano/mesa): no necesitás la cancha.

## 1. El bug que vas a ver con tus propios ojos

El robot tiene una cámara adelante y una atrás, mirando a lados opuestos. Una
misma pelota **nunca** puede estar en las dos a la vez. Pero el programa actual,
cuando AMBAS cámaras dicen "veo pelota" (o sea: al menos una está viendo un
naranja falso), **promedia las dos posiciones** → inventa una pelota fantasma en
el punto medio. Ejemplo: pelota real adelante a 40 cm + botella naranja atrás a
60 cm → el robot "ve" la pelota a 10 cm DETRÁS suyo y se da vuelta.

La versión nueva ("cámara pegajosa", idea de Gustavo) hace lo que haría una
persona: **se acuerda de qué cámara venía viendo la pelota y le cree a esa**; la
otra queda como sospechosa. Y baja la confianza reportada a 60 para avisar que
hay un naranja espurio en escena (en el panel del TOP ahora se ve:
`ball=(x,y)c60`).

## 2. Lo que necesitás

- Tu robot (cualquiera de los dos), batería OK, USB a la placa **TOP**.
- La pelota naranja + UN objeto naranja extra (botella, cono, cinta).
- El monitor del TOP abierto: `pio device monitor` (la línea `[TOP] ... ball=`).

## 3. La prueba (con el ANTES y el DESPUÉS — guardá las dos evidencias)

### Parte A — Documentar el bug con el firmware ACTUAL (5 min)
1. Robot quieto en la mesa con el firmware de hoy (`top_robot2_pri`).
2. Pelota real ADELANTE a ~50 cm (visible para la cámara frontal). Mirá el
   panel: `ball=(algo, +400 aprox)c80` — la `y` POSITIVA dice "adelante". ✍️ Anotá.
3. SIN sacar la pelota, poné el objeto naranja ATRÁS a ~50 cm. Mirá el panel:
   - **El bug:** la posición salta a un punto que no es ni la pelota ni el
     objeto (la `y` se achica o se hace negativa = "pelota fantasma en el
     medio/atrás") y la confianza SUBE a `c95` (¡el programa viejo premiaba el
     caso imposible!). ✍️ Anotá la posición fantasma que viste.

### Parte B — Flashear la pegajosa (5 min)
```
pio run -e top_robot2_pri_sticky -t upload
```
(USB en la placa TOP. La CENTRAL y la DOWN no se tocan.)

### Parte C — Verificar la conducta nueva (10 min)
Repetí el escenario de la parte A (pelota adelante primero, después aparece el
naranja atrás):
1. **Criterio 1:** `ball=` queda CLAVADA en la pelota real de adelante
   (`y` positiva, misma posición que antes) aunque el naranja esté atrás.
2. **Criterio 2:** la confianza muestra `c60` (= "veo conflicto, me quedo con la
   titular") mientras ambos naranjas están a la vista.
3. **Traspaso legítimo:** sacá el objeto falso, y llevá la pelota real rodando
   por el costado hasta ATRÁS del robot. Cuando la cámara frontal la pierde, la
   trasera la toma AL INSTANTE (`y` pasa a negativa, `c80`). Sin pausas raras.
4. **Memoria:** con la pelota atrás (titular = trasera), volvé a poner el
   objeto falso ADELANTE: el panel debe SEGUIR mostrando la pelota de atrás
   (`y` negativa, `c60`).
5. **Arranque en frío con conflicto:** tapá TODO 2 segundos (que diga
   `ball=--`), después destapá ambos a la vez: debe elegir **el más cercano**
   al robot. Probalo con la pelota más cerca que el objeto.

### Parte D — ¿Y el robot jugando? (5 min, opcional del opcional)
Si tu secuencia principal quedó armada, repetí UNA corrida corta de tu programa
(la patrulla de Virginia o el buscar-y-empujar de Elías) con la pegajosa puesta
y el objeto naranja a la vista. El robot debería ignorarlo mientras tenga la
pelota real. Guardá el CSV de la caja negra como siempre
(`opcional_pegajosa_corrida1.csv`).

## 4. Qué anotar para el coach

1. La posición fantasma de la parte A (el ANTES) y qué mostró la parte C en el
   mismo escenario (el DESPUÉS).
2. ¿Los 5 criterios de la parte C pasaron? ¿Cuál no?
3. Si hiciste la parte D: el CSV + qué hizo el robot.
4. Veredicto tuyo: ¿la dejarías puesta para los partidos? ¿por qué?

## 5. Al terminar

- **Si TODOS los criterios pasaron:** podés dejar `top_robot2_pri_sticky`
  puesta el resto de la práctica (es estrictamente mejor que el promedio) y
  anotarlo en tu reporte.
- **Si algo salió raro:** volvé al firmware de siempre —
  `pio run -e top_robot2_pri -t upload` — y anotá QUÉ viste. Ese dato vale
  tanto como el éxito.

## 6. Qué NO es esta mejora (límite conocido, para la entrevista de jueces 😉)

Si el naranja falso está **del mismo lado** que la pelota y es MÁS GRANDE, la
cámara de ese lado va a elegir el falso igual — eso se decide adentro de la
cámara (elige el blob más grande) y se arregla recién cuando el mensaje
cámara→TOP transporte el tamaño del blob (mejora futura, anotada). La pegajosa
arregla el conflicto ENTRE cámaras, que es el caso que inventaba pelotas
fantasma de la nada.
