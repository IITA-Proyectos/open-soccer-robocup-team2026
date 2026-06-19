---
title: "Análisis FIEL — zirconLib (primitivas motor/PWM, 2025)"
date: 2026-06-18
status: referencia-fiel
tipo: analisis-codigo-historico
fuente: "software/_deprecated-2025/zirconLib/zirconLib.{cpp,h} + cruce delantero-sin-zirconLib.cpp"
generado-por: "Claude (workflow 14 agentes, análisis línea por línea + crítico de completitud)"
---

> **TRANSCRIPCIÓN FIEL del código del Nacional 2025 (Buenos Aires, campeones).** Cada valor citado con `archivo:línea`.
> Objetivo: reconstruir el comportamiento EXACTO en el robot 2026 (más sensores), sin perder nada.
> ⚠️ Los DOS programas (`definitivo-arquero` y `definitivo-delantero`) son el MISMO firmware unificado arquero+delantero; cambian el `#define` de robot (arquero=ROBOT1, delantero=ROBOT2) y el estado inicial que selecciona el modo.
> ⚠️ Las mejoras con sensores nuevos van en una sección APARTE y marcada — NO contaminan la transcripción.
> Completitud (crítico adversarial que re-leyó el programa): **extracción de primitivas (motor1/2/3, mapeo PWM, pines, detección de versión)**.

# zirconLib — Extracción FIEL de primitivas de movimiento (Nacional 2025)

Archivos leídos COMPLETOS:
- `C:/Users/violl/dev/open-soccer-robocup-team2026/software/_deprecated-2025/zirconLib/zirconLib.h` (28 líneas)
- `C:/Users/violl/dev/open-soccer-robocup-team2026/software/_deprecated-2025/zirconLib/zirconLib.cpp` (356 líneas)
- `C:/Users/violl/dev/open-soccer-robocup-team2026/software/_deprecated-2025/robot-delantero/delantero-sin-zirconLib.cpp` (1061 líneas) — el MISMO delantero con la librería expandida inline

> NOTA DE NOMBRE: la tarea menciona `robot-delantero/delantero-sin-zirconLib.cpp`. El archivo existe con ese nombre exacto. (El otro, `definitivo-delantero.cpp`, NO fue parte de esta tarea.)

> ACLARACIÓN IMPORTANTE sobre el cruce: zirconLib expone **solo primitivas de bajo nivel por motor** (`motor1/motor2/motor3`). Las primitivas de ALTO nivel (`girar`, `avanzar`, `parar`, `retroceder`, etc.) **NO están en zirconLib** — viven dentro del programa del robot. El archivo `delantero-sin-zirconLib.cpp` es justamente la versión que **NO usa zirconLib**: reemplaza `motorN(power,dir)` por escrituras directas `analogWrite(PWMn,...)` + `digitalWrite(INAn/INBn,...)`. Por eso este archivo "revela el PWM": muestra qué combinación de dir+power produce cada movimiento compuesto. Documento ambas capas.

---

## PARTE A — zirconLib: la primitiva real de motor

### A.1 Constante global de saturación de PWM

`zirconLib.cpp:9`
```cpp
#define motorLimit 100
```
- Tope DURO de PWM aplicado en TODA llamada a `motorN`. Ver A.3. (En un Teensy `analogWrite` por defecto es 8 bits, 0–255; acá se autolimita a 100.)

### A.2 Firmas públicas (header)

`zirconLib.h:19-23`
```cpp
void motor1(int power, bool direction);
void motor2(int power, bool direction);
void motor3(int power, bool direction);
```
- `power` = magnitud PWM (entero).
- `direction` = sentido booleano (0 / 1).

