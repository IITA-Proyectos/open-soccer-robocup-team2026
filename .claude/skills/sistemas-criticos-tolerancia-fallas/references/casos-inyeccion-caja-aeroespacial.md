# Casos de estudio — inyección electrónica, caja por cable, control aeroespacial

> Referencia de `sistemas-criticos-tolerancia-fallas`. Los tres dominios que pediste,
> cada uno como un producto real donde se juntan tiempo real + control + tolerancia a
> fallas. Para cada uno: la arquitectura de control, qué lo hace difícil, y cómo falla
> seguro. Voz de estudiante: jerga explicada, el "por qué" antes del detalle.

---

## 1. Inyección electrónica (ECU de motor)

### Qué controla

Cuánto combustible inyectar y cuándo encender la chispa, en cada cilindro, cientos de
veces por segundo, de modo que el motor entregue el par pedido con el mínimo consumo y
emisiones, sin detonar (knock) ni ahogarse.

### Lo que lo hace ÚNICO: control motor-síncrono

**El tiempo no lo marca un reloj fijo — lo marca el cigüeñal.** Las decisiones se
toman en *grados de giro del cigüeñal*, no en milisegundos. Esto es el ejemplo más puro
del principio "el instante ES la corrección" (`tiempo-real-determinismo`):

- **Decodificación de cigüeñal/árbol de levas:** una rueda dentada (típico 60−2 dientes)
  + sensor de efecto Hall/VR da la posición angular; el "−2" (dientes faltantes) es la
  marca de referencia para saber en qué vuelta se está. El cam sensor desambigua el ciclo
  de 4 tiempos (720°). De acá sale **la base de tiempo de todo el sistema**.
- **Scheduling angular:** "inyectar 3,2 ms terminando 60° antes del PMS de admisión del
  cilindro 3" → el firmware programa un timer que dispara el inyector en el ángulo exacto.
  A 7000 RPM hay ~117 vueltas/s: las ventanas son de cientos de microsegundos y el jitter
  cuesta par y emisiones. **Hard real-time motor-síncrono.**

### Los lazos de control

- **Combustible en lazo cerrado por sonda lambda (O₂):** mide la mezcla en el escape y
  corrige el tiempo de inyección para mantener la relación estequiométrica (λ=1). Lazo
  lento (el escape tiene retardo de transporte — `control-embebido-tiempo-real` §5).
- **Mapas (lookup tables) + interpolación:** el "feedforward" base sale de tablas
  RPM×carga calibradas en banco (VE, avance de chispa); el lazo cerrado solo corrige el
  residuo. Es feedforward+feedback de manual.
- **Control de knock:** un sensor de vibración detecta la detonación; el control **atrasa
  la chispa** de ese cilindro hasta que para. Lazo rápido y adaptativo.
- **Ralentí (idle):** lazo de RPM actuando sobre el aire (válvula IAC/throttle) — control
  de velocidad clásico.

### Cómo falla seguro

- **Plausibilidad de sensores:** TPS, MAP, ECT, IAT con rango válido; un sensor fuera de
  rango → valor sustituto (limp).
- **Limp mode (degradación con gracia):** ante falla, capar RPM/par y andar con un mapa
  conservador para "llegar al taller". Estado seguro = potencia limitada o motor cortado.
- **Watchdog + monitor independiente:** muchas ECUs tienen un segundo micro "monitor"
  que vigila al principal (concepto de E-Gas/level-2 monitoring de ISO 26262) y puede
  cortar la inyección si el principal se va de rango.

---

## 2. Caja de cambios con accionamiento electrónico (shift / clutch-by-wire)

### Qué controla

Mover los actuadores que seleccionan y enganchan marchas (y, en cajas automatizadas o
doble embrague, modular el/los embragues) para hacer un cambio suave, rápido y sin dañar
la transmisión — sin conexión mecánica directa palanca↔caja.

### La arquitectura de control: cascada posición+fuerza

El actuador (motor DC/BLDC, solenoide o hidráulico) se controla en **cascada multi-rate**
(`control-embebido-tiempo-real` §6):

- **Lazo interno de corriente** (kHz): controla el par/fuerza del actuador (la corriente
  ∝ par). Rápido, sobre el driver de potencia.
- **Lazo medio de posición/velocidad:** lleva el actuador a la posición de "engranaje
  enganchado" siguiendo un perfil suave (no un escalón — rompería dientes).
- **Lazo externo de fuerza/sincronización:** durante el engrane, controla la FUERZA
  contra el sincronizador, no solo la posición (apretar de más rompe, de menos no engrana).
  Detectar "marcha puesta" por posición + fuerza.

### La máquina de estados del cambio

