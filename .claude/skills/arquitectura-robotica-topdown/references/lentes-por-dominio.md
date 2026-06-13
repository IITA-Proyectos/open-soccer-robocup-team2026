# Lentes por dominio — qué mira PRIMERO un arquitecto senior

> Para cada subsistema, las 3-5 preguntas que un arquitecto hace ANTES de
> entrar al detalle. No es un manual del dominio: es el filtro de "qué importa".
> Usar junto con el método de `SKILL.md`. Cada lente termina en **la pregunta
> que destila el subsistema en una frase**.

## Control / lazos de realimentación / estabilidad

1. **¿En qué RÉGIMEN opera el actuador?** Lineal/proporcional vs cuantizado
   (zona muerta, pisos, bang-bang). El régimen decide TODO el diseño de control;
   un PID de libro falla fuera del lineal. (En este robot: ver
   [[dinamica-omni-3-ruedas]] — <420 mm/s es cuantizado.)
2. **¿Autoridad vs perturbación?** ¿La corrección máxima del lazo supera a la
   perturbación sistemática? Si no, runaway. Medir las dos como números.
3. **¿Latencia DENTRO del lazo?** Sensar→decidir→actuar. Si el retardo se acerca
   al período de la dinámica, oscila aunque las ganancias estén bien. Heading a
   4 Hz vs 100 Hz cambia la estabilidad, no solo la suavidad.
4. **¿Cuantización del sensor y del actuador?** Resolución, pisos de PWM, int
   overflow (omega·100 en int16 desbordó e invirtió el giro). Lo discreto muerde.
5. **Margen de estabilidad:** ¿qué pasa al subir ganancia? ¿Hay un punto donde
   oscila? Ese punto menos un factor de seguridad = dónde vivís.

→ *Frase:* "Este lazo es estable porque [régimen] + [autoridad>perturbación] +
[latencia≪dinámica]; se rompe si [la perturbación crece / la latencia sube]."

## Potencia / electrónica de potencia

1. **Presupuesto:** corrientes pico vs continuas por riel; ¿el regulador
   aguanta el pico de arranque de motores (stall) sin caer?
2. **Cascada de brownout** (el modo de falla más traicionero): batería floja →
   riel 3.3 V cae → sensor reinicia/da basura → localización miente → control
   mal. Un síntoma "de software" cuya causa es eléctrica. Sospechar potencia
   ANTES de debuggear firmware cuando un sensor "desaparece" o da direcciones
   raras (0x64 = brownout, no otro chip).
3. **Caminos de corriente y tierra:** ¿la tierra de potencia (motores) y la de
   señal comparten un punto limpio? Ruido de motor en una línea de señal/botón.
4. **Límites térmicos:** ¿algún componente trabaja fuera de su rango? (motores
   5V a 7.4 V → tope ~70% o se queman). El robot caliente en reposo = bandera.
5. **Qué alimenta qué:** el USB NO alimenta los sensores de la batería — saberlo
   evita perseguir fantasmas (OTOS muertos por riel flojo, no por firmware).

→ *Frase:* "El sistema vive de [batería 2S]; el punto frágil es [brownout del
3.3 V] que se disfraza de [falla de sensor]."

## Localización / posicionamiento / SLAM

1. **¿Cuál es la FUENTE DE VERDAD de la pose?** ¿Una, o fusión? Si fusionás,
   ¿quién gana en conflicto? (heading: BNO vs OTOS vs trilateración).
2. **Deriva vs referencia absoluta:** odometría (OTOS, encoders) deriva sin
   límite; necesita corrección absoluta (cámara, paredes por ToF, línea). SLAM
   completo cierra el lazo con un mapa; este robot hace lo mínimo: trilateración
   directa + heading absoluto, sin mapa persistente. Decidir el nivel correcto:
   no siempre hace falta SLAM.
3. **¿Qué pasa cuando una fuente MIENTE (no muere)?** El caso peligroso no es el
   sensor que se apaga (se detecta), es el que sigue reportando `valid=1` con un
   valor congelado/sesgado y arrastra a la fusión. (Incidente real: el árbitro de
   deriva eligió al BNO congelado como "estable".) Detectar muerte ANTES de
   arbitrar.
4. **Marco de referencia y signo:** ¿robot o mundo? ¿+CCW o +CW? El 80% de los
   bugs de localización son un signo o un marco equivocado. Validar girando a
   mano y mirando si el número sube en la dirección esperada.
5. **Degradación:** si cae la mejor fuente, ¿a qué baja? (sin gyro → línea +
   cámara + OTOS, con ω=0 si heading inválido — no orientar con rumbo falso).