Otras públicas del header (no de movimiento, pero declaradas):
`zirconLib.h:5,7,11,13,15,17,25,27`
```cpp
void setZirconVersion();           // detecta Mark1 vs Naveen1
void InitializeZircon();           // setea versión + pines
double readCompass();              // BNO055 euler.x  (devuelve 0 si no calibrado)
int  readBall(int ballSensorNumber);   // 1..8
int  readLine(int lineNumber);         // 1..3
int  readButton(int buttonNumber);     // 1..2
void initializePins();
String getZirconVersion();
```
> AMBIGUO/INCONSISTENTE: `zirconLib.h` declara `void InitializeZircon()` pero NO declara `bool isCompassCalibrated()` aunque está definida en el `.cpp` (línea 349). `CalibrateCompass()` está comentada en ambos archivos (`.h:9`, `.cpp:62-92`).

### A.3 Mapeo a PWM por motor — fórmula EXACTA (`motor1`, `motor2`, `motor3`)

Las tres funciones son IDÉNTICAS en estructura, solo cambian los pines. Ejemplo `motor1` (`zirconLib.cpp:173-192`); `motor2` (194-212); `motor3` (214-232).

```cpp
void motor1(int power, bool direction) {
  power = min(power, motorLimit);               // SATURACIÓN: power = min(power, 100)
  if (ZirconVersion == "Naveen1") {             // --- variante Naveen1 (PWM por pin de dirección) ---
    if (direction == 0) {
      analogWrite(motor1dir1, 0);               // DIR1 = 0
      analogWrite(motor1dir2, power);           // DIR2 = power
    } else {
      analogWrite(motor1dir1, power);           // DIR1 = power
      analogWrite(motor1dir2, 0);               // DIR2 = 0
    }
  } else if (ZirconVersion == "Mark1") {        // --- variante Mark1 (DIR digital + PWM dedicado) ---
    digitalWrite(motor1dir1, direction);        // DIR1 = direction
    digitalWrite(motor1dir2, !direction);       // DIR2 = NOT direction
    analogWrite(motor1pwm, power);              // PWM = power
  } else {
    Serial.println("Zircon version not defined");// --- versión no seteada: NO mueve, solo imprime ---
  }
}
```

Resumen FIEL del mapeo (vale para los 3 motores):

| Versión | Si `direction==1` | Si `direction==0` | Pin PWM |
|---|---|---|---|
| **"Mark1"** | `dir1=HIGH`, `dir2=LOW`, `pwm=power` | `dir1=LOW`, `dir2=HIGH`, `pwm=power` | pin PWM dedicado (`motorNpwm`) |
| **"Naveen1"** | `dir1=power`, `dir2=0` (PWM) | `dir1=0`, `dir2=power` (PWM) | NO hay pin PWM; el PWM va por el pin de dirección |
| otra (sin definir) | nada (imprime "Zircon version not defined") | idem | — |

Notas FIELES:
- **Saturación**: `power = min(power, motorLimit)` con `motorLimit=100`. SOLO tope superior. **NO hay piso de PWM ni deadzone**: si `power<min(...)` no se eleva, y `power` negativo no se filtra (no hay `max(power,0)` ni `abs`). El sentido lo da SOLO `direction`, no el signo de `power`.
- **Mark1**: dirección es DIGITAL (un pin HIGH y el otro su negado), velocidad por pin PWM separado. Esquema "PWM + 2 DIR".
- **Naveen1**: NO usa pin PWM separado (`motorNpwm` está comentado en los pines, A.5). El PWM se inyecta en uno de los dos pines de dirección; el otro queda en 0. Esquema "2× PWM-DIR" (estilo driver tipo TB6612/DRV con IN1/IN2 ambos PWM-capaces).
- **NO existe `direction` inversa por software adicional**: el sentido físico de cada rueda depende del cableado, capturado luego en los movimientos compuestos del programa (Parte B).

### A.4 Variables de pin (declaración)

`zirconLib.cpp:11-19`
```cpp
int motor1dir1; int motor1dir2; int motor1pwm;
int motor2dir1; int motor2dir2; int motor2pwm;
int motor3dir1; int motor3dir2; int motor3pwm;
```
Asignación de pines según versión, en `initializePins()`:

