// mix_config.h — Constantes del programa AUTOCONTENIDO "centralmix".
//
// CONTRATO. Esto es un PORT del delantero 2025
// (software/_deprecated-2025/robot-delantero/delantero-sin-zirconLib.cpp), pero:
//   - alimentado por datos de TOP/DOWN (NO por la lectura serial cruda 2025),
//   - SIN usar world_model (variables planas estilo 2025, ver mix_io.h),
//   - con PRIMITIVAS DE MOTOR DIRECTAS (analogWrite/digitalWrite) como el 2025.
//
// Pines de motor = ZIRCON ACTUAL ROBOT1 (R1), tomados de
// src/central/config_central.h rama ROBOT1:
//   M1 = INA2 / INB5 / PWM3   (delantera IZQUIERDA, U5)
//   M2 = INA8 / INB7 / PWM6   (delantera DERECHA,   U17)
//   M3 = INA11 / INB12 / PWM4 (TRASERA,             U7)
//   MOTOR_INVERT = {+1, +1, +1}
//
// ⚠️ OJO MAPEO 2025↔2026: el delantero 2025 con #define ROBOT2 usaba OTROS pines
// (M1=8/7/6, M2=11/12/4, M3=2/5/3). ESTE archivo NO copia esos: usa los pines R1
// del Zircon 2026 (config_central.h), como pide la tarea. Los implementadores de
// mix_motors.cpp deben respetar ESTE pinout, no el del .cpp 2025.
//
// ⚠️ NO TESTEADO EN HARDWARE. Contrato de constantes — los valores numéricos son
// el port 1:1 del 2025; el sentido físico de cada primitiva se valida en banco.

#pragma once
#include <stdint.h>   // uint8_t (MIX_LINE_DEPTH_TRIGGER)

