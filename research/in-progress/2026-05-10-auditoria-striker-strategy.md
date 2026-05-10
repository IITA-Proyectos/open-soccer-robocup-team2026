---
title: "Auditoría — skills/striker-strategy.md vs código real del delantero"
date: 2026-05-10
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: draft
tags: [estrategia, software, delantero, auditoria, analisis]
robot: delantero
area: control
tipo: analisis
---

# Auditoría — `skills/striker-strategy.md` vs código real

## Resumen ejecutivo

El playbook `striker-strategy.md` describe una FSM de 4 estados (SEARCH/APPROACH/POSITION/PUSH) con WorldModel, partner WiFi, Kalman ball, multi-camera fusion, orbit ball y behind-the-ball geométrico. **Nada de eso está implementado**. El código real del delantero (`definitivo-delantero.cpp`, 1215 líneas) implementa una FSM ~21 estados con estrategia "oscilación horario/antihorario" alineándose a `Yp - Ycontrincante`. Son dos sistemas distintos. Adicionalmente: el archivo del delantero contiene ~180 líneas de código muerto del arquero (copy-paste no usado), polaridad de campo hardcoded, protocolo UART frágil, y BNO055 ya funcionando contra lo que dice la timeline.

8 temas-a-analizar detectados (2 × P0, 4 × P1, 2 × P2). Tiempo total estimado si se atacan todos: ~5 días de trabajo del equipo.

## Alcance

- **Playbook auditado:** `skills/striker-strategy.md`
- **Código fuente real:** `software/robot-delantero/definitivo-delantero.cpp` (1215 líneas, `#define ROBOT2`)
- **Variante alternativa no auditada:** `software/robot-delantero/delantero-sin-zirconLib.cpp` (para revisión separada).
- **Fecha de auditoría:** 2026-05-10
- **Objetivo:** contrastar lo que el playbook dice con lo que el código hace, generar temas-a-analizar accionables para decisión del coach.

---

## Hallazgo meta-nivel (necesita decisión antes que los demás)

El playbook **no describe el robot real**. Describe una aspiración técnica avanzada (Kalman, WorldModel, partner, multi-camera fusion, behind-the-ball) que ningún archivo del repo implementa.

El código real implementa una **estrategia distinta pero funcional**: el robot avanza hacia la pelota, oscila lateralmente para alinearse con el arco rival, y patea. Es más simple, no usa partner, no usa Kalman, no usa visión geométrica avanzada — pero **fue suficiente para ganar el Nacional 2025**.

Esto crea un problema de coaching: cualquier persona (Virginia 2027, nuevos alumnos, otro coach) que lea el playbook va a tomar decisiones basadas en código inexistente. Es la primera deuda a saldar.

---

## Temas-a-analizar

### T1 — Polaridad de campo hardcoded (qué arco es el rival)

**Categoría:** estrategia / control
**Robot afectado:** delantero (también aplica a arquero, por verificar)
**Prioridad:** P0

**Qué observo.** En `definitivo-delantero.cpp:354-356`:
```cpp
ARCO_CONTRINCANTE = hayarco_amarillo;
Ycontrincante = Yam;
```
La decisión "qué arco es el rival" se setea estáticamente al arco amarillo. No hay parámetro dinámico, no hay dipswitch, no hay setter por serial.

**Risk-no-fix.** En RCJ, los lados se sortean al inicio de cada partido. ~50% de los partidos en Incheon el equipo va a jugar "del otro lado" — el robot va a alinearse al arco propio y patear gol en contra de forma automática.

**Risk-fix.** Bajo. Lógica candidata: parámetro de compilación `#define ATTACK_YELLOW` o `ATTACK_BLUE`; o lectura de dipswitch al setup; o setter por serial. Hay que asegurar que la cámara detecta consistentemente los 2 colores (cyan/magenta en reglas 2026, no amarillo/azul — **otro tema: nomenclatura desactualizada vs reglas 2026**).

**Tiempo estimado.** 3-4 horas (cambio + dipswitch + test).