**ZirconVersion == "Mark1"** (`zirconLib.cpp:235-261`):
| Motor | dir1 | dir2 | pwm |
|---|---|---|---|
| motor1 | 2 | 5 | 3 |
| motor2 | 8 | 7 | 6 |
| motor3 | 11 | 12 | 4 |

**ZirconVersion == "Naveen1"** (`zirconLib.cpp:263-272`):
| Motor | dir1 | dir2 | pwm |
|---|---|---|---|
| motor1 | 3 | 4 | (comentado: `// motor1pwm = 3;`) |
| motor2 | 6 | 7 | (comentado: `// motor2pwm = 6;`) |
| motor3 | 5 | 2 | (comentado: `// motor3pwm = 4;`) |

**else (versión NO seteada — fallback)** (`zirconLib.cpp:290-316`):
| Motor | dir1 | dir2 | pwm |
|---|---|---|---|
| motor1 | 2 | 5 | 3 |
| motor2 | 8 | 7 | 6 |
| motor3 | 12 | 11 | 4 |
> OJO: el fallback es CASI igual a Mark1 PERO motor3 tiene dir1/dir2 INVERTIDOS (12/11 vs Mark1 11/12). `zirconLib.cpp:297-298`.
> ADEMÁS: este `else` asigna pines pero como `ZirconVersion` no es "Mark1" ni "Naveen1", `motorN()` cae en la rama final que solo imprime "Zircon version not defined" → **no movería**. Caso teóricamente inconsistente; en la práctica `setZirconVersion()` siempre asigna "Mark1" o "Naveen1".

`pinMode` de motores como OUTPUT: `zirconLib.cpp:320-328` (los 9 pines dir1/dir2/pwm de los 3 motores).

### A.5 Detección de versión

`zirconLib.cpp:52-60`
```cpp
void setZirconVersion() {
  pinMode(32, INPUT_PULLDOWN);
  if (digitalRead(32) == LOW)  ZirconVersion = "Mark1";
  else                         ZirconVersion = "Naveen1";
}
```
- Pin **32** con pull-down: LOW → "Mark1"; HIGH → "Naveen1". Es un jumper/strap de hardware que decide TODO el esquema de motores.

### A.6 Inicialización

`zirconLib.cpp:40-50`
```cpp
void InitializeZircon() {
  setZirconVersion();   // primero detecta versión
  initializePins();     // luego asigna pines y pinMode
  //CalibrateCompass(); // COMENTADO
}
```

> NOTA FIEL: el `.cpp` termina con una llave de cierre `}` extra y suelta en `zirconLib.cpp:355`, después de `isCompassCalibrated()` (líneas 349-351). **AMBIGUO en línea 355**: ese `}` final no corresponde a ninguna función abierta visible — sería error de compilación tal cual está. (Posible artefacto de copia/pegado al deprecar.)

---

## PARTE B — Primitivas de movimiento COMPUESTAS (en `delantero-sin-zirconLib.cpp`)

Aquí está lo que la tarea llama "la librería expandida inline". Estas funciones NO usan `motorN()`; escriben PWM/dir directo. La correspondencia es:
- `analogWrite(PWMx, power)` ≡ `power` de `motorN`.
- `digitalWrite(INAx, ...); digitalWrite(INBx, ...)` ≡ `dir1/dir2` de `motorN` en variante **Mark1** (DIR digital + PWM dedicado). Es decir, **este robot delantero usa el esquema Mark1** (PWM + 2 pines DIR digitales).

### B.0 Pines de motor (ROBOT1 vs ROBOT2) — captura de AMBOS

`#if defined(ROBOT2)` (`delantero-sin-zirconLib.cpp:12-23`):
| Motor | INA | INB | PWM |
|---|---|---|---|
| M1 | INA1=8 | INB1=7 | PWM1=6 |
| M2 | INA2=11 | INB2=12 | PWM2=4 |
| M3 | INA3=2 | INB3=5 | PWM3=3 |

