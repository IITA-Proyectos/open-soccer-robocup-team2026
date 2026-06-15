---
title: "strategy.cpp explicado — el cerebro de la CENTRAL en palabras simples"
date: 2026-06-14
author: "Claude (Anthropic - Claude Opus 4.8 1M) — coach"
requested-by: "Virginia Viollaz (@gviollaz)"
status: vivo
tipo: guia-de-lectura
scope: software/teensy/Soccer 2026/src/central/strategy.cpp
---

# `strategy.cpp` explicado — el cerebro de la CENTRAL

> **Para qué es este documento.** `strategy.cpp` es el archivo más largo y más
> enredado del robot (1759 líneas). Esta guía NO reemplaza al código ni al
> documento detallado ([`FIRMWARE-PLACA-CENTRAL.md` §8](FIRMWARE-PLACA-CENTRAL.md)).
> Es un **mapa mental**: lo que conviene tener en la cabeza ANTES de abrir el
> archivo, para que las 1759 líneas dejen de ser una pared y pasen a ser unos
> 15 mini-programas chiquitos que ya sabés dónde buscar.
>
> Los nombres en MAYÚSCULA tipo `SEARCH` o `PATROL` son **el nombre exacto que
> tiene esa parte en el código** (no se pueden traducir adentro del programa).
> Al lado de cada uno te ponemos qué significa en castellano.

---

## 1. La idea en una sola frase

**`strategy.cpp` mira lo que pasa, elige UNA conducta, y devuelve UNA orden de
movimiento. Nada más. 100 veces por segundo.**

Si te llevás solo eso, ya entendiste el 80 %. Todo lo demás es detalle de
*cuál* conducta y *qué* orden.

---

## 2. Cómo funciona: entra → decide → sale

El cerebro es una función, `strategy_tick()`, que el programa principal llama
cada 10 milésimas de segundo (100 veces por segundo). Cada vez que la llaman,
hace tres pasos:

```
   ┌──────────────────┐      ┌────────────────────┐      ┌────────────────────┐
   │  ENTRA            │      │  DECIDE            │      │  SALE              │
   │  lo que el robot  │ ───► │  elige UNA         │ ───► │  una orden:        │
   │  cree que pasa    │      │  conducta y qué    │      │  ir al costado,    │
   │  (pelota, arcos,  │      │  hacer en ella     │      │  ir de frente,     │
   │   línea, rumbo)   │      │                    │      │  y girar           │
   └──────────────────┘      └────────────────────┘      └────────────────────┘
```

- **Entra** — Lo que el robot "cree que está pasando" se guarda en algo llamado
  `world_model` (el *modelo del mundo*). Es el espejo de lo que ven las otras
  dos placas: la cámara y los arcos los arma la placa de ARRIBA; la línea blanca
  del piso y el movimiento medido los arma la de ABAJO. **`strategy.cpp` no le
  habla a ningún sensor**: solo le hace preguntas a ese modelo, como
  "¿se ve la pelota?" o "¿a qué ángulo está la línea?".
- **Decide** — La máquina de conductas (la explicamos en el punto 4). Acá vive
  toda la táctica.
- **Sale** — Una orden con tres números: cuánto ir **al costado** (izquierda o
  derecha), cuánto ir **de frente** (adelante o atrás), y cuánto **girar**.
  **Cómo se reparten esos tres números a las 3 ruedas NO es asunto de este
  archivo** — eso lo hace otra parte (la que calcula las ruedas). Por eso el
  cerebro piensa en "quiero ir para allá y girar así", no en "rueda 1 a tanta
  fuerza".

> **Palabra difícil: 100 veces por segundo (en inglés, "100 Hz").** Significa
> que esta decisión se rehace 100 veces cada segundo. El robot no "piensa una
> jugada y la ejecuta": vuelve a decidir todo, todo el tiempo, casi al instante.

---

## 3. Un archivo, dos cerebros: el que ataca y el que ataja

El mismo `strategy.cpp` corre en los dos robots. **El robot decide qué cerebro
usar cuando se arma el programa** (al compilar), no en la cancha:

- `ROBOT1` → **arquero** (el que ataja) — función `goalkeeper_tick()`
- `ROBOT2` → **delantero** (el que ataca) — función `attacker_tick()`

`strategy_tick()` es solo un portero que, según el papel, llama a una de las dos
funciones:

```cpp
MotorCommand strategy_tick() {
    return (es delantero) ? attacker_tick() : goalkeeper_tick();
}
```

Así que en realidad hay **dos máquinas de conductas distintas** metidas en el
mismo archivo. Cuando leas, elegí una y olvidate de la otra.