**Plan de prueba en hardware real.**
1. Setup: robot en cancha, pelota al centro, arco propio identificado.
2. Configurar "arco rival = amarillo (o magenta). Verificar: robot apunta al arco amarillo.
3. Cambiar configuración a "arco rival = azul (o cyan). Verificar: robot apunta al arco azul.
4. Criterio aceptación: 5/5 intentos por configuración, robot dirige patada al arco correcto.
5. Regresión: `CENTRANDO_horario` y `CENTRANDO_antihorario` siguen funcionando idéntico (no afectar la oscilación).

---

### T2 — `Xp == 0` interpretado como "no hay pelota"

**Categoría:** visión / comunicación
**Robot afectado:** ambos (mismo protocolo UART OpenMV↔Teensy)
**Prioridad:** P0

**Qué observo.** En `definitivo-delantero.cpp:324-330`:
```cpp
if (Xp == 0) { haypelota = false; }
else { haypelota = true; millis_pelota = millis(); }
```
La detección de pelota usa el valor `Xp = 0` como flag "no detectada". Pero la cámara puede reportar `Xp = 0` cuando la pelota está exactamente en el centro horizontal del frame.

**Risk-no-fix.** Cuando la pelota cruza por X = 0 (justo en frente del robot), `haypelota` flickea a `false` por un frame, el FSM puede transicionar a búsqueda y perder la oportunidad de patear. En partido real, el momento crítico (pelota frente al robot, listo para patear) es el más expuesto a este bug.

**Risk-fix.** Cambio coordinado entre OpenMV y Teensy: agregar un byte de flag de detección separado del X,Y; o reservar un valor fuera del rango válido (ej. 255) como sentinel de "no detectada". Hay que tocar ambos firmwares al mismo tiempo.

**Tiempo estimado.** 4-6 horas (cambio protocolo + recompilar ambos + test).

**Plan de prueba en hardware real.**
1. Setup: robot inmóvil, pelota fija centrada en X=0 directamente al frente.
2. Verificar (10s de log): `haypelota == true` estable, sin flickeo a `false`.
3. Setup: pelota fuera del FOV de la cámara.
4. Verificar: `haypelota == false` estable.
5. Setup: pelota en distintas X (-50, -20, 0, 20, 50).
6. Verificar: detección estable en todas las posiciones.
7. Regresión: tiempo total de detección por frame no aumenta más del 5%.

---

### T3 — Código muerto del arquero embebido en archivo del delantero

**Categoría:** software / docs
**Robot afectado:** delantero (archivo del delantero, código del arquero)
**Prioridad:** P1

**Qué observo.** `definitivo-delantero.cpp:1030-1212` contiene los estados `impulso_inicial`, `moverce_derecha/izquierda`, `impulso_derecha/izquierda`, `PATEANDO_*_arquero` (~180 líneas). El estado inicial (`AVANCE_INICIO`, línea 138) no transiciona nunca a estos estados, y ningún estado del delantero los referencia. Es código muerto producto de copy-paste con el archivo del arquero.

**Risk-no-fix.** Confusión cognitiva: cualquier alumno nuevo va a leer 180 líneas tratando de entender qué hacen y cuándo se ejecutan. Riesgo de copy-paste errors en futuro (alguien copia un patrón pensando que se ejecuta). Uso innecesario de flash del Teensy 4.1 (mínimo pero presente).

**Risk-fix.** Cero si genuinamente es código muerto. Verificación previa: `grep` en el switch principal por cada estado, confirmar que ninguno entra. Si se confirma, eliminar bloque entero.

**Tiempo estimado.** 1-2 horas (verificación con grep + borrado + recompilación + commit + journal entry).

**Plan de prueba en hardware real.**
1. Compilación pre-cambio: registrar flash y RAM usage del binario.
2. Borrar bloque (líneas 1030-1212).
3. Compilación post-cambio: verificar baja en flash (debería bajar ~5-10KB).
4. Cargar firmware en robot. Run completo (5 min en cancha).
5. Criterio aceptación: comportamiento idéntico al pre-cambio, sin nuevos warnings de compilación.

---

### T4 — Playbook desconectado del código real

**Categoría:** docs / estrategia
**Robot afectado:** ambos (el playbook habla del delantero, pero el problema es estructural)
**Prioridad:** P1