→ *Frase:* "La pose sale de [fuente]; deriva por [X], se corrige con [Y]; si
[fuente] miente, el riesgo es [arrastre] y degradamos a [fallback]."

## Visión / procesamiento de imágenes

1. **¿De qué depende la detección?** Color (LAB) depende de ILUMINACIÓN →
   calibración atada a la sede. Es la fragilidad #1: anda en el lab, no en cancha.
2. **Exposición/WB/gain: ¿fijos o auto?** Auto = el robot "ve distinto" cuando
   cambia la luz. Para competencia: lock.
3. **Tasa vs precisión:** fps vs resolución; ¿la velocidad de pelota necesita
   más fps que la detección de arco?
4. **Fusión multi-cámara:** ¿promedian (mal — pelota fantasma en el medio) o
   eligen una titular? ¿Marco común?
5. **Fail-open vs fail-closed** en el filtro de forma/color: ¿un frame raro mata
   la cámara o la deja pasar?

→ *Frase:* "Vemos [objeto] por [color LAB]; el punto frágil es [iluminación],
mitigado por [lock + recalibración en sede]."

## Comunicaciones

1. **El CONTRATO antes que el cable:** ¿hay un formato versionado byte-a-byte?
   ¿`static_assert(sizeof)`? Un cambio de contrato sin re-desplegar todo =
   cadena muerta silenciosa.
2. **Modo de falla = FAIL-SAFE:** cable suelto / placa muda → ¿el sistema cae a
   SEGURO (motores parados, STOP)? El árbitro por GPIO con pulldown lo logra:
   desconectado = 0 = STOP.
3. **Latencia y saltos:** dato urgente (borde de cancha) por el camino más corto
   (1 salto, <15 ms), no por el pipeline de fusión.
4. **Integridad:** CRC + resync. ¿Un byte corrupto envenena una decisión o se
   descarta el frame?
5. **Backpressure:** buffer lleno → ¿se roba CPU al lazo crítico? ¿Se dropea con
   gracia?

→ *Frase:* "Las placas hablan por [contrato de N bytes]; si el enlace cae, el
sistema va a [estado seguro]."

## Sensores (modelo de confianza)

1. **¿Cuánto le creo a cada sensor y cuándo?** Confianza condicional (la pose con
   conf<umbral no se usa; heading_valid gatea ω).
2. **Dependencia de calibración:** ¿qué sensor es inútil sin calibrar? (línea,
   cámara). Eso es deuda bloqueante, no "ajuste fino".
3. **Degradación silenciosa:** congelado-que-ACKea, deriva lenta. Peor que la
   muerte limpia.
4. **Redundancia real vs aparente:** 2 sensores en el MISMO bus que se cae juntos
   no son redundancia.

→ *Frase:* "Confío en [sensor] mientras [condición]; lo valido contra [otro]."

## Fail-safe / seguridad del sistema

1. **Para CADA componente: ¿qué pasa si muere a mitad de partido?** Caminar la
   lista, no asumir. Cámara, gyro, una placa, el árbitro, la batería.
2. **Default-to-safe:** el estado por ausencia de información debe ser SEGURO
   (sin snapshot → motores parados; sin árbitro → STOP).
3. **Prioridad del freno:** la seguridad (salir de cancha) bypassa la FSM. PERO
   ojo: un freno que eclipsa toda la lógica puede dejar al robot CLAVADO frenando
   (la red de seguridad que se vuelve la falla). Dejar pasar un tick de escape.
4. **¿El "freno" frena o suelta?** Brake activo (corto en H-bridge) vs coast.
   Medirlo: a 1 m/s son 15 mm en 15 ms.

→ *Frase:* "Si [componente] muere, el sistema [acción segura]; el único lugar
donde el fail-safe se vuelve riesgo es [freno-clavado]."

## Optimización de velocidad

1. **¿Qué pone el RITMO?** Identificar el recurso limitante: ¿el lazo es
   perception-limited (fps), compute-limited (loop time), power-limited
   (motores), o control-limited (cuantización)? Optimizar otra cosa no mueve la
   aguja.
2. **Medir antes de optimizar:** loop_us max/avg, Hz reales del snapshot. El
   caso del TOP a 6 Hz (no 100) por leer 4 ToF completos por tick → round-robin
   = 30× más rápido. La medición encontró el cuello, no la intuición.
3. **Velocidad vs estabilidad:** correr más rápido cambia el régimen de control
   (más velocidad puede SALIR del régimen cuantizado y habilitar PID continuo).
4. **Costo de la latencia en cadena:** un dato 250 ms viejo en un lazo de 100 Hz
   es peor que uno fresco a 50 Hz.

→ *Frase:* "El sistema está limitado por [recurso]; acelera al [quitar el cuello
medido], no al optimizar [lo no-limitante]."