`#if defined(ROBOT1)` (`delantero-sin-zirconLib.cpp:40-51`):
| Motor | INA | INB | PWM |
|---|---|---|---|
| M1 | INA1=2 | INB1=5 | PWM1=3 |
| M2 | INA2=8 | INB2=7 | PWM2=6 |
| M3 | INA3=11 | INB3=12 | PWM3=4 |

> OBSERVACIÓN FIEL del cruce: los pines de **ROBOT1** coinciden exactamente con los de **zirconLib "Mark1"** (M1=2/5/3, M2=8/7/6, M3=11/12/4). En **ROBOT2** los roles M1↔M2 y los pines están permutados respecto de Mark1. Esto confirma que `delantero-sin-zirconLib.cpp` es la expansión inline del esquema Mark1.
- Selección de robot activo: `delantero-sin-zirconLib.cpp:9-10`: `//#define ROBOT1` comentado, `#define ROBOT2` activo. **El build actual es ROBOT2.**
- `pinMode(...OUTPUT)` de los 9 pines: `delantero-sin-zirconLib.cpp:277-285`.

### B.1 Constantes de potencia/velocidad (AMBOS robots)

Umbrales de línea (blanco), patada y factores de centrado:

`ROBOT2` (`delantero-sin-zirconLib.cpp:29-37`):
```
blanco1 = 650 ; blanco2 = 650 ; blanco3 = 750
patadM2 = 170 ; patadM1 = 250
c  = 0.4      ; ic = 0.55
```
`ROBOT1` (`delantero-sin-zirconLib.cpp:53-64`):
```
blanco1 = 600 ; blanco2 = 600 ; blanco3 = 600
patadM2 = 170 ; patadM1 = 250
c  = 0.4      ; ic = 0.5
```
Significado declarado (`delantero-sin-zirconLib.cpp:68-69`): `c` = velocidad centrando, `ic` = velocidad impulso centrando.

Factores de velocidad globales (NO dependen del robot) (`delantero-sin-zirconLib.cpp:94-98`):
```
g  = 0.3   // girando
a  = 0.4   // apuntando pelota
pd = 1     // velocidades de avance proporcionales (multiplicador)
```
Patada con rampa (`delantero-sin-zirconLib.cpp:76-80`):
```
velocidadActualPateo = 0 (inicial)
velocidadFinalPateo  = 240
pasoPateo            = 5
intervaloPateo       = 20  // ms entre incrementos
```
Giroscopio (`delantero-sin-zirconLib.cpp:88`): `kp = 0.3`. Serial: `BAUD_RATE = 19200` (`:92`), `START_BYTE = 0xAA` (`:91`).
Tolerancias (`delantero-sin-zirconLib.cpp:125-127`): `tolerancia_centrado = 30.0`, `tolerancia_cercania = 50.0`, `tolerancia_apuntado = 15.0`.

> NOTA: estas primitivas NO aplican el `motorLimit=100` de zirconLib (al no pasar por `motorN`). Por eso pueden escribir PWM hasta 240/250 directo (`avanzar_patear`, `retroceder_patear`). **No hay piso de PWM ni deadzone en NINGUNA primitiva** — todos los valores son fijos por tabla.

### B.2 `girar()` — rotación en el lugar

`delantero-sin-zirconLib.cpp:149-153`
```cpp
void girar() {
  analogWrite(PWM1, 100*g); digitalWrite(INA1,0); digitalWrite(INB1,1);
  analogWrite(PWM2, 100*g); digitalWrite(INA2,0); digitalWrite(INB2,1);
  analogWrite(PWM3, 100*g); digitalWrite(INA3,0); digitalWrite(INB3,1);
}
```
- PWM en cada rueda = `100*g = 100*0.3 = 30`.
- Las 3 ruedas con MISMO patrón de dirección `(INA=0, INB=1)` → rotación pura sobre el eje (sentido dado por el cableado; en el FSM se usa como "antihorario" base). El impulso de arranque más fuerte está inline en `IMPULSO_INICIAL_GIRANDO` con PWM=150 (`:425-427`), mismo patrón dir.