**Qué observo.** `skills/striker-strategy.md:13-21` describe la FSM SEARCH → APPROACH → POSITION → PUSH con behind-the-ball, orbit, WorldModel, partner. El código real (`definitivo-delantero.cpp`) implementa AVANCE_INICIO → IMPULSO_INICIAL_GIRANDO → GIRANDO → APUNTAR_PELOTA → AVANZANDO → CENTRANDO_horario/antihorario → (con APUNTAR_PELOTA_horario/antihorario para corrección) → PATEANDO_*. Son dos FSM distintas con estrategias distintas.

**Risk-no-fix.** Doc engañosa. Virginia 2027 hereda un playbook que no describe el robot que recibe. Decisiones futuras basadas en código inexistente. Pérdida de credibilidad de toda la knowledge base de `skills/`.

**Risk-fix.** Tres opciones:
- **(a) Borrar el playbook.** Pérdida: se pierde el blueprint aspiracional que sí tiene valor como meta a mediano plazo.
- **(b) Refactor del playbook** en dos partes claras: (i) "Estrategia actual del delantero IITA 2026" describiendo la FSM real con sus 21 estados, magic numbers documentados, casos de uso; (ii) "Propuesta futura: WorldModel + behind-the-ball" como item de roadmap para 2027.
- **(c) Implementar la FSM del playbook** a 7 semanas de Incheon — no recomendable, ambición desproporcionada para el nivel del equipo y el calendario.

**Recomendación:** opción (b).

**Tiempo estimado.** (a) 30 min. **(b) 1 día de trabajo + revisión del equipo.** (c) 3-4 semanas dedicadas.

**Plan de prueba en hardware real.** N/A — es trabajo de doc. Validación:
1. Virginia y Elías leen el playbook actualizado.
2. Pueden identificar cada estado del playbook en el código.
3. Pueden listar los magic numbers y de dónde salen.

---

### T5 — Magic numbers sin trazabilidad

**Categoría:** software
**Robot afectado:** delantero (extensible a arquero)
**Prioridad:** P1

**Qué observo.** El archivo tiene 30+ constantes sin trazabilidad a calibración o decisión:
- Tolerancias: `tolerancia_centrado=30`, `tolerancia_cercania=50`, `tolerancia_apuntado=15`.
- Velocidades: `g=0.3`, `a=0.4`, `pd=1.0`, `c=0.4`, `ic=0.55`.
- Umbrales línea: `blanco1=650`, `blanco2=650`, `blanco3=750` (los 3 distintos — bien — pero sin doc).
- PWM hardcoded: 60, 100, 150, 180.
- Timeouts: 70ms, 200ms, 300ms, 500ms, 700ms, 1000ms, 3000ms, 4000ms, 9000ms, 10000ms, 20000ms, 25000ms.
- Comentarios `// 🛑` marcando "revisar después" (el equipo ya sabe que hay deuda).

**Risk-no-fix.** Imposible iterar sin entender qué hace cada número. Calibración para Incheon (carpet distinto, iluminación distinta) va a ser caótica. Virginia 2027 no puede ajustar nada con confianza.

**Risk-fix.** Refactor a `#define`s nombrados con comentario explicativo + tabla de calibración en `docs/calibration/delantero.md`. No cambia el comportamiento del robot — sólo movemos los números a constantes nombradas.

**Tiempo estimado.** 4-6 horas (refactor + doc + verificar compila idéntico).

**Plan de prueba en hardware real.**
1. Compilar antes del refactor: registrar binary hash.
2. Aplicar refactor (solo renombrar a constantes, mismo valor).
3. Compilar después: hash de binary debería diferir SOLO en metadata de símbolos (mismas constantes embebidas).
4. Cargar firmware. Run completo (5 min cancha).
5. Criterio aceptación: comportamiento idéntico al pre-refactor.
6. Crear `docs/calibration/delantero.md` con tabla: constante | valor actual | qué significa | cómo se recalibra.

---

### T6 — Protocolo UART frágil (sin CRC, sin recovery, baud bajo)

**Categoría:** comunicación
**Robot afectado:** ambos (mismo stack OpenMV↔Teensy)
**Prioridad:** P1