---

## 4. ¿Qué es una "máquina de estados"? (la idea más importante)

**Comparación: las conductas de un jugador.** Pensá en vos jugando al fútbol.
En cada momento estás haciendo UNA sola cosa: o buscás la pelota, o corrés
hacia ella, o la empujás al arco. No hacés dos a la vez. Y tenés reglas claras
de cuándo cambiar: "si veo la pelota, dejo de buscar y voy hacia ella".

Eso es una **máquina de estados** (una forma de ordenar el programa por
"conductas"):

- **Estado** = la conducta en la que estás ahora (una sola a la vez).
- **Cambio** = la regla que te lleva de una conducta a otra ("si pasa esto,
  cambiá a aquello").
- Cada conducta hace **una** cosa concreta y mira lo que pasa para decidir si
  tiene que saltar a otra.

**Dónde la comparación se rompe** (y por qué importa): un jugador de verdad
mezcla las conductas con naturalidad —frena mientras gira mientras mira—. El
robot **salta de golpe** de una conducta a la siguiente. Ese salto brusco es la
causa de la mitad de los problemas que vas a ver comentados en el código (el
robot que tiembla entre dos conductas, los giros que se pasan de largo). Mucho
del código difícil es justamente **suavizar esos saltos**.

En el código, cada máquina es un `switch` grande: una rama (`case`) por
conducta. Leé cada rama como si fuera una mini-función aparte.

---

## 5. El delantero — el que ataca (`attacker_tick`)

**Misión:** encontrar la pelota, ponerse detrás de ella mirando al arco rival,
y empujarla (el robot no tiene pateador: empuja con el cuerpo, por inercia).

### Las conductas y el camino

```
        ARRANCA el partido
              │
              ▼
  ┌─────────┐   ┌──────────┐   ┌──────────────────────────┐
  │ SAQUE   │──►│ BUSCAR   │◄──│ (vuelve acá si pierde la pelota)
  │KICKOFF  │   │ SEARCH   │
  └─────────┘   └────┬─────┘
                     │ veo la pelota
              ┌──────┴──────┐
              ▼             ▼
        ¿estoy alineado con el arco?
            no │         │ sí
               ▼         ▼
      ┌──────────┐ ┌──────────┐  ┌─────────┐  ┌────────────┐
      │ RODEAR   │►│ ACERCARSE│─►│ EMPUJAR │─►│ RETROCEDER │──► BUSCAR
      │ POSITION │ │ APPROACH │  │  PUSH   │  │ PUSH_BACK  │
      └──────────┘ └──────────┘  └─────────┘  └────────────┘
```

| En el código | Qué significa | Qué hace en una línea |
|---|---|---|
| `WAIT_START` | ESPERAR | Quieto. Espera la señal de arranque del árbitro. |
| `KICKOFF` | SAQUE | Saque inicial: empujón recto al frente por un ratito, después busca. |
| `SEARCH` | BUSCAR | Avanza despacio girando hasta que la cámara ve la pelota. |
| `POSITION` | RODEAR | Rodea la pelota para quedar detrás de ella, mirando al arco. |
| `APPROACH` | ACERCARSE | Va derecho a la pelota, girando para mirarla de frente. |
| `PUSH` | EMPUJAR | Pelota cerca y bien apuntado → empuje fuerte por un tiempo fijo, "a ciegas". |
| `PUSH_BACK` | RETROCEDER | Retroceso corto para despegarse de la pelota y de la línea. |
| `LINE_AVOID` | HUIR DE LA LÍNEA | (Aparte) Vio la línea blanca del borde → retrocede para no salirse de la cancha. |

> **Por qué "ponerse detrás de la pelota".** Es una técnica clásica de
> RoboCup: nunca empujás la pelota desde cualquier lado. Primero te ponés DETRÁS
> de ella, en la línea imaginaria pelota→arco. Así, cuando empujás, la pelota va
> hacia el arco y no para afuera.

> **Por qué empuja "a ciegas".** Cuando la pelota queda pegada al paragolpes,
> las cámaras ya no la ven. Por eso la conducta EMPUJAR no vuelve a mirar la
> pelota: se decide y empuja fuerte por un tiempo fijo (igual que pateaba el
> robot del 2025).

---

## 6. El arquero — el que ataja (`goalkeeper_tick`)

**Misión:** quedarse pegado a su línea de arco, ir de lado a lado tapando el
arco, y salir a sacar la pelota cuando se acerca.

### Las conductas y el camino

```
   ARRANCA (con una pequeña demora)
        │
        ▼
  ┌──────────────────────────┐
  │ IR A LA LÍNEA / GOTO_LINE │   se acomoda contra su arco:
  │  primero: retrocede recto │   va para atrás hasta tocar la línea,
  │  después: avanza ~3 cm    │   después se despega un poquito
  └────────────┬─────────────┘
               ▼
        ┌────────────┐  veo la pelota   ┌────────────┐  pelota muy cerca  ┌─────────┐
        │ PATRULLAR  │ ───────────────► │ INTERCEPTAR│ ─────────────────► │ DESPEJAR│
        │ PATROL     │ ◄─────────────── │ INTERCEPT  │ ◄───────────────── │  CLEAR  │
        │ de lado a  │  perdí la pelota │ seguir a   │  la pelota se alejó │ sacar la│
        │ lado       │                  │ la pelota  │                     │ pelota  │
        └────────────┘                  └────────────┘                    └─────────┘
```

| En el código | Qué significa | Qué hace en una línea |
|---|---|---|
| `WAIT_START` | ESPERAR | Quieto. Tras el arranque espera un ratito (para que lo acomodes) y va a acomodarse. |
| `GOTO_LINE` | IR A LA LÍNEA | Se acomoda: retrocede hasta tocar su línea, después avanza un poco para no quedar pisándola. |
| `PATROL` | PATRULLAR | Va de lado a lado tapando el arco; rebota cuando toca la línea del costado. |
| `INTERCEPT` | INTERCEPTAR | Sigue **dónde va a estar** la pelota (la adivina un poquito antes), no dónde está ahora. |
| `CLEAR` | DESPEJAR | Pelota muy cerca → sale a empujarla lejos del arco. |
| `LINE_AVOID` | HUIR DE LA LÍNEA | (Aparte) Retroceso de emergencia por el borde. |

> **Ir de costado.** El arquero se mueve de lado sin girar el cuerpo, como un
> cangrejo (en inglés esto se llama *strafe*). El robot tiene ruedas especiales
> que le dejan ir para cualquier lado mirando siempre al frente.

> **Por qué "adivina dónde va a estar la pelota".** En vez de seguir la posición
> de AHORA, apunta a dónde va a estar dentro de un instante. Si la pelota está
> quieta, "dónde va a estar" = "dónde está", así que se porta igual que antes
> (es una red de seguridad: cuando no hay info nueva, no inventa nada).

### El arquero tiene DOS versiones

Esto confunde al leer. Hay un segundo arquero, **más sencillo**, escondido
detrás de un interruptor de compilación llamado `GK_SIMPLE_STRAFE`:

- **Sin ese interruptor** → la máquina de arriba (PATRULLAR + INTERCEPTAR + DESPEJAR).
- **Con ese interruptor** → un arquero mínimo: se acomoda, va de lado a lado
  cuidando el rumbo, rebota en las líneas y (si querés) se centra con la pelota.
  Es el que se usó en las prácticas con los chicos.

Las dos versiones viven en `goalkeeper_tick()`. La versión simple está PRIMERO y
termina ahí (con un `return`) antes de llegar a la versión completa. **Al leer,
fijate qué versión estás armando** para saber cuál corre.

---

## 7. Las reglas que mandan sobre todo lo demás

Antes del `switch` de conductas, cada máquina tiene un grupo de **reglas con
prioridad máxima**. Son reglas que ganan sin importar en qué conducta estés:

1. **¿El árbitro paró el partido?** → todos quietos, a ESPERAR.
2. **¿Apareció la línea blanca del borde?** → a HUIR DE LA LÍNEA (delantero) o a
   la maniobra de despegue (arquero). El robot **nunca** se tiene que salir de la
   cancha.
3. **¿Recién arrancó el partido?** → al saque (KICKOFF).

Es como las reglas de seguridad de un auto: el freno gana sobre el acelerador,
no importa qué estabas haciendo. Por eso este grupo va ANTES del `switch`.

> **Hay un freno todavía más prioritario que NO está acá.** El freno de borde de
> verdad-verdad vive en
> [`main_central.cpp`](../../software/teensy/Soccer%202026/src/central/main_central.cpp)
> y se revisa en CADA vuelta del programa (no 100 veces por segundo, sino todo el
> tiempo), para frenar en menos de 15 milésimas de segundo. Si frena, ni siquiera
> llama a `strategy_tick()`. Por eso a veces parece que el robot "no le hace caso
> a la estrategia": hay una capa de seguridad por encima.

---

## 8. Por qué el archivo es largo y enredado (y no es tu culpa)

Lo notaste bien: es un archivo difícil de leer. Las razones son concretas, y casi
todas son **buena información** (cosas que costó mucho aprender):

1. **Cada número de ajuste arrastra su historia de banco.** Vas a ver
   comentarios de 10 líneas para explicar por qué un número es `107` y no `42`.
   No es relleno: el número solo, sin la historia, es peligroso. Si el próximo lo
   cambia sin saber por qué está ahí, rompe lo que costó tres pruebas de banco.
2. **Muchísimos interruptores de compilación** (en el código aparecen como
   `#ifdef`). Cada uno prende o apaga una variante de conducta que se probó en
   banco. La regla del equipo: **todos apagados por defecto** → el programa de la
   competencia es siempre el mismo, las variantes solo se prenden a propósito para
   una prueba. Eso te obliga a mirar "qué está prendido" antes de creerle al
   código.
3. **Dos arqueros en una sola función** (punto 6) y dos delanteros (con y sin
   giroscopio).
4. **El código guarda la cicatriz de cada error.** El temblequeo, los giros
   violentos, la "J" del retroceso, la "medialuna"… cada uno dejó su parche y su
   comentario.

### Cómo leerlo sin marearte (orden que conviene)

1. **`strategy.h`** — la "tapa" del programa. 48 líneas. Qué entra, qué sale, qué
   se puede configurar. Empezá acá.
2. **`strategy_tick()`** (al final del `.cpp`) — el portero que elige delantero o
   arquero.
3. **El grupo de reglas con prioridad** de la máquina que te interese (punto 7).
4. **Cada rama (`case`) del `switch` como si fuera una mini-función.** No leas el
   archivo de arriba a abajo: andá directo a la conducta que te importa y leé SOLO
   esa rama.
5. **Ignorá todos los números de ajuste la primera vez.** Son para afinar. Volvé a
   ellos solo cuando quieras cambiar una conducta puntual.

---

## 9. Lo que NO está en este archivo (las conexiones con el resto)

`strategy.cpp` es solo el cerebro táctico. A propósito NO contiene:

| Tema | Dónde vive de verdad |
|---|---|
| De dónde salen los datos (pelota, línea, rumbo) | `world_model` + lo arman las placas de ARRIBA y ABAJO |
| Repartir la orden de movimiento a las 3 ruedas | `kinematics` + `motors_zircon` |
| Las fórmulas que corrigen solas el error (lazos PID) | `src/shared/pids` |
| Ponerse detrás de la pelota (la cuenta) | `src/shared/behind_ball` |
| Adivinar dónde va a estar la pelota | `src/shared/ball_predict` |
| **El freno de borde de emergencia** (más prioritario que la estrategia) | `main_central.cpp` |

> Por eso el cerebro es "flaco": las cuentas pesadas se las deja a piezas
> separadas que están probadas una por una en la compu (sin el robot).
> `strategy.cpp` solo organiza: decide *cuándo* usar cada una.

---

## 10. Para llevarte

- `strategy.cpp` = **mirar lo que pasa → elegir UNA conducta → dar una orden de
  movimiento (costado, frente, giro)**, 100 veces por segundo.
- Hay **dos cerebros** en el archivo (el que ataca y el que ataja); cuál corre se
  decide al armar el programa.
- Una **máquina de estados** = las conductas de un jugador: una a la vez + reglas
  claras de cuándo cambiar.
- **Delantero:** ESPERAR → SAQUE → BUSCAR → RODEAR → ACERCARSE → EMPUJAR → RETROCEDER.
- **Arquero:** ESPERAR → IR A LA LÍNEA → PATRULLAR ↔ INTERCEPTAR ↔ DESPEJAR.
- Las **reglas con prioridad** (árbitro, línea, saque) ganan sobre la conducta del
  momento.
- El archivo es largo porque guarda **la historia de cada decisión de banco** y
  **muchas variantes con interruptores** (todas apagadas por defecto).
- Lo pesado (ruedas, fórmulas de corrección, adivinar la pelota, freno de borde)
  **no está acá**: está en otras piezas y en `main_central.cpp`.

---

### Para profundizar

- Documento detallado de la máquina: [`FIRMWARE-PLACA-CENTRAL.md` §8](FIRMWARE-PLACA-CENTRAL.md)
- Versión de la máquina pensada para probar en la compu (35 pruebas): `src/shared/strategy_transitions`
- Qué pieza es la "oficial" para cada tema: [`FUENTES-DE-VERDAD.md`](../FUENTES-DE-VERDAD.md)
