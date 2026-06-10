# TEST-CARDS de banco — Placa CENTRAL (Teensy 4.1 sobre Zircon Rev v15)

> ✅ **ACTUALIZADO 2026-06-06 — programas que antes faltaban YA EXISTEN** (creados aditivos; verificar con `pio run -e <env>`, NO compilan en el gate host):
> - **CARD CENTRAL-5 (brake vs COAST):** env **`diag_central_brake`** + `src/diag/diag_central_brake.cpp` ✅ creado.
> - **CARD CENTRAL-7 (auto-reset WDOG1):** env **`central_robot1_wdt_hangtest`** (= `central_robot1_wdt` + `-DCENTRAL_WDT_HANG_TEST`) + hook gateado en `main_central.cpp` ✅ creado.
> - **CARD CENTRAL-8 (cap 70%):** el módulo puro `motor_power_cap.h` existe; el **wiring** en `motors_zircon.cpp` + constante en `config_central.h` sigue PENDIENTE (cambia binario → sesión `pio`).

> Tarjetas de verificación **rápidas** (idealmente <5 min) para la placa CENTRAL,
> el master de motores del robot. Cada card es autocontenida: comando exacto,
> qué esperar, cómo interpretar el resultado y **qué pegar de vuelta a la IA**.
>
> Convención de rutas: todos los comandos `pio` se corren desde
> `software/teensy/Soccer 2026` (envoltura: `cd "software/teensy/Soccer 2026" && pio ...`).
> Monitor serial: `pio device monitor -b 115200` salvo que la card diga otro baud.
>
> ⚠️ **Antes de cualquier card de motores**: SUJETAR el robot o ponerlo con las
> ruedas al aire, batería cargada (los H-bridge NO andan por USB), y tener a mano
> el botón físico (pin 9) o el Serial Monitor para arrancar/parar.
>
> Fuentes verificadas: `platformio.ini`, `src/diag/`, `src/central/config_central.h`,
> `src/central/motors_zircon.cpp`, `src/central/main_central.cpp`,
> `src/shared/robot_config/robot2.h`, `src/shared/kinematics.{h,cpp}`.

---