### B.3 `parar()` — freno

`delantero-sin-zirconLib.cpp:155-159`
```cpp
void parar() {
  analogWrite(PWM1,0); digitalWrite(INA1,0); digitalWrite(INB1,0);
  analogWrite(PWM2,0); digitalWrite(INA2,0); digitalWrite(INB2,0);
  analogWrite(PWM3,0); digitalWrite(INA3,0); digitalWrite(INB3,0);
}
```
- PWM=0 y AMBOS pines DIR en 0 en las 3 ruedas → motores libres/freno por driver (coast/brake según H-bridge; con IN1=IN2=0 típicamente coast).

### B.4 `avanzar()` — avance recto (sin pelota)

`delantero-sin-zirconLib.cpp:160-164`
```cpp
void avanzar() {
  analogWrite(PWM1,100); digitalWrite(INA1,1); digitalWrite(INB1,0);
  analogWrite(PWM2,100); digitalWrite(INA2,0); digitalWrite(INB2,1);
  analogWrite(PWM3,0);   digitalWrite(INA3,1); digitalWrite(INB3,0);
}
```
- M1: PWM=100, dir (1,0). M2: PWM=100, dir (0,1) [opuesto a M1]. M3: **PWM=0** (rueda trasera apagada).
- Geometría omni de 3 ruedas: las dos delanteras empujan en sentidos opuestos de su eje y la trasera queda inerte → vector de avance recto. Confirma rueda M3 = trasera.

### B.5 Retrocesos por sensor de línea — `retroceder1/2/3()`

Cada uno aleja del sensor que detectó blanco (M1=izq, M2=centro, M3=der según comentarios del loop `:399-401`).

`retroceder1()` `delantero-sin-zirconLib.cpp:166-170`:
```cpp
analogWrite(PWM1,0);   INA1=0; INB1=1;   // M1 apagada
analogWrite(PWM2,100); INA2=0; INB2=1;
analogWrite(PWM3,100); INA3=1; INB3=0;
```
`retroceder2()` `delantero-sin-zirconLib.cpp:171-175`:
```cpp
analogWrite(PWM1,100); INA1=1; INB1=0;
analogWrite(PWM2,0);   INA2=1; INB2=0;   // M2 apagada
analogWrite(PWM3,100); INA3=0; INB3=1;
```
`retroceder3()` `delantero-sin-zirconLib.cpp:176-180`:
```cpp
analogWrite(PWM1,100); INA1=0; INB1=1;
analogWrite(PWM2,100); INA2=1; INB2=0;
analogWrite(PWM3,0);   INA3=0; INB3=1;   // M3 apagada
```
- Patrón: en cada uno la rueda del sensor disparado va a PWM=0 y las otras dos a 100 con direcciones tales que el robot se traslada (strafe) alejándose de esa línea. Duración fija 400 ms en el FSM (`DETECTA_LINEA_1/2/3`, `:1027,1039,1051`).

### B.6 Patada — `avanzar_patear()` y `retroceder_patear()`

`avanzar_patear()` `delantero-sin-zirconLib.cpp:196-216` (rampa no bloqueante):
```cpp
if (millis() - tiempoAnteriorPateo >= intervaloPateo /*20ms*/) {
  tiempoAnteriorPateo = tiempoActual;
  if (velocidadActualPateo < velocidadFinalPateo /*240*/) {
    velocidadActualPateo += pasoPateo; // +5
    if (velocidadActualPateo > 240) velocidadActualPateo = 240;
  }
  analogWrite(PWM1, velocidadActualPateo); INA1=1; INB1=0;
  analogWrite(PWM2, velocidadActualPateo); INA2=0; INB2=1;
  analogWrite(PWM3, 0);                     INA3=0; INB3=0;  // trasera apagada
}
```
- Rampa: cada 20 ms suma 5 al PWM hasta tope 240. Mismo patrón direccional que `avanzar()` (M1 (1,0), M2 (0,1)) pero con PWM creciente y M3 totalmente off (coast). Es el avance fuerte para impulsar la pelota.