**Qué observo.** En `definitivo-delantero.cpp:287-352`: `Serial1.available() >= 9` + 3 headers fijos (201/202/203), baud 19200, sin CRC, sin packet recovery, sin timeout robusto. Si la cola de UART tiene basura inicial (glitch, reinicio de OpenMV, EMI de motor), el sistema espera 9 bytes que pueden no llegar o llegar desincronizados.

Bonus: el `#define START_BYTE 0xAA;` (línea 84) tiene punto y coma al final — si alguien usa `START_BYTE` en código, expande a `0xAA;` con doble `;`. Inocuo hoy (no se usa) pero es macro mal escrito.

**Risk-no-fix.** En cancha de Incheon: vibración del cable, ruido EMI de motores propios, ruido de cancha vecina → buffer UART desincronizado → robot ciego por segundos sin que nadie en el equipo entienda por qué.

**Risk-fix.** Mejoras incrementales:
- Subir baud a 115200 (más resistente, no requiere cambio de hardware).
- Agregar START_BYTE consistente (0xAA al inicio de cada packet).
- Agregar checksum simple (XOR de bytes).
- Reset del buffer si checksum falla 3 packets seguidos.
- Idealmente: protocolo COBS o similar con escape, pero es más invasivo.

Hay que tocar OpenMV + Teensy juntos.

**Tiempo estimado.** 1 día (cambio coordinado + testing exhaustivo).

**Plan de prueba en hardware real.**
1. Test packet loss en condiciones limpias: 60s de stream, contar packets recibidos vs enviados. Objetivo: > 99.5%.
2. Test packet loss con motor a full PWM (peor EMI): mismo objetivo > 99%.
3. Test recovery: simular byte corrupto (inyectar bytes random en buffer) y verificar que la recepción vuelve a sincronizar en < 100ms.
4. Regresión: latencia OpenMV→Teensy no aumenta más del 10% vs baseline.

---

### T7 — Inconsistencia BNO055: doc dice "deshabilitado", código lo usa

**Categoría:** docs / electrónica
**Robot afectado:** delantero (confirmado integrado); arquero (por verificar)
**Prioridad:** P2

**Qué observo.** En `definitivo-delantero.cpp:76-80, 270-279, 358-365`: BNO055 inicializado, lee `event.orientation.x` para `currentYaw`, calcula `error = currentYaw - initialYaw` con wrap-around 360°, aplica `correccion = error * kp` con `kp = 0.3`. **El IMU está activo y se usa en la corrección.**

Pero `competition/timeline.md:45` dice: *"Abr 2026 | Integrar giróscopo BNO055 | ⏳ Pendiente | Deshabilitado en 2025, resolver problema"*.

**Risk-no-fix.** Doc engañosa. Próximas sesiones (humanas o AI) van a duplicar trabajo asumiendo que BNO055 no funciona. Virginia 2027 puede gastar horas debuggeando algo que ya está resuelto.

**Risk-fix.** Actualizar `competition/timeline.md` + journal entry confirmando estado actual + verificar si el arquero también lo usa (siguiente auditoría, T-goalkeeper).

**Tiempo estimado.** 1 hora (verificación arquero + edición timeline + journal).

**Plan de prueba en hardware real.** Validación que BNO055 funciona bien (no solo "compila"):
1. Robot encendido, leer `currentYaw` en estático durante 30s.
2. Criterio: drift < 2° en 30s estático.
3. Robot girando 90° a comando: verificar que `error` cambia coherentemente.
4. Criterio: error reportado coincide con ángulo real ± 5°.

---

### T8 — Cálculos de ángulos de arcos sin uso

**Categoría:** software
**Robot afectado:** delantero
**Prioridad:** P2

**Qué observo.** En `definitivo-delantero.cpp:312-313`:
```cpp
anguloArco_Amarillo = atan2(Yam, Xam) * 180.0 / PI;
anguloArco_Azul = atan2(Yaz, Xaz) * 180.0 / PI;
```
Pero en el resto del archivo (líneas 314-1215) **estos dos ángulos no se usan**. Solo se usa `Yp - Ycontrincante` para alineación al arco (comparación cartesiana, no angular).

**Risk-no-fix.** Cálculo desperdiciado (mínimo, 2 atan2 por ciclo de comunicación). Confusión: lector cree que es información disponible pero nadie la consume.