## Índice de subsistemas
1. [Motores ROBOT2 — pines / inversión](#1-motores-robot2)
2. [Cinemática — WHEEL_ANGLES (tuneo fino del lateral + sentido)](#2-cinematica-wheel_angles)
3. [Freno (brake vs coast del Zircon)](#3-freno-brake-vs-coast)
4. [Watchdog de hardware (WDOG1)](#4-watchdog-de-hardware)
5. [Cap de potencia 70% (incluye wiring)](#5-cap-de-potencia-70)
6. [Ingest de línea DOWN→CENTRAL + freno de borde](#6-ingest-de-linea-downcentral--freno-de-borde)

---

<a name="1-motores-robot2"></a>
## 1. Motores ROBOT2

### CARD CENTRAL-1: Mapa físico de los 3 motores R2
> ✅ **HECHA — banco 2026-06-09 (Gustavo):** la disposición de R2 resultó **IGUAL a R1** —
> M1=U5(2/5/3)=delantera-IZQ · M2=U17(8/7/6)=delantera-DER · M3=U7(11/12/4)=trasera.
> La suposición "drivers ROTADOS" venía del delantero 2025 y es FALSA en el robot2 2026.
> Ya cargado en `config_central.h` (rama ROBOT2). Se conserva la card como procedimiento.
- **Objetivo:** confirmar qué "Motor N" del firmware mueve qué rueda física del **delantero (ROBOT2)** y que los 3 H-bridge energizan. Importa porque `robot2.h` advierte que R2 puede tener los drivers ROTADOS o mal soldados respecto a R1.
- **Placa:** CENTRAL (Teensy 4.1).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_central_motors -t upload`
- **¿Existe el programa?:** SÍ. Env `[env:diag_central_motors]` en `platformio.ini:368`; sketch `src/diag/diag_central_motors.cpp`. Es agnóstico al rol: los pines están **hardcodeados** en la tabla `MOTORS[]` (`diag_central_motors.cpp:` `MOTOR 1=U5(2/5/3)`, `MOTOR 2=U17(8/7/6)`, `MOTOR 3=U7(11/12/4)`). PWM tope de seguridad = 128/255 (50%).
- **Setup físico:** robot del DELANTERO con ruedas al aire o sujeto. USB conectado. Batería cargada. Botón pin 9 accesible (o usar ENTER por Serial).
- **Pasos:**
  1. Flashear y abrir monitor (`-b 115200`).
  2. Apretar el botón (pin 9) o ENTER → arranca Motor 1 con onda creciente/decreciente.
  3. Apretón/ENTER de nuevo → Motor 1 para, arranca Motor 2; otra vez → Motor 3; otra vez → FIN.
  4. **Anotar** qué rueda física gira en cada paso (frente / izq / der).
- **Qué esperar si PASA:** la serial imprime `>>> Arrancando MOTOR 1`, luego `MOTOR 2`, luego `MOTOR 3`; las 3 ruedas giran, una por paso.
- **Resultados posibles:**
  - **A)** Los 3 motores giran, uno por paso → cableado OK; anotar el mapa motor→rueda de R2.
  - **B)** El Motor 2 (U17, pines 8/7/6) NO se mueve → en R2 esos pines no llegan al H-bridge (o conflicto 7/8 con Serial2). El sketch lo avisa en serial.
  - **C)** Un motor gira pero **al revés** del esperado → la inversión por HW de R2 difiere de R1; anotar cuál motor (índice) va al revés.
- **Feedback a devolver a la IA:** pegar literal el mapa medido, p.ej.
  `MOTOR1=rueda DERECHA gira CW; MOTOR2=rueda IZQUIERDA NO gira; MOTOR3=rueda TRASERA gira CW`.
  Eso fija `PIN_INA/INB/PWM` y `MOTOR_INVERT[]` reales en `robot2.h` (hoy placeholders con `// TODO: confirmar en banco`).
- **Tiempo estimado:** 4 min.

### CARD CENTRAL-2: Sentido de giro / MOTOR_INVERT R2
> ✅ **HECHA — banco 2026-06-09 (Gustavo):** los 3 motores de R2 giraron HORARIO con el drive
> directo del diag → **`MOTOR_INVERT={+1,+1,+1}`** (el U17 de ESA placa NO está invertido por
> HW, a diferencia de la Zircon de R1). Ya cargado en `config_central.h` (rama ROBOT2).
- **Objetivo:** validar el array `MOTOR_INVERT[3]` para ROBOT2 (hoy copiado de R1 `{+1,-1,+1}` **sin validar**, con la trampa de índices documentada en `config_central.h:50` y `robot2.h`).
- **Placa:** CENTRAL (Teensy 4.1).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_central_motors -t upload` (misma corrida que CENTRAL-1, mirando el SENTIDO).
- **¿Existe el programa?:** SÍ (mismo `diag_central_motors`). El sketch tiene su propio `MOTOR_DIR[3]={+1,+1,+1}` editable; cuando un motor gire al revés, se le cambia el signo y se recompila. Ese resultado es lo que va a `MOTOR_INVERT[]` de producción.
- **Setup físico:** igual que CENTRAL-1, con una marca visible en cada rueda para ver el sentido.
- **Pasos:**
  1. Correr CENTRAL-1 y observar cada rueda con el comando "adelante" nominal.
  2. Para cada rueda que gire en sentido equivocado, anotar su índice.
- **Qué esperar si PASA:** las 3 ruedas, al comando "adelante", empujan el robot hacia adelante de forma coherente (no en diagonal ni rotando).
- **Resultados posibles:**
  - **A)** Las 3 coherentes con `{+1,-1,+1}` → R2 = igual que R1, confirmar.
  - **B)** El motor índice 0 (en R2 = driver U17) va al revés → el array correcto sería `{-1,+1,+1}` (caso advertido en `config_central.h:54`).
  - **C)** Otra combinación → anotar exactamente cuáles índices invertir.
- **Feedback a devolver a la IA:** pegar `MOTOR_INVERT correcto para R2 = {a,b,c}` con los signos medidos. Ej: `R2 MOTOR_INVERT={-1,+1,+1} (el U17 quedó en índice 0 y va invertido)`.
- **Tiempo estimado:** 3 min (sobre la misma corrida de CENTRAL-1).

---

<a name="2-cinematica-wheel_angles"></a>
## 2. Cinemática (WHEEL_ANGLES)

### CARD CENTRAL-3: ¿La cinemática traslada limpio? (tuneo fino + sentido)
> ✅ **ROBOT2: VALIDADO en banco 2026-06-09 (Gustavo, `diag_central_strafe_robot2_kick` — "anda bien")**
> con el **motion lateral estándar** (3 técnicas): piso por rueda `MOTOR_MIN_PWM={70,70,107}`
> (trasera barrida 42→107) + impulso inicial `{130,130,140}` PWM ×40 ms (`-DCENTRAL_MOTOR_KICKSTART`)
> + freno anticipado de la trasera 66 ms (`-DCENTRAL_REAR_BRAKE_LEAD`). Para **ROBOT1**, que arranca
> de esos MISMOS valores, ver la **CARD CENTRAL-3b** (verificación pendiente).
- **Objetivo:** con `WHEEL_ANGLES_DEG={330,210,90}` (**CALIBRADO 2026-06-08**, `config_central.h:113`), confirmar que el strafe es **traslación pura** y hacer el **tuneo fino del lateral** (que no rote) + **confirmar el SENTIDO** de la traslación (si va al revés, sacar el +180 → `{150,30,270}`). El strafe abre lazo con `omega=0`: si las ruedas/ángulos están bien, va de costado sin rotar. (La vieja `{60,-60,180}` estaba en el eje equivocado y daba círculos; ya corregida — esto es ajuste fino, no diagnóstico de círculos.)
- **Placa:** CENTRAL (Teensy 4.1).
- **Programa / env (arquero):** `cd "software/teensy/Soccer 2026" && pio run -e diag_central_strafe_robot1 -t upload`
  - delantero: `-e diag_central_strafe_robot2`
- **¿Existe el programa?:** SÍ. Envs `[env:diag_central_strafe_robot1]` (`platformio.ini:490`) / `_robot2` (`:506`); sketch `src/diag/diag_central_strafe.cpp`. Pasa por `motors_apply_command → inverse_kinematics` real con `WHEEL_ANGLES_DEG`. Open-loop (omega=0, sin BNO). Patrulla ~30 cm izq → pausa → ~30 cm der → loop.
- **Setup físico:** **robot en el piso de la cancha (NO al aire)** para ver la trayectoria. Espacio libre ~1 m a cada lado. Batería cargada.
- **Pasos:**
  1. Flashear, abrir monitor (`-b 115200`).
  2. Botón pin 9 (o ENTER) → arranca patrulla IZQUIERDA.
  3. Observar la trayectoria: ¿va recto de costado o curva/rota?
- **Qué esperar si PASA:** el robot se desliza lateral **mirando al frente** (sin girar) ~30 cm a cada lado. Serial imprime el estado `STRAFE`/`PAUSE` y la dirección.
- **Resultados posibles:**
  - **A)** Traslación lateral limpia, deriva mínima → `WHEEL_ANGLES` OK.
  - **B)** El robot **rota residual** mientras debería ir recto → resta tuneo fino del lateral. Para ROBOT1 los ángulos ya están CALIBRADOS (`{330,210,90}`) y `MOTOR_INVERT={+1,-1,+1}` validado, así que un residual chico es ajuste fino (no los "círculos" de la vieja `{60,-60,180}`). Para ROBOT2 (esta sección) `MOTOR_INVERT` sigue sin validar → **resolver CENTRAL-1/2 ANTES** de concluir sobre los ángulos de R2.
  - **C)** Va en diagonal constante → un solo motor invertido o un ángulo de rueda errado.
- **Feedback a devolver a la IA:** describir literal lo observado, p.ej.
  `strafe_robot1: el robot gira en círculo en sentido horario en vez de ir de costado` o `va recto de costado, deriva ~5cm en 30cm`. Si hay físico medido de los ángulos de montaje de las ruedas, pegarlos (grados desde +X).
- **Tiempo estimado:** 5 min.

### CARD CENTRAL-3b: Motion lateral estándar en ROBOT1 — verificar los valores validados de R2
- **Objetivo:** verificar en el **arquero (ROBOT1)** los valores del motion lateral estándar que R2 validó en banco 2026-06-09: piso `MOTOR_MIN_PWM={70,70,107}` + impulso inicial `{130,130,140}` PWM ×40 ms + freno anticipado de la trasera 66 ms. R1 ARRANCA de esos valores por decisión de Gustavo (2026-06-09), pero su historia previa avisa: el `{70,70,42}` viejo de R1 era del banco 2026-06-08, donde la trasera se BAJÓ porque con piso alto el robot **rotaba** en el strafe.
- **Placa:** CENTRAL (Teensy 4.1) del ROBOT1.
- **Programa / env:** la base es `diag_central_strafe_robot1`, pero ⚠️ **ese env HOY no activa las 3 técnicas** (los flags `-DCENTRAL_MOTOR_KICKSTART` / `-DCENTRAL_REAR_BRAKE_LEAD` solo están en `diag_central_strafe_robot2_kick`). Falta un env espejo `diag_central_strafe_robot1_kick` (= `diag_central_strafe_robot1` + esos 2 flags; **OJO**: su `build_src_filter` explícito necesita además `+<shared/motor_kickstart.cpp>`, copiar el patrón del env `_robot2_kick` en `platformio.ini`). Compila el equipo (sesión `pio`).
- **Setup físico:** ROBOT1 en el piso de la cancha (NO al aire), ~1 m libre a cada lado, batería CARGADA (>7,6 V — batería floja invalida el test).
- **Pasos:**
  1. Flashear el env con las 3 técnicas activas, monitor `-b 115200`.
  2. Botón pin 9 (o ENTER) → patrulla lateral IZQ → pausa → DER → loop.
  3. Observar: arranque (¿rompen la inercia las 3 ruedas?), trayectoria (¿recto sin rotar?), frenada (¿se desacomoda la cola al parar?).
- **Qué esperar si PASA (criterio):** el robot **strafea derecho SIN rotar** (las delanteras arrancan sin quedarse, la trasera sostiene la relación 2:1) y **NO se desacomoda al frenar** (el corte anticipado de la trasera evita que su inercia patee la cola).
- **Resultados posibles:**
  - **A)** Strafea derecho y frena limpio → R1 confirma los valores de R2; quitar la leyenda "A VERIFICAR EN BANCO R1" de `config_central.h`.
  - **B)** **Rota** durante el strafe → la trasera de R1 se adelanta con 107: bajar `MOTOR_MIN_PWM[2]` GRADUALMENTE (107→95→85→…; la historia del 42 está en git y en el comentario de `config_central.h`).
  - **C)** Las delanteras no rompen la inercia → subir idx0/idx1 (70→90…; NO pasar ~150, los motores 5V a 7,4 V se queman).
  - **D)** Se desacomoda al frenar → tunear `-DDIAG_STRAFE_REAR_LEAD_MS` (subir si la cola sigue rodando; bajar si frena antes de tiempo).
- **Feedback a devolver a la IA:** pegar literal, p.ej. `R1 strafea derecho sin rotar con {70,70,107}, frena sin desacomodarse` o `R1 ROTA con 107; con 85 va derecho` + el lead usado si se tocó.
- **Tiempo estimado:** 8 min (más el env nuevo si no existe).

### CARD CENTRAL-4: Avance recto con HeadingPID (drive)
- **Objetivo:** validar la cadena completa de avance con corrección de heading: `WorldSnapshot → world_model → HeadingPID → kinematics → motores`. Confirma que adelante/atrás salen rectos (cinemática + signo de heading juntos).
- **Placa:** CENTRAL (Teensy 4.1). **Requiere placa TOP encendida mandando snapshots** por Serial7.
- **Programa / env (arquero):** `cd "software/teensy/Soccer 2026" && pio run -e diag_central_drive_robot1 -t upload`
  - delantero: `-e diag_central_drive_robot2`
- **¿Existe el programa?:** SÍ. Envs `[env:diag_central_drive_robot1]` (`platformio.ini:440`) / `_robot2` (`:463`); sketch `src/diag/diag_central_drive_straight.cpp`. Secuencia botón pin 9: WAITING → FORWARD 3s → PAUSED 1s → REVERSE 3s → DONE. Flags: `-DDIAG_DRIVE_SPEED_MM_S` / `-DDIAG_DRIVE_DURATION_MS`.
- **Setup físico:** robot en el piso, pasillo recto ≥1.5 m. TOP cableada (TOP TX4/pin17 → CENTRAL RX7/pin28) + GND común, alimentada. Batería cargada.
- **Pasos:**
  1. Encender la TOP (que mande snapshots). Flashear la CENTRAL y abrir monitor.
  2. Botón pin 9 → FORWARD 3s; observar si va recto.
  3. Esperar PAUSED y REVERSE; observar.
- **Qué esperar si PASA:** avanza recto ~SPEED·3s adelante, pausa, vuelve recto atrás. Serial muestra `snap_fresh=Y` y el estado FORWARD/REVERSE.
- **Resultados posibles:**
  - **A)** Recto adelante y atrás → cadena + heading OK.
  - **B)** `snap_fresh=N` / no arranca el PID → la TOP no llega (revisar enlace, ver CARD CENTRAL-7 análoga / `top[rxB=...]`).
  - **C)** Se va de costado / corrige al lado equivocado → signo de heading o cinemática (cruzar con CENTRAL-3).
- **Feedback a devolver a la IA:** pegar la línea de debug de la CENTRAL durante FORWARD (la que tiene `snap_fresh=` y `hdg=`) + descripción de la trayectoria.
- **Tiempo estimado:** 5 min.

---

<a name="3-freno-brake-vs-coast"></a>
## 3. Freno (brake vs coast)

### CARD CENTRAL-5: ¿motors_brake frena o queda en COAST?
- **Objetivo:** medir si `motors_brake()` (corto activo INA=INB=HIGH, PWM=0, `motors_zircon.cpp`) **frena de verdad** en el Zircon Rev v15 o si las ruedas quedan **libres (coast)**. CRÍTICO: el freno de borde de emergencia (`main_central.cpp`) confía en este freno; está marcado `⚠️ FALTA CONFIRMAR en el Zircon` (`main_central.cpp` comentario del bloque FRENO DE BORDE).
- **Placa:** CENTRAL (Teensy 4.1).
- **Programa / env:** **NO existe un diag dedicado a `motors_brake()`.** Falta crear (NO lo crees vos): un sketch `diag_central_brake` que (a) acelere los 3 motores a ~150 PWM unos segundos, (b) al apretar el botón llame `motors_brake()`, y (c) mida/observe el tiempo hasta detenerse. **Mientras tanto**, se puede aproximar con el freno de borde real (ver CARD CENTRAL-9) o con `diag_central_line_sweep_*` que ya invoca el freno ante salida inminente.
- **Setup físico (aproximación con line_sweep):** robot con ruedas al aire; usar `diag_central_line_sweep_robot1` y forzar la condición de freno (línea inminente desde DOWN, o variante con safety) observando cómo paran las ruedas.
- **Pasos (aproximación):**
  1. `pio run -e diag_central_line_sweep_robot1 -t upload`, monitor `-b 115200`.
  2. Botón pin 9 → barrido lateral (ruedas girando).
  3. Disparar el freno (DOWN reportando salida inminente, ver CARD CENTRAL-9).
  4. Observar/cronometrar: ¿las ruedas se clavan o siguen girando por inercia?
- **Qué esperar si PASA (freno real):** al frenar, las ruedas se **detienen casi instantáneo** y oponen resistencia si se las empuja a mano (corto del H-bridge).
- **Resultados posibles:**
  - **A)** Ruedas se clavan + resisten al empuje → BRAKE real, el freno de borde sirve.
  - **B)** Ruedas siguen girando por inercia / giran libres al empujarlas → el Zircon hace **COAST**, no brake → el freno de borde NO protege; hay que cambiar la estrategia (p.ej. invertir PWM breve = freno por reversa, o pedir el diag dedicado).
  - **C)** Solo algunos motores frenan → driver/cableado parcial.
- **Feedback a devolver a la IA:** pegar literal: `motors_brake: ruedas SE CLAVAN y resisten` **o** `ruedas QUEDAN LIBRES (coast), giran al empujar` + tiempo aprox. de detención (s) y desde qué velocidad. Indicar también si falta crear `diag_central_brake`.
- **Tiempo estimado:** 4 min (con line_sweep) / pedir diag dedicado para medición limpia.

---

<a name="4-watchdog-de-hardware"></a>
## 4. Watchdog de hardware

### CARD CENTRAL-6: WDT — 30 min sin reset espurio
- **Objetivo:** confirmar que con el watchdog HW (WDOG1, 1 s) **activado** la CENTRAL corre en marcha normal **30 min sin resets espurios**. Importa porque la CENTRAL es el único master de PWM: si se cuelga y no hay WDT, el robot sigue a ciegas.
- **Placa:** CENTRAL (Teensy 4.1).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e central_robot1_wdt -t upload`
- **¿Existe el programa?:** SÍ. Env `[env:central_robot1_wdt]` (`platformio.ini:976`) = `central_robot1` + `-DCENTRAL_ENABLE_WDT`. El WDT vive en `main_central.cpp` (`watchdog_init_1s()` / `watchdog_feed()`, gateado por `CENTRAL_ENABLE_WDT`, default OFF). Es firmware de competencia con el flag, no un diag aparte.
- **Setup físico:** robot completo en marcha normal (TOP + DOWN cableadas y alimentadas, o al menos la CENTRAL drenando ambos UARTs). Ruedas al aire por seguridad. Batería cargada / fuente estable 30 min.
- **Pasos:**
  1. Flashear `central_robot1_wdt`, abrir monitor.
  2. Anotar el valor inicial de `loop=` y dejar correr 30 min.
  3. Vigilar el monitor: el banner de setup (`IITA Soccer Open — CENTRAL firmware` + `WDT de hardware ARMADO`) NO debe reaparecer (un reset reimprime el setup).
- **Qué esperar si PASA:** un único banner de arranque, `loop=` crece monótono 30 min, aparece `[CENTRAL] WDT de hardware ARMADO (CENTRAL_ENABLE_WDT, 1 s)` UNA vez.
- **Resultados posibles:**
  - **A)** Sin reaparición del banner en 30 min → 0 resets espurios → WDT seguro para competencia.
  - **B)** El banner reaparece y `loop=` se reinicia a ~0 → reset espurio → el loop a veces tarda >1 s (revisar `loop_us(max/avg)` en el debug); NO prender el flag en competencia hasta resolver.
- **Feedback a devolver a la IA:** pegar la última línea de debug a los 30 min (con `loop=` y `loop_us(max/avg)=`) e indicar **cuántos resets** se vieron (0 idealmente). Si hubo reset, pegar el `loop_us(max/avg)` máximo observado.
- **Tiempo estimado:** 30 min (test largo; dejar corriendo en paralelo).

### CARD CENTRAL-7: WDT — auto-reset al colgar el loop
- **Objetivo:** confirmar que el WDOG1 **resetea** el Teensy si el loop se cuelga (la otra mitad del WDT: que sí reaccione al cuelgue real, no solo que no moleste).
- **Placa:** CENTRAL (Teensy 4.1).
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e central_robot1_wdt -t upload` (mismo env que CENTRAL-6).
- **¿Existe el programa?:** El env existe (`central_robot1_wdt`), pero **NO hay un hook para colgar el loop a propósito**. Falta (NO lo crees vos): un flag de banco tipo `-DCENTRAL_WDT_HANG_TEST` que, al recibir una tecla por Serial, entre en `while(1){}` sin alimentar el WDT, para verificar el auto-reset y `WDOG1_WRSR`. Sin eso, se puede aproximar **desconectando uno de los UART de entrada** si eso traba algún ring (no garantizado).
- **Setup físico:** CENTRAL alimentada, monitor abierto.
- **Pasos:**
  1. Flashear `central_robot1_wdt`, abrir monitor, confirmar `WDT ... ARMADO`.
  2. Provocar el cuelgue (con el flag de hang test si se agrega; o el método aproximado).
  3. Esperar ~1 s.
- **Qué esperar si PASA:** a ~1 s del cuelgue, el Teensy se reinicia → reaparece el banner de setup completo (`IITA Soccer Open — CENTRAL firmware ...`) y `loop=` arranca de 0.
- **Resultados posibles:**
  - **A)** Reaparece el banner ~1 s tras colgar → WDT reacciona, auto-reset OK.
  - **B)** No reaparece nunca / queda congelado → el WDT no está reseteando (revisar `watchdog_init_1s` / que `watchdog_feed` esté antes del cuelgue) → NO confiar en el WDT.
- **Feedback a devolver a la IA:** indicar `SÍ reinició a ~Xs` o `NO reinició (quedó colgado)`, y si se pudo leer `WDOG1_WRSR` que el reset fue por WDT. Pedir el flag `CENTRAL_WDT_HANG_TEST` si no existe.
- **Tiempo estimado:** 3 min (si existe el hook de hang).

---

<a name="5-cap-de-potencia-70"></a>
## 5. Cap de potencia 70%

### CARD CENTRAL-8: Aplicar cap 70% y confirmar duty ≤ ~178/255
- **Objetivo:** limitar el PWM de los motores a **~70% (178/255)** para no quemar los motores brushed 5V alimentados a 7.4V (cap obligatorio HOY). Esta card incluye el **wiring exacto** (2 líneas gateadas en `motors_zircon.cpp`) y cómo confirmar que el duty no pasa de ~178.
- **Placa:** CENTRAL (Teensy 4.1).
- **¿Existe el programa?:** El módulo PURO host-testeable **`src/shared/motor_power_cap.h` YA EXISTE** (verificado: `motor_power_cap(int duty, int cap_abs)` + `motor_cap_from_pct(int max_pwm, int pct)`; `motor_cap_from_pct(255,70)==178`; `cap_abs<=0` o `>=255` = passthrough). El wiring va en `src/central/motors_zircon.cpp` con el valor del cap tomado de una constante de `config_central.h`. El env de flasheo es el de competencia `central_robot1` / `central_robot2`. **OJO: tocar `motors_zircon.cpp` cambia el binario de ROBOT1 — aplicar SOLO en sesión con compilación PlatformIO, no acá; acá se documenta.**
- **Wiring exacto (2 líneas gateadas) en `src/central/motors_zircon.cpp`:**
  1. Arriba del archivo, junto a los includes:
     ```cpp
     #include "motor_power_cap.h"   // cap de potencia 70% (PURO, host-testeable)
     ```
  2. Dentro de `apply_pwm_to_motor(...)`, **justo después** de `pwm_signed *= MOTOR_INVERT[motor_idx];` y **antes** del `if (pwm_signed > 0)`:
     ```cpp
     pwm_signed = motor_power_cap(pwm_signed, MOTOR_POWER_CAP_ABS);   // clamp a ±cap, def OFF
     ```
  Donde `MOTOR_POWER_CAP_ABS` es una constante NUEVA en `config_central.h` (p.ej. `constexpr int MOTOR_POWER_CAP_ABS = motor_cap_from_pct(MAX_PWM, MOTOR_POWER_CAP_PCT);` con `MOTOR_POWER_CAP_PCT` gateado: **DEFAULT 100 ⇒ cap = 255 = passthrough = binario idéntico**). `motor_power_cap()` clampa `|pwm|` a `cap_abs` conservando el signo y hace **no-op** si `cap_abs<=0` o `>=255`. Igual patrón de gate que `apply_pwm_floor()` (`kinematics.cpp:41`). Para 70%: `MOTOR_POWER_CAP_PCT=70` ⇒ cap = 178.
- **Build + flash (sesión PlatformIO, NO acá):**
  - `cd "software/teensy/Soccer 2026" && pio run -e central_robot1 -t upload` con el cap activo, p.ej. `-DMOTOR_POWER_CAP_PCT=70` en el `build_flags` de un env de banco (recomendado: env nuevo `central_robot1_pcap` aditivo, NO pisar competencia).
- **Setup físico:** robot con ruedas al aire; multímetro/osciloscopio en la salida PWM de un driver (o medir corriente/velocidad relativa), o usar el diag de motores comandando 100%.
- **Pasos:**
  1. Con el cap aplicado y compilado, comandar el máximo (100% nominal) a un motor (vía drive/strafe a velocidad alta, o el diag de motores subiendo el tope).
  2. Medir el duty efectivo en la línea PWM del driver (o la velocidad/corriente máxima alcanzada vs. sin cap).
- **Qué esperar si PASA:** el duty efectivo se **satura en ~70% (≈178/255)** aunque se pida 100%; sin el flag, llega a 255.
- **Resultados posibles:**
  - **A)** Duty máximo ≈178/255 (~70%) → cap OK, motores protegidos.
  - **B)** Duty llega a 255 con el flag puesto → el `motor_power_cap()` no se está aplicando (revisar que la línea quedó DESPUÉS del `MOTOR_INVERT` y que `MOTOR_POWER_CAP_PCT=70` llegó al build).
  - **C)** Duty mucho menor a 178 → el porcentaje del cap quedó mal (revisar `MOTOR_POWER_CAP_PCT` / `motor_cap_from_pct`).
- **Feedback a devolver a la IA:** pegar el **duty máximo medido** (ej. `178/255` o `~70%`), con qué instrumento, y si el cap estaba activo (`MOTOR_POWER_CAP_PCT=70`).
- **Tiempo estimado:** 6 min (más si se usa osciloscopio).

---

<a name="6-ingest-de-linea-downcentral--freno-de-borde"></a>
## 6. Ingest de línea DOWN→CENTRAL + freno de borde

### CARD CENTRAL-9: Freno de emergencia de borde (línea inminente)
- **Objetivo:** validar end-to-end que cuando DOWN reporta **salida inminente** (línea de borde), la CENTRAL **frena de inmediato** (`world_model_imminent_exit() && world_model_line_is_fresh()` → `motors_brake()`, `main_central.cpp` bloque FRENO DE BORDE, prioridad sobre la FSM, <15 ms).
- **Placa:** CENTRAL (Teensy 4.1). Requiere DOWN cableada mandando `LineStatusV2`.
- **Programa / env (arquero):** `cd "software/teensy/Soccer 2026" && pio run -e diag_central_line_sweep_robot1 -t upload`
  - delantero: `-e diag_central_line_sweep_robot2`
- **¿Existe el programa?:** SÍ. Envs `[env:diag_central_line_sweep_robot1]` (`platformio.ini:750`) / `_robot2` (`:766`); sketch `src/diag/diag_central_line_sweep.cpp`. Barre lateral 4s/lado y **FRENA ante salida inminente / robot levantado / enlace caído**. Variante sin freno: `diag_central_line_sweep_nosafety_robot1` (`:783`). El firmware de competencia (`central_robot1`) implementa el mismo freno de borde.
- **Setup físico:** robot en la **cancha**, sobre la zona blanca de la línea de borde. DOWN calibrada (correr `diag_down_calibracion` antes) y cableada: DOWN TX1(pin1) → CENTRAL RX7(pin28) + GND común, baud 230400. Batería cargada.
- **Pasos:**
  1. Flashear `diag_central_line_sweep_robot1`, monitor `-b 115200`.
  2. Botón pin 9 → arranca el barrido lateral.
  3. Empujar/dejar que el robot llegue al borde blanco (o pasar la línea bajo los sensores de DOWN).
- **Qué esperar si PASA:** al detectar el borde, el robot **frena en seco** (no sigue hacia afuera). Serial: aparece el evento de salida inminente y los motores paran.
- **Resultados posibles:**
  - **A)** Frena al tocar el borde → ingest de línea + freno de borde OK.
  - **B)** No frena, sigue de largo → la línea no llega fresca (revisar enlace, ver CARD CENTRAL-10) o DOWN no la detecta (recalibrar) o `motors_brake` es coast (cruzar con CENTRAL-5).
  - **C)** Frena SIEMPRE / falso positivo continuo → DOWN reporta salida inminente espuria sin calibrar (usar variante `nosafety` solo para ver el movimiento, y recalibrar DOWN).
- **Feedback a devolver a la IA:** pegar literal: `frena al borde SÍ/NO`, y si NO, qué muestra la serial (¿`line_fresh=N`? ¿`valid=N`? ¿`ev=0x..`?). Indicar si DOWN estaba calibrada.
- **Tiempo estimado:** 5 min (más la calibración previa de DOWN).

### CARD CENTRAL-10: Salud del enlace DOWN→CENTRAL (frescura del dato)
- **Objetivo:** confirmar que la CENTRAL recibe y decodifica `LineStatusV2` de DOWN (frames OK, sin CRC errors, `line_fresh=Y`, `valid=Y`). Es el prerequisito del freno de borde (CENTRAL-9).
- **Placa:** CENTRAL (Teensy 4.1). Requiere DOWN encendida.
- **Programa / env:** `cd "software/teensy/Soccer 2026" && pio run -e diag_central_rx_all -t upload`
  (escucha DOWN por Serial1/pin0 + TOP por Serial7/pin28 y decodifica campo por campo).
- **¿Existe el programa?:** SÍ. Env `[env:diag_central_rx_all]` (`platformio.ini:663`); sketch `src/diag/diag_central_rx_all.cpp`. Decodifica `LineStatusV2 + Pose2D + Velocity2D` de DOWN y `WorldSnapshot` de TOP. Alternativa más simple solo-DOWN: `diag_central_comm_down` (`:643`).
  - **OJO de cableado:** `diag_central_rx_all` escucha DOWN por **Serial1 (pin 0)**, mientras que `diag_central_line_sweep` (CENTRAL-9) escucha por **Serial7 (pin 28)**. Conectar el TX de DOWN al pin que corresponda según la card que se corra (rx_all → CENTRAL pin0; line_sweep → CENTRAL pin28). GND común, 230400.
- **Setup físico:** DOWN cableada según la card, alimentada, sensores de línea expuestos al piso. Batería/fuente.
- **Pasos:**
  1. Flashear `diag_central_rx_all`, monitor `-b 115200`.
  2. Pasar la línea blanca bajo los sensores de DOWN.
  3. Leer los contadores y flags del enlace.
- **Qué esperar si PASA:** los frames de DOWN suben (`rx` crece), `crc=0`, `valid=Y`, y los campos de línea cambian al pasar el blanco.
- **Resultados posibles:**
  - **A)** `rx` crece, `crc=0`, `valid=Y`, dato cambia con el blanco → enlace + ingest OK.
  - **B)** No llega nada (rx=0) → cable/pin/GND o DOWN apagada (verificar pin0 vs pin28 según card).
  - **C)** Llegan bytes pero `crc` sube / `badsch` sube → baud/ruido o **schema desfasado** (deploy DOWN viejo: `LineStatusV2` con schema != 2, ver `comm_down_line_schema_rejects` en `main_central.cpp`).
- **Feedback a devolver a la IA:** pegar literal el bloque de telemetría (de `diag_central_rx_all`, o de la línea `down[rx=.. crc=.. lost=.. badsch=.. valid=.. ev=0x..]` del firmware), antes y después de pasar el blanco.
- **Tiempo estimado:** 4 min.

---

## Notas / pendientes para crear (NO en esta card)
- **`diag_central_brake`** (CARD CENTRAL-5): sketch dedicado para medir limpio brake vs coast.
- **`-DCENTRAL_WDT_HANG_TEST`** (CARD CENTRAL-7): hook para colgar el loop a propósito y verificar el auto-reset del WDOG1.
- **`src/shared/motor_power_cap.h`** (CARD CENTRAL-8): YA EXISTE (módulo PURO host-testeable del cap). Falta el **wiring en `motors_zircon.cpp` + la constante `MOTOR_POWER_CAP_PCT`/`_ABS` en `config_central.h`** (cambia binario → sesión PlatformIO), y un env de banco aditivo `central_robot1_pcap` con `-DMOTOR_POWER_CAP_PCT=70` para no pisar el binario de competencia.
- **`MOTOR_INVERT[]` / pines / `WHEEL_ANGLES` de R2** (CARDS CENTRAL-1/2/3): hoy placeholders `// TODO: confirmar en banco` en `src/shared/robot_config/robot2.h`; fijarlos con los resultados de banco.