`retroceder_patear()` `delantero-sin-zirconLib.cpp:219-223`:
```cpp
analogWrite(PWM1, patadM1 /*250*/); INA1=0; INB1=1;
analogWrite(PWM2, patadM2 /*170*/); INA2=1; INB2=0;
analogWrite(PWM3, 0);               INA3=1; INB3=0;  // trasera off (coast con IN1=1,IN2=0? -> no, PWM=0)
```
- Retroceso asimétrico: M1=250, M2=170 (PWM distintos por rueda → corrige el tiro hacia atrás), direcciones invertidas respecto a `avanzar_patear`. M3 PWM=0.
- Valores `patadM1/patadM2` iguales en ROBOT1 y ROBOT2 (250/170).

### B.7 Avances proporcionales con corrección por error de yaw — `aiproporcional()` / `adproporcional()`

Usan `error` (yaw actual − inicial, ver loop `:392-394`) y el multiplicador global `pd=1`. Tres ramas por rango de error.

`aiproporcional()` (avance izquierdo proporcional) `delantero-sin-zirconLib.cpp:226-248`:
| Condición | M2 (izq) dir/PWM | M1 (der) dir/PWM | M3 (atrás) dir/PWM |
|---|---|---|---|
| `-2 < error < 2` | (0,1) `pd*50` | (0,1) `pd*50` | (1,0) `pd*89` |
| `error > 0` | (0,1) `pd*50` | (0,1) `pd*50` | (1,0) `pd*40` |
| `error < 0` | (0,1) `pd*65` | (0,1) `pd*40` | (1,0) `pd*100` |
(comentarios al lado muestran valores "objetivo" 60/60/99, 60/60/50, 75/50/120 — son referencias, NO se aplican.)

`adproporcional()` (avance derecho proporcional) `delantero-sin-zirconLib.cpp:250-272`:
| Condición | M2 (izq) dir/PWM | M1 (der) dir/PWM | M3 (atrás) dir/PWM |
|---|---|---|---|
| `-2 < error < 2` | (1,0) `pd*50` | (1,0) `pd*50` | (0,1) `pd*89` |
| `error > 0` | (1,0) `pd*50` | (1,0) `pd*50` | (0,1) `pd*100` |
| `error < 0` | (1,0) `pd*65` | (1,0) `pd*40` | (0,1) `pd*40` |
(comentarios: 60/60/99, 60/60/120, 75/50/50.)

- Son strafes laterales (izq/der) con la rueda trasera M3 modulada por el error de heading para enderezar la trayectoria. `pd=1` ⇒ los PWM son literalmente 50/50/89, 50/50/40, 65/40/100 (ai) y 50/50/89, 50/50/100, 65/40/40 (ad).
> NOTA: `aiproporcional`/`adproporcional` están DEFINIDAS pero NO se invocan en el `switch` del loop de este archivo (búsqueda en el FSM: el centrado se hace inline en `CENTRANDO_*`). Quedan como primitivas disponibles.

### B.8 Movimientos compuestos definidos INLINE dentro del FSM (no como función)

Estos no son funciones nombradas pero son primitivas de movimiento reales con sus PWM:

- **Impulso de giro fuerte** (`IMPULSO_INICIAL_GIRANDO`, `:425-427`): las 3 ruedas PWM=150, dir (0,1). Versión potente de `girar()` por 70 ms.
- **Apuntar pelota** (`APUNTAR_PELOTA` y variantes `_horario/_antihorario`, `:543-552, 792-801, 837-846`): las 3 ruedas PWM=`100*a=40`; si `anguloPelota>0` dir (0,1) [antihorario], si `<0` dir (1,0) [horario]. Rotación lenta para encarar la pelota; tolerancia ±15°.
- **Centrado horario** (`CENTRANDO_horario`, `:645-647`): M1 PWM=`60*c`, M2 PWM=`60*c`, M3 PWM=`180*c`, dirs (M1 0,1)(M2 0,1)(M3 1,0). Con `c=0.4` ⇒ 24/24/72. Orbita la pelota.
- **Centrado antihorario** (`CENTRANDO_antihorario`, `:716-718`): mismos PWM (24/24/72) con todas las direcciones invertidas.
- **Impulso centrando** (`IMPULSO_CENTRANDO_antihorario` `:704-706`, `IMPULSO_CENTRANDO_horario` `:775-777`): M1/M2 PWM=`60*ic`, M3 PWM=`180*ic`. Con `ic=0.55` (ROBOT2) ⇒ 33/33/99; con `ic=0.5` (ROBOT1) ⇒ 30/30/90. Duración 500 ms (antihorario) / 300 ms (horario).

---

## PARTE C — Síntesis del modelo de motores (load-bearing)

1. **Convención de potencia/dirección (la primitiva atómica)**: cada rueda se comanda con `analogWrite(PWM, magnitud)` + dos pines de dirección complementarios `(INA, INB)`. En zirconLib esto es `motorN(power, direction)` con `direction` ↔ `(INA=dir, INB=!dir)`. El signo del movimiento lo da SIEMPRE `direction`, nunca el signo de `power`.

2. **Dos esquemas eléctricos según strap pin 32**: "Mark1" (PWM dedicado + 2 DIR digitales) y "Naveen1" (PWM inyectado en el pin de dirección, sin PWM dedicado). El delantero (ROBOT1/ROBOT2) usa el esquema Mark1 expandido inline.

3. **Saturación única, sin deadzone**: el ÚNICO recorte es `power = min(power, 100)` dentro de zirconLib. Las primitivas inline del delantero NO pasan por ahí y escriben hasta 240/250 directo. **No hay compensación de zona muerta ni piso de PWM en ninguna capa** — todos los movimientos usan PWM fijos tabulados (afinados a mano por banco), modulados solo por los factores `g/a/c/ic/pd`.

4. **Rol de ruedas (deducido del PWM)**: M1 y M2 son las dos delanteras (en `avanzar`/`avanzar_patear` van a PWM≠0 con dirs opuestas), M3 es la trasera (queda en 0 al avanzar recto, y se modula al máximo —72/99/100— en los strafes de centrado y avances proporcionales para corregir heading). Geometría omni de 3 ruedas a ~120°.

5. **Diferencias ROBOT1 vs ROBOT2**: solo cambian (a) la asignación de pines INA/INB/PWM por motor (B.0), (b) `blanco1/2/3` (600/600/600 vs 650/650/750), y (c) `ic` (0.5 vs 0.55). `patadM1/patadM2` y todos los demás factores son iguales.

### Inconsistencias/ambigüedades marcadas (FIEL)
- `zirconLib.cpp:355` — llave `}` de cierre suelta tras `isCompassCalibrated()`; no cierra ninguna función abierta → no compilaría tal cual. AMBIGUO.
- `zirconLib.h` NO declara `isCompassCalibrated()` aunque el `.cpp` la define.
- Fallback `else` en `initializePins` (`:290-316`) asigna pines de "casi-Mark1" pero deja `ZirconVersion` sin valor válido para `motorN`, que entonces no movería (rama "Zircon version not defined"). Caso inconsistente, no alcanzable con `setZirconVersion()` normal.
- `motorN()` no filtra `power` negativo ni aplica piso: con `power<0`, `min(power,100)=power` (negativo) → `analogWrite` recibiría negativo. Ninguna de las primitivas del delantero pasa valores negativos, así que no se dispara en la práctica.