**Risk-fix.** Dos caminos:
- **Borrar las 2 líneas** (5 min).
- **Usarlos en una estrategia mejor:** apuntar al arco directamente con `anguloArco_Amarillo` en vez de oscilar buscando `Yp ≈ Ycontrincante`. Esto cambia comportamiento y va junto al refactor de estrategia (T4 opción b).

**Tiempo estimado.** Borrar: 5 min. Usarlos: parte de T4.

**Plan de prueba en hardware real.** Si solo borrar: compilar + ejecutar, comportamiento idéntico. Si usar para apuntar: protocolo separado de calibración (parte de T4).

---

## Cosas bien hechas en el código real (capitalizar, no cambiar)

Una auditoría honesta también marca lo que vale. Estos son patrones que el equipo IITA implementó bien y no hay que tocar:

**A. `IMPULSO_INICIAL_GIRANDO` con torque burst** (líneas 393-403): da PWM=150 por 70ms antes de pasar a velocidad de giro normal (PWM=30 a `g=0.3`). Vence inercia de arranque. Es una técnica de drive control madura, equivalente a "feedforward burst". **Capitalizar:** podemos extenderlo a otros estados (arranque desde parada).

**B. Sensores de línea con thresholds independientes** (`blanco1=650`, `blanco2=650`, `blanco3=750`): reconoce que el tercer sensor (probablemente físicamente distinto o en posición distinta) calibra diferente. Buena disciplina empírica.

**C. Función de "centrado" oscilante** (`CENTRANDO_horario` ↔ `IMPULSO_CENTRANDO_antihorario` ↔ `CENTRANDO_antihorario` ↔ `IMPULSO_CENTRANDO_horario`): es una estrategia real, distinta del playbook pero **funciona**. La oscilación con "impulso" entre direcciones es una forma de explorar el espacio de alineación sin quedar pegado en un local minimum. **Documentar como técnica IITA** en el playbook refactoreado (T4).

**D. Corrección de heading con BNO055** (`error = currentYaw - initialYaw` con wrap 360°, `correccion = error * kp`): lógica correcta para manejar el wrap-around del IMU. Base sólida para extender a control más fino.

**E. `DETECTA_LINEA_1/2/3` con retrocesos angulares distintos** (`retroceder1/2/3`): respeta la geometría del omnidireccional — la "reacción a línea" depende de cuál sensor disparó. Esto es geometría correcta de un omni 3-ruedas.

**F. Comentarios `// 🛑` en magic numbers**: el equipo ya marcó qué constantes son sospechosas/de revisión. Aprovechemos esos marcadores para priorizar el refactor del T5.

**G. Estado `IMPULSO_CENTRANDO_*` separado del `CENTRANDO_*`**: separar el impulso del régimen permanente es un patrón limpio de FSM (mejor que tener "fase 1" y "fase 2" dentro de un mismo estado).

---

## Próximos pasos

1. **Coach + equipo deciden** qué de los P0/P1 atacar antes de Incheon. Mi recomendación de orden:
   - **T1 (polaridad de campo)** primero. P0, 3-4 horas, gana partidos. No negociable.
   - **T4 (playbook refactor)** segundo. P1, 1 día, deja el repo coherente para Virginia 2027.
   - **T2 (Xp==0)** tercero. P0, 4-6 horas, evita comportamiento errático.
   - T3, T5, T6, T7, T8 — atacar según tiempo disponible post-Incheon (capitalizar para Nacional Nov 2026).
2. **Auditoría siguiente:** `goalkeeper-strategy.md` vs código del arquero. Mismo formato.
3. **Verificación cruzada:** este archivo entra a `research/in-progress/`. Cuando se ejecuten los temas, cierre del análisis en `research/completed/`.

---

## Apéndice — datos de la auditoría

- **Líneas leídas del código:** 1-1215 de `definitivo-delantero.cpp`.
- **Líneas leídas del playbook:** todo `skills/striker-strategy.md`.
- **No auditado en esta pasada:** `software/robot-delantero/delantero-sin-zirconLib.cpp` (variante alternativa), `software/vision/` (código OpenMV — necesario para T2).
- **Tiempo de auditoría:** ~1 hora de trabajo de coach + LLM.