El corazón es una **FSM** segura: `EN_MARCHA_N → DESEMBRAGAR → SACAR_A_NEUTRAL →
SINCRONIZAR → METER_MARCHA_M → EMBRAGAR → EN_MARCHA_M`, con timeouts y abortos en cada
paso. En doble embrague (DCT), dos sub-cajas (pares/impares) con sus embragues se
solapan: pre-seleccionar la próxima marcha y "cruzar" los embragues → cambio sin corte
de par. Coordinación fina de dos actuadores en tiempo real.

### Cómo falla seguro

- **Estado seguro = nunca un cambio no comandado.** Ante falla, **mantener la marcha
  actual** (o ir a neutral de forma controlada), jamás engranar/desengranar solo a alta
  velocidad (destruye la caja o bloquea las ruedas).
- **Fallback mecánico/hidráulico:** muchas cajas tienen un default por resorte/presión a
  una posición segura si se corta la energía (estado seguro por física, no por software).
- **Realimentación redundante de posición** (dos sensores) para no engranar a ciegas;
  discrepancia → abortar el cambio y mantener.
- **Protección térmica/corriente** del actuador y el embrague (un embrague patinando se
  quema → limitar y proteger).

> Es el dominio más cercano al robot: **control de actuadores electromecánicos con
> realimentación, FSM y fail-safe** — exactamente lo que es mover los motores del omni
> con una FSM táctica y un estado seguro (STOP). Las técnicas se transfieren directo.

---

## 3. Control aeroespacial (fly-by-wire / FADEC)

### Qué controla

- **Fly-by-wire:** las órdenes del piloto (o del piloto automático) se convierten en
  señales que mueven las superficies de control (alerones, timón, elevadores) vía
  actuadores, con leyes de control que estabilizan y protegen la envolvente de vuelo.
- **FADEC (Full Authority Digital Engine Control):** el "ECU" del motor de turbina —
  controla flujo de combustible, geometría variable, etc., con autoridad total.

### Lo que lo define: la redundancia NO es un extra, es la arquitectura

- **Canales redundantes con votación:** triple o cuádruple redundancia; computadoras de
  vuelo que calculan en paralelo y **votan** (2-de-3, 2-de-4). El A320 tiene 7
  computadoras de vuelo de 2 tipos; el 777 usa triple-triple. Una falla no derriba el
  sistema.
- **Diversidad de diseño:** canales con hardware y software DISTINTO (distinto procesador,
  distinto equipo, a veces distinto lenguaje) para que un mismo bug no esté en todos.
- **FDIR permanente:** cada canal se auto-testea y compara con los otros; un canal que
  discrepa se vota afuera y se aísla.

### Particionado temporal y espacial (ARINC 653)

En aviónica modular integrada (IMA), varias funciones de criticidad distinta comparten
un procesador bajo un **RTOS particionado (ARINC 653)**: cada partición tiene su ventana
de tiempo GARANTIZADA y su espacio de memoria AISLADO → una función no puede robarle CPU
ni corromper memoria a otra. Es `rtos-scheduling-embebido` llevado al extremo de "una
falla en la partición de entretenimiento NO puede tocar la de control de vuelo".

### Las leyes de control y la degradación

- **Leyes de control con protección de envolvente:** "normal law" limita ángulo de ataque,
  factor de carga, banqueo — el avión no deja que el piloto lo lleve a pérdida.
- **Degradación explícita:** normal law → **alternate law** → **direct law** → mecánico de
  respaldo, perdiendo protecciones de a una a medida que fallan sensores/canales. El piloto
  siempre conserva control, con menos red. Es degradación con gracia de manual.

### La cultura: DO-178C

Software clasificado por **DAL (Design Assurance Level) A–E** según la consecuencia de su
falla (A = catastrófico). DAL A exige trazabilidad total requisito→diseño→código→test,
cobertura estructural **MC/DC**, análisis de WCET demostrable (por eso se desactiva/acota
la cache: para poder *calcular* el peor caso), y sin código muerto. El costo de
verificación supera por mucho al de escribir el código — porque el deadline es vidas.

---

## El hilo común (lo que te llevás a CUALQUIER sistema, incluido el robot)

1. **Definí el estado seguro primero** y hacé que la falla caiga ahí por física.
2. **El instante importa tanto como el valor** (motor-síncrono, ventanas de inyección,
   lazos de vuelo) — es la lección de `tiempo-real-determinismo` en su forma extrema.
3. **Control en cascada multi-rate** (corriente→posición→fuerza→misión) — el patrón
   universal del control de actuadores (`control-embebido-tiempo-real`).
4. **Redundancia + votación + diversidad + FDIR** escalan la confiabilidad; el robot usa
   versiones chiquitas (CRC, outlier-rejection, degradar sin BNO).
5. **Degradá con gracia, no caigas entero.** Limp mode, alternate law, navegar sin un
   sensor — siempre un escalón más, nunca el abismo.