namespace iitasoccer {
namespace mix {

// ============================================================
// Pines de motor — R1 (Zircon actual). Espejo de config_central.h rama ROBOT1.
// ============================================================
//   M1 = delantera IZQUIERDA (U5)
constexpr int MIX_PIN_INA1 = 2;
constexpr int MIX_PIN_INB1 = 5;
constexpr int MIX_PIN_PWM1 = 3;
//   M2 = delantera DERECHA (U17)
constexpr int MIX_PIN_INA2 = 8;
constexpr int MIX_PIN_INB2 = 7;
constexpr int MIX_PIN_PWM2 = 6;
//   M3 = TRASERA (U7)
constexpr int MIX_PIN_INA3 = 11;
constexpr int MIX_PIN_INB3 = 12;
constexpr int MIX_PIN_PWM3 = 4;

// Sentido por motor (+1 normal, -1 invertido por HW). R1 banco 2026: {+1,+1,+1}.
// Índices: [0]=M1, [1]=M2, [2]=M3. mix_set_motor() debe aplicar este signo.
constexpr int MIX_MOTOR_INVERT[3] = { +1, +1, +1 };

constexpr int MIX_MAX_PWM = 255;  // rango de analogWrite

// ============================================================
// Umbrales de línea (sensores analógicos) — port del 2025.
// El 2025 leía 3 sensores analógicos (LINE_PIN1/2/3) y comparaba contra blanco1/2/3.
// En centralmix la LÍNEA llega por DOWN (mix_io: line_present / line_angle_deg /
// line_depth), NO por analogRead. Estos umbrales quedan como REFERENCIA del 2025 y
// por si se cablean sensores locales de respaldo. line_depth>=MIX_LINE_DEPTH_TRIGGER
// es el criterio sugerido para "tocando línea" cuando se usa el dato de DOWN.
// ============================================================
constexpr int MIX_LINE_BLANCO1 = 650;  // 2025 ROBOT2 blanco1
constexpr int MIX_LINE_BLANCO2 = 650;  // 2025 ROBOT2 blanco2
constexpr int MIX_LINE_BLANCO3 = 750;  // 2025 ROBOT2 blanco3
// Umbral sugerido sobre el dato de DOWN (line_depth = # sensores / profundidad).
constexpr uint8_t MIX_LINE_DEPTH_TRIGGER = 1;  // ≥1 sensor en blanco = línea presente

// ============================================================
// Constantes de velocidad 2025 (multiplicadores de PWM).
//   g  = girando
//   a  = apuntando pelota
//   c  = velocidad centrando
//   ic = velocidad impulso centrando
// (ROBOT2 2025: c=0.4, ic=0.55)
// ============================================================
constexpr float MIX_G  = 0.4f;   // girando
constexpr float MIX_A  = 0.35f;   // apuntando pelota
constexpr float MIX_C  = 0.4f;   // centrando (2025 ROBOT2)
constexpr float MIX_IC = 0.55f;  // impulso centrando (2025 ROBOT2)
constexpr float MIX_PD = 1.0f;   // avances proporcionales (2025 'pd')

// ============================================================
// Tolerancias 2025 (en mm para cercanía/centrado; en grados para apuntado).
// ============================================================
constexpr float MIX_TOL_CENTRADO = 5.0f;  // tolerancia_centrado
constexpr float MIX_TOL_CERCANIA = 30.0f;  // tolerancia_cercania
constexpr float MIX_TOL_APUNTADO = 20.0f;  // tolerancia_apuntado (grados)

// ============================================================
// Jugada "PELOTA ATRÁS" (la ve la cámara TRASERA) — giro-encare sobre el piso del motor.
// Diseñada con análisis de FSM + red-team multi-agente (workflow 2026-06-22).
//
// PROBLEMA que resuelve: cuando la cámara trasera ve la pelota, ésta queda DETRÁS del robot
// (ball_y_cm<0) y angulo_pelota_deg ≈ ±180. El apuntado fino 2025 (apuntar_pelota_motores) gira a
// 100*MIX_A = 35 PWM, que está POR DEBAJO del piso del motor ({70,70,107}) → el robot ZUMBA sin
// girar (zona muerta) y, encima, el ángulo SALTA +180↔-180 con el ruido lateral → el delantero
// queda CLAVADO ~10 s. (Riesgo de gol en contra: BAJO — CENTRANDO ya orbita y alinea al arco rival
// antes de patear; esto solo lo desclava rápido.)
//
// SOLUCIÓN: en APUNTAR_PELOTA, si la pelota está atrás, GIRAR EN EL LUGAR a MIX_ATRAS_PWM (sobre el
// piso) hasta que quede APUNTADA (|angulo|<MIX_TOL_APUNTADO). Claves de robustez (del red-team):
//   - Se ENTRA por ball_y_cm (señal MONÓTONA), NO por el ángulo (que salta ±180).
//   - El SENTIDO se LATCHEA una sola vez al entrar (no se re-lee cada tick) → no dithera en ±180.
//   - Se gira a MIX_ATRAS_PWM en TODO el arco 180→15 (no se entrega al apuntado de 35 a mitad de
//     camino, que volvería a la zona muerta).
//   - Es GIRO PURO en el lugar (las 3 ruedas mismo signo, como girar()) → NO traslada la pelota
//     hacia ningún arco mientras gira.
// ⚠️ SIGNO FÍSICO de +pwm SIN verificar en código (mix_fsm.cpp marca <RE-VERIFICAR SENTIDO EN BANCO>).
//   El peor caso de un signo mal NO es gol en contra (gira en el lugar): es encarar por el lado LARGO
//   (~340° en vez de ~20°). Si en banco encara por el lado largo, poné MIX_ATRAS_DIR_SIGN = -1.
// ============================================================
constexpr float MIX_ATRAS_Y_ENTRA  = 6.0f;  // cm: ENTRA al giro si ball_y_cm < -6 (pelota claramente
                                            //   atrás). Rango 4..10. KILL-SWITCH: poné 9999.0f →
                                            //   la jugada NUNCA dispara (FSM idéntica a hoy).
constexpr int   MIX_ATRAS_PWM      = 120;   // PWM del giro-encare. >= piso M3 (107) CON margen. Es la
                                            //   PERILLA principal de banco: subí si zumba/no gira,
                                            //   bajá si el regulador hace brownout. Rango 110..140.
constexpr int   MIX_ATRAS_DIR_SIGN = +1;    // sentido del giro (+1/-1). Si encara por el lado LARGO
                                            //   en banco, invertir a -1 (signo físico A CONFIRMAR).

// ============================================================
// Kicker / patada — RECTA y FUERTE con corrección de rumbo por OTOS (2026-06-21, pedido Elías).
// Revisado con análisis de cinemática + red-team multi-agente (workflow 2026-06-21).
//
// El "kicker" es empuje por inercia (NO hay solenoide): el robot acelera hacia adelante para
// empujar la pelota al arco. ANTES (port 2025) era LAZO ABIERTO: rampa lenta M1=+vel / M2=-vel /
// M3=0 hasta 240, SIN realimentación → si las 2 ruedas delanteras no están parejas, la patada
// CURVA. AHORA:
//   1) FUERTE y RÁPIDA: arranca por encima del piso del motor (MIX_KICK_VEL_START) y rampa
//      AGRESIVA hasta MIX_KICK_VEL_FINAL. La rampa NO desaparece (no slam 0→255) para no hacer
//      brownout del regulador con el pico de arranque.
//   2) DERECHA: heading-hold con el OTOS. Al iniciar la patada se ANCLA otos_heading_deg como
//      objetivo; durante el empuje se agrega un término de GIRO (la MISMA corr en las 3 ruedas
//      = el término ω·R de la cinemática, idéntico para las 3 porque están al mismo radio; es
//      EXACTAMENTE lo que hace girar()). Sumar la misma corr a las 3 NO es un bug: es giro puro,
//      sin traslación parásita. (Sumarla solo a 2 ruedas SÍ sería un bug: metería strafe.)
//
// SATURACIÓN — el arreglo clave (cinemática verificada contra src/shared/kinematics.cpp):
//   NO se deja que mix_set_motor recorte CADA rueda por separado. El clamp por-rueda hace que
//   una delantera sature a 255 y la otra no → el vector se TUERCE e inyecta STRAFE LATERAL
//   PARÁSITO (la patada sale de costado aunque el morro apunte bien). En su lugar, si el pico
//   pasa 255 se ESCALAN las 3 ruedas por el MISMO factor → la mezcla (empuje+giro) preserva su
//   dirección. Mismo patrón que saturate_wheels() (kinematics.cpp:27-39), ya validado en el repo.
//
// ⚠️ SIGNO DE LA CORRECCIÓN — este lazo es MIXER-FREE: NO pasa por inverse_kinematics(), así que
//    NO hereda el OMEGA_SIGN=-1 de config_central.h:175 (el fix del único caso documentado de
//    realimentación POSITIVA del repo, donde el rumbo se iba -3.7→-71.5). El signo correcto de
//    MIX_KICK_HEADING_KP hay que validarlo en banco POR SEPARADO del lazo de navegación: que la
//    navegación vaya derecha NO garantiza que esta patada también. Por eso el default arranca
//    CHICO (titular de menor a mayor, skill control-pid-zona-muerta).
//
// ⚠️ El OTOS (otos_heading_deg) es independiente de la fuente de heading de navegación (BNO del
//    TOP en el env _bno): la patada se corrige con el OTOS LOCAL de DOWN aunque el BNO del
//    snapshot esté en 0. Requiere que DOWN mande Pose2D con confidence>0 (OTOS sano).
//
// LÍMITE CONOCIDO: corrige ROTACIÓN (rumbo), NO deriva lateral (crab). Si el robot sale empujado
//   de costado pero apuntando bien, el heading-hold no lo ve. La corrección de eso es el
//   "cross-track" con x/y del OTOS (la generalización correcta de la idea per-OTOS de Elías) →
//   queda como mejora futura (necesita cablear otos_x/y_mm a g_io y medir la deriva XY del OTOS).
// ============================================================
constexpr int MIX_KICK_VEL_START    = 120;  // PWM inicial del empuje (arranca SOBRE el piso ~70,
                                            //   no desde 0 → bite inmediato, "rápida"). Rango 90..140.
constexpr int MIX_KICK_VEL_FINAL    = 240;  // PWM final del empuje (FUERTE). Rango 180..255. Con el
                                            //   escalado-vector ya NO hay que bajarlo para evitar
                                            //   saturación asimétrica; si la patada pierde fuerza al
                                            //   corregir, bajalo a ~200.
constexpr int MIX_KICK_PASO         = 25;   // incremento de PWM por paso (RÁPIDO). Rango 15..30.
constexpr int MIX_KICK_INTERVALO_MS = 10;   // ms entre incrementos → 120→240 en ~50 ms.

// --- Corrección de rumbo durante el empuje (heading-hold con el OTOS) ---
constexpr float MIX_KICK_HEADING_KP = -2.5f; // PWM de giro por GRADO de error. *** LA PERILLA DE
                                            //   BANCO ***. Arranca CHICO (seguridad, ver nota OMEGA_SIGN
                                            //   arriba). Pasos: (1) confirmar SIGNO con robot LEVANTADO
                                            //   y VEL_FINAL bajo; si la corrección ALEJA del rumbo →
                                            //   invertir a +2.5f. (2) Tras fijar signo, subir magnitud
                                            //   (hasta ~-10) hasta que corrija firme sin zigzag.
constexpr int MIX_KICK_CORR_MAX     = 30;   // tope (PWM) del término de giro. Arranca CHICO; tras
                                            //   confirmar signo subir (hasta ~90). Con el escalado,
                                            //   subirlo NO genera asimetría (las 3 escalan juntas).
constexpr unsigned long MIX_KICK_OTOS_FRESH_MS = 300;  // si la pose OTOS está más vieja que esto
                                                       // (o confidence==0) → NO corregir (recto a ciegas)

// Piso de la rueda TRASERA (M3) durante la corrección. M3 SOLO hace giro (corr); por debajo de su
// piso físico (~107, config_central.h MOTOR_MIN_PWM rear) NO gira (zumba) → el giro sale solo del
// diferencial M1/M2 y reaparece algo de strafe parásito. DEFAULT 0 = APAGADO (a ganancias chicas
// elevar M3 lo vuelve bang-bang → zigzag). Si en banco el giro es flojo, probar 107 (eleva M3 a su
// piso conservando signo, DESPUÉS del escalado). Medir si la trasera gira durante la patada.
constexpr int MIX_KICK_REAR_FLOOR   = 0;    // 0 = off; típico ON = 107

// FEEDFORWARD de balance del empuje — DEFAULT 0 (probar PRIMERO sin trim). El veer "solo al patear"
// (banco 2026-06-22) NO es un motor flojo: es la SATURACIÓN a PWM alto (el clamp por-rueda torcía el
// vector; el ESCALADO de avanzar_patear ya lo arregla — en avanzar() a PWM 100 no saturaba, por eso
// NO derivaba ahí). Este trim queda SOLO para un sesgo CONSTANTE residual de hardware tras el escalado:
// trasvasa PWM de la delantera IZQ (M1) a la DER (M2).
//   + (positivo) → menos a M1, MÁS a M2 → corrige deriva residual a la DERECHA.
//   - (negativo) → al revés, corrige deriva a la IZQUIERDA.
// Subirlo SOLO si tras el escalado todavía se va siempre para un lado. El heading-hold del OTOS queda
// ENCIMA para el sesgo variable.
constexpr int MIX_KICK_FWD_TRIM     = 40;    // PWM trasvasado M1→M2 (0 = sin trim). Subir solo si residual.

// Retroceso de patada (PWM crudo por motor) — port 1:1 del 2025 (freno/recoil tras el empuje).
constexpr int MIX_PATAD_M1 = 250;  // patadM1
constexpr int MIX_PATAD_M2 = 170;  // patadM2

// ============================================================
// Candidato de ÓRBITA 2026 para centrar_*  (detrás de flag, APAGADO por defecto).
//
// PROBLEMA: el centrar_*() actual (port 2025) decodifica a CASI PURO STRAFE en la
// geometría de ruedas 2026 (vx≈±64, ω·R≈±8) → orbita mal: la pelota se escapa de
// costado. Verificado por análisis de cinemática (decode contra
// src/shared/kinematics.cpp; workflow 2026-06-19, 6 agentes, 0 refutaciones fatales).
//
// CANDIDATO: rebalancea a rotación-dominante MANTENIENDO la misma dirección de
// órbita (horario = strafe-IZQ + giro CW) y supera los pisos físicos {70,70,107}.
// Valores DIRECTOS de PWM por rueda (centralmix es mixer-free): delanteras (M1,M2)
// = MIX_CENTRAR_FRONT, trasera (M3) = MIX_CENTRAR_REAR (signo opuesto a las delanteras).
// Default [80,80,170] → vx≈-60 (strafe IZQ), ω·R≈+110 (CW), ratio ω/strafe≈1.83.
//
// ⚠️ TRADE-OFF DE 3 VÍAS (no hay óptimo libre): piso delantero (≥70) vs ratio de
// órbita ideal (≈R/d≈1.0) vs TECHO TÉRMICO (~150 PWM sostenido; motores brushed
// 5V@7.4V se queman >~70%). Una órbita geométricamente perfecta (ratio≈1.0) forzaría
// la trasera a ≥200 (quema). Por eso el default sobre-rota un poco (la corrección
// APUNTAR_PELOTA del FSM recoge la deriva residual). Trasera=170 ya está algo sobre
// el techo → usar órbitas CORTAS y vigilar temperatura.
//
// BENCH-TUNING (Elías): MEDIR d (distancia centro-pelota) y ajustar SOLO estas 2:
//   - pelota deriva hacia ADENTRO del giro → sobra rotación → BAJAR MIX_CENTRAR_REAR.
//   - pelota se va de COSTADO (afuera)     → falta rotación → SUBIR MIX_CENTRAR_REAR.
//   - si la trasera calienta → bajar REAR hacia 150 (acepta más sobre-rotación).
//
// ⚠️ NO TESTEADO EN HARDWARE. El sentido físico de R1 (izq/der, CW/CCW) se CONFIRMA en
// banco (config_central.h marca el +180 de traslación R1 "A VERIFICAR"). Si orbita al
// revés: intercambiar etiquetas horario↔antihorario en el FSM, NO tocar los signos acá.
// Se habilita con -DMIX_CENTRAR_ORBIT_2026 en build_flags (ver platformio.ini).
// ============================================================
constexpr int MIX_CENTRAR_FRONT = 85;   // M1,M2 (delanteras) — ≥70 (piso físico)
constexpr int MIX_CENTRAR_REAR  = 150;  // M3 (trasera) — ≥107 (piso); ojo techo térmico ~150

// ============================================================
// CAMINO CORTO al centrar (2026-06-22, pedido Elías): al empezar a orbitar la pelota, ELEGIR el
// sentido (horario/antihorario) según de qué lado está el ARCO RIVAL (goal_opp_angle), en vez de
// arrancar SIEMPRE para el mismo lado. Así no le da toda la vuelta a la pelota cuando el arco está
// del otro lado. Si el arco NO se ve, usa un sentido por defecto.
// Regla: si goal_opp_angle·MIX_CENTRAR_SHORT_SIGN >= 0 → CENTRANDO_horario; si no → antihorario.
// ⚠️ SIGNO FÍSICO A CONFIRMAR EN BANCO: si igual te da la vuelta LARGA (orbita para el lado
//   contrario al arco), invertí MIX_CENTRAR_SHORT_SIGN a -1. (KILL-SWITCH: poné 0 → vuelve al
//   comportamiento viejo, siempre el mismo sentido por defecto.)
// ============================================================
constexpr int MIX_CENTRAR_SHORT_SIGN = +1;  // +1 / -1 (camino corto por el lado del arco); 0 = apagado
// (El respaldo por HEADING cuando NO se ve el arco lo agrega Elías a mano — se sacó la VÍA 2.)

// ============================================================
// KICKOFF / arranque — PRIMER estado del FSM (coach 2026-06-21, SIN flag: KICKOFF_SEEK es
// el arranque de TODAS las builds del centralmix). Pedido de Elías: al arrancar, si VE
// la pelota va hacia ella; si NO la ve, da un IMPULSO FUERTE y CORTO de MEDIALUNA (arco)
// para despegarse hacia el centro, y después cae a la búsqueda por giro (GIRANDO).
//
// La medialuna se arma por SUPERPOSICIÓN de las 2 bases que YA existen (en un omni las
// velocidades de rueda SUMAN linealmente, así que esto SÍ da un arco, no el casi-strafe
// del centrar_*):
//   AVANCE (base avanzar(): M1=+F, M2=-F, M3=0)  +  GIRO (base girar(): M1=M2=M3=+T)
//   ⇒  M1 = +F + dir·T ,  M2 = -F + dir·T ,  M3 = dir·T     (ver kickoff_medialuna()).
// F = cuánto AVANZA · T = cuánta CURVA · dir = lado (±1). FUERTE = F alto; CORTO = ARC_MS
// chico → térmicamente seguro aunque el PWM sea alto, porque dura poco.
//
// ⚠️ NO TESTEADO EN HARDWARE. En banco se confirma: (1) que curva para el lado correcto
// (si no, invertir MIX_KICKOFF_ARC_DIR); (2) F/T/duración; (3) que "hacia el centro" tenga
// sentido para tu lado de saque (sin pose absoluta el lado es FIJO → setearlo acá).
// ============================================================
constexpr int MIX_KICKOFF_ARC_PWD  = 2;  // F: componente de AVANCE (fuerte; > piso ~70)
constexpr int MIX_KICKOFF_ARC_CURV = 2.0;   // T: componente de GIRO/curvatura de la medialuna
constexpr int MIX_KICKOFF_ARC_DIR  = +1;   // lado de la curva (+1 / -1) — confirmar en banco
constexpr int MIX_KICKOFF_ARC_MS   = 500;  // duración del impulso (CORTO), en ms

// ============================================================
// Heading — control de rumbo del 2025 (error = currentYaw - initialYaw, kp=0.3).
// SELECTOR de fuente de heading (TRES modos, mutuamente excluyentes):
//   - DEFAULT (sin flag): BNO055 LOCAL en la CENTRAL (Wire @ 0x28), como el 2025.
//       ⚠️ R1 2026 NO tiene BNO local (los 2 BNO viven en el TOP) → este modo NO sirve
//       para R1: el read local falla y deja heading_valid=false. Sólo úsalo en una placa
//       que REALMENTE tenga un BNO en su propio bus.
//   - Con -DMIX_HEADING_SNAPSHOT: heading = BNO del TOP que llega por el WorldSnapshot
//       (Serial7), SIN tocar ningún BNO local. ESTE es el modo para R1 2026 (BNO del TOP
//       arreglado + heading validado en banco 2026-06-21; ver src/top/sensors_imu.cpp:279).
//   - Con -DMIX_HEADING_OTOS: heading = OTOS (otos_heading_deg de DOWN). Fallback SIN gyro
//       (era para cuando el BNO de R1 estaba "muerto"; hoy el BNO anda → preferí SNAPSHOT).
// mix_comm puebla mix_io.heading_deg desde la fuente elegida y deja SIEMPRE
// otos_heading_deg crudo disponible para diagnóstico/A-B.
// (SNAPSHOT y OTOS son excluyentes; mix_comm.cpp lo verifica con un #error.)
// ============================================================
constexpr float MIX_HEADING_KP = 0.3f;  // kp del control de rumbo 2025

#if defined(MIX_HEADING_OTOS)
constexpr bool MIX_HEADING_SOURCE_IS_OTOS     = true;
constexpr bool MIX_HEADING_SOURCE_IS_SNAPSHOT = false;
#elif defined(MIX_HEADING_SNAPSHOT)
constexpr bool MIX_HEADING_SOURCE_IS_OTOS     = false;
constexpr bool MIX_HEADING_SOURCE_IS_SNAPSHOT = true;   // BNO del TOP vía snapshot
#else
constexpr bool MIX_HEADING_SOURCE_IS_OTOS     = false;
constexpr bool MIX_HEADING_SOURCE_IS_SNAPSHOT = false;  // DEFAULT: BNO LOCAL (Wire@0x28)
#endif

// BNO055 LOCAL (solo relevante en el modo por defecto = BNO local en la CENTRAL).
constexpr int MIX_BNO055_I2C_ADDR = 0x28;

// ============================================================
// Comunicación — baud de los enlaces TOP/DOWN (referencia comm_top.cpp /
// comm_down.cpp: ambos a 230400). El 2025 usaba 19200 sobre Serial1 crudo; ese
// valor queda SOLO como nota histórica, NO se usa: centralmix consume frames
// proto.h de TOP/DOWN a 230400.
// ============================================================
constexpr long MIX_UART_BAUD = 230400;  // TOP (Serial7) y DOWN (Serial1)

// ============================================================
// RODEO estilo "Edge" (回り込み) — delantero REACTIVO (coach + Elías, 2026-06-23).
// Solo se usa con -DMIX_ATTACK_EDGE (FSM mix_fsm_edge.cpp). Sin el flag, centralmix
// corre EXACTAMENTE como hoy (FSM 2025 en mix_fsm.cpp): esto es 100% aditivo.
//
// QUÉ ES: el delantero campeón mundial 2024 Team Edge se pone detrás de la pelota con UNA
// sola fórmula reactiva (no apuntar→avanzar→orbitar en estados), a full velocidad. Acá se
// porta esa curva (mix_edge.cpp) + un giro que mira al arco, sobre la primitiva holonómica
// mix_mover_vector. Empuje al arco POR INERCIA (sin pateador), reusando avanzar_patear.
//
// ⚠️ NADA de esto está testeado en hardware. Toda PERILLA se titula en banco. Las unidades
// de distancia/ángulo son las de mix_io (cm CRUDO de cámara, grados marco robot).
// ============================================================

// --- Curva de rodeo: |ángulo de pelota| → ángulo de avance (mix_edge_wrap_angle). ---
// Piecewise lineal CONTINUA en 3 tramos. Subir una pendiente = rodear MÁS agresivo en esa
// zona (apuntar más al costado de la pelota). Defaults = espíritu de Edge (1.2 / 2.0 / 1.0).
constexpr float MIX_EDGE_K_NEAR    = 1.2f;    // pendiente zona cercana (pelota casi al frente)
constexpr float MIX_EDGE_B1_DEG    = 20.0f;   // fin zona cercana (°)
constexpr float MIX_EDGE_K_SIDE    = 2.0f;    // pendiente zona lateral (rodeo fuerte: ×2 el ángulo)
constexpr float MIX_EDGE_B2_DEG    = 75.0f;   // fin zona lateral (°)
constexpr float MIX_EDGE_K_WIDE    = 1.0f;    // pendiente zona ancha (pelota muy al costado/atrás)
constexpr float MIX_EDGE_GO_MAX_DEG = 170.0f; // tope del ángulo de avance (no apuntar 100% atrás)

// --- Velocidad de rodeo (PWM de traslación). Edge va a 220-240; arrancá MEDIO y subí. ---
constexpr int   MIX_EDGE_SPEED     = 200;     // PWM de traslación durante el rodeo. Rango 150..240.

// --- Giro: orientar el FRENTE al arco rival MIENTRAS rodea (clave SIN pateador: hay que ---
//     llegar detrás de la pelota ya mirando al arco para empujar derecho). ---
constexpr float MIX_EDGE_FACE_KP   = 1.5f;    // PWM de giro por GRADO de error al arco rival.
                                              //   *** PERILLA DE BANCO (signo Y magnitud) ***.
                                              //   Paso 1: confirmar SIGNO con robot levantado (si
                                              //   gira ALEJÁNDOSE del arco → invertir a -1.5). Paso
                                              //   2: subir magnitud hasta que encare firme sin zigzag.
constexpr int   MIX_EDGE_OMEGA_MAX = 70;      // tope del PWM de giro (no dominar a la traslación).

// --- Disparo del EMPUJE (gol por inercia, sin pateador). El robot se compromete a empujar
//     recto cuando la pelota está CERCA + al FRENTE + (arco alineado o no visible). ---
constexpr float MIX_EDGE_PUSH_DIST_CM   = 14.0f; // pelota más cerca que esto = empujar. RE-TUNEAR
                                                 //   (cm CRUDO de cámara; medir el valor real cerca).
constexpr float MIX_EDGE_PUSH_ALIGN_DEG = 25.0f; // y dentro de este ángulo al frente.
constexpr float MIX_EDGE_PUSH_GOAL_DEG  = 25.0f; // si se VE el arco rival, alineado dentro de esto.
constexpr unsigned long MIX_EDGE_PUSH_MS = 500;  // duración del empuje a fondo (= PATEANDO_adelante 2025).
constexpr unsigned long MIX_EDGE_BACK_MS = 200;  // retroceso corto post-empuje (= PATEANDO_atras 2025).

// --- VERSIÓN POSICIÓN PURA (esta carpeta, centraledgefijo): NO hay feedforward de velocidad. ---
//     El rodeo usa SOLO la curva de arriba sobre la posición ACTUAL de la pelota, como el rodeo
//     SIMPLE de Edge (pedido de Elías). Si querés anticipar la pelota en movimiento, usá la carpeta
//     `centraledge` (esa SÍ trae el feedforward con la velocidad). Acá NO se lee ball_vx/vy.

// --- Pérdida de pelota durante el rodeo → volver a buscar. ---
constexpr unsigned long MIX_EDGE_BALL_LOST_MS = 500;  // sin ver pelota más que esto → BUSCAR.

}  // namespace mix
}  // namespace iitasoccer
