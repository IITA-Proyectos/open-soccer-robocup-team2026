// amix_config.h — Constantes del programa AUTOCONTENIDO "arqueromix".
//
// CONTRATO. Es el HERMANO ARQUERO de centralmix: un PORT del ARQUERO 2025
// (software/_deprecated-2025/robot-arquero/definitivo-arquero_6-9-2026, rama
// #define ROBOT1 = ARQUERO), con el mismo enfoque que centralmix (delantero):
//   - FSM 2025 portada fiel + PRIMITIVAS DE MOTOR DIRECTAS (analogWrite/digitalWrite),
//   - alimentado por TOP/DOWN (NO por los sensores locales del 2025),
//   - SIN world_model (variables planas estilo 2025, ver amix_io.h).
//
// QUÉ REEMPLAZA (pedido de Virginia, igual que el delantero del viernes):
//   - BNO local del 2025        → HEADING del SNAPSHOT del TOP (por serie).
//   - cámara local v1 del 2025  → pelota/arcos del SNAPSHOT del TOP (por serie).
//   - 3 sensores de luz locales → LÍNEA del DOWN (LineStatusV2, por serie).
//
// PINES de motor = ZIRCON ACTUAL (R1 y R2 comparten layout en 2026):
//   M1 = INA2 / INB5 / PWM3   (delantera IZQUIERDA, U5)
//   M2 = INA8 / INB7 / PWM6   (delantera DERECHA,   U17)
//   M3 = INA11 / INB12 / PWM4 (TRASERA,             U7)
//   MOTOR_INVERT = {+1, +1, +1}  (banco 2026, ambos robots)
//
// ⚠️ OJO MAPEO 2025↔2026: el arquero 2025 era #define ROBOT1 con pines
// M1=2/5/3, M2=8/7/6, M3=11/12/4 (FIEL §3.1). Coincide con el layout 2026 → se usan
// esos. Pero el SENTIDO FÍSICO de cada primitiva se RE-VERIFICA en banco (compila != anda).
//
// ⚠️ NO TESTEADO EN HARDWARE. Los valores numéricos son el port 1:1 del 2025
// (docs/internal/ANALISIS-FIEL-ARQUERO-2025.md); el sentido y los umbrales en mm se
// validan/re-tunean en banco (TASK del equipo).

#pragma once
#include <stdint.h>

namespace iitasoccer {
namespace arqmix {

// ============================================================
// Pines de motor — Zircon 2026 (espejo de config_central.h + centralmix/mix_config.h).
// ============================================================
constexpr int AMIX_PIN_INA1 = 2;   // M1 delantera IZQUIERDA (U5)
constexpr int AMIX_PIN_INB1 = 5;
constexpr int AMIX_PIN_PWM1 = 3;
constexpr int AMIX_PIN_INA2 = 8;   // M2 delantera DERECHA (U17)
constexpr int AMIX_PIN_INB2 = 7;
constexpr int AMIX_PIN_PWM2 = 6;
constexpr int AMIX_PIN_INA3 = 11;  // M3 TRASERA (U7)
constexpr int AMIX_PIN_INB3 = 12;
constexpr int AMIX_PIN_PWM3 = 4;

// Sentido por motor ({+1,+1,+1} banco 2026). [0]=M1, [1]=M2, [2]=M3.
constexpr int AMIX_MOTOR_INVERT[3] = { +1, +1, +1 };
constexpr int AMIX_MAX_PWM = 255;

// ============================================================
// PATRULLA — adproporcional() / aiproporcional() (FIEL §5, código 2025 L186-233).
// Strafe lateral con CORRECCIÓN DE RUMBO en 3 bandas según el signo de `error`
// (error = heading - heading_inicial). Las delanteras (M1,M2) van al MISMO sentido;
// la trasera (M3) al opuesto, y su magnitud cambia por banda para enderezar.
// Valores 2025 (multiplicados por `pd` en runtime). Los comentarios //NN del 2025 eran
// valores ANTIGUOS (no activos) — acá van los ACTIVOS.
// ============================================================
//  Banda CENTRADA (-1 < error < 1):
constexpr int AMIX_PROP_FRONT_CENTER = 50;   // M1 y M2
constexpr int AMIX_PROP_REAR_CENTER  = 89;   // M3
//  Banda error > 0:
constexpr int AMIX_PROP_FRONT_EPOS   = 50;   // M1 y M2 (ambas funciones)
constexpr int AMIX_AD_REAR_EPOS      = 100;  // adproporcional (derecha) M3
constexpr int AMIX_AI_REAR_EPOS      = 40;   // aiproporcional  (izquierda) M3
//  Banda error < 0 (M1 y M2 difieren entre sí):
constexpr int AMIX_PROP_M1_ENEG      = 40;   // "motor derecho" (idx0=M1) 2025
constexpr int AMIX_PROP_M2_ENEG      = 65;   // "motor izquierdo" (idx1=M2) 2025
constexpr int AMIX_AD_REAR_ENEG      = 40;   // adproporcional M3
constexpr int AMIX_AI_REAR_ENEG      = 75;   // aiproporcional M3 — REVERTIDO a 75 (banco Virginia 2026-06-21: la versión 65 + sesgo forward hacía MEDIALUNA a la izquierda → se volvió al estado anterior que andaba mejor). 100→75. (1.875× la derecha=40.)

// `pd` (factor proporcional): patrulla base, ×1.5 con pelota desviada.
// BAJADO 1.0→0.85 (banco Virginia 2026-06-21): patrulla más LENTA (-15%) sin caer en zona muerta (el
// patrón de PWM se escala parejo). ⚠️ NO bajar de 0.80 o las delanteras (piso ~70) stallean → si a
// 0.85 se traba/espasmódico, SUBIR a 0.90 (no bajar).
constexpr float AMIX_PD_BASE = 0.85f;
constexpr float AMIX_PD_BALL = 1.5f;

// SESGO HACIA ADELANTE de la patrulla (pedido Virginia 2026-06-21: el arquero deriva hacia ATRÁS y se
// mete al área/corner; quiere "tendencia a avanzar en la cancha"). Es un micro-empuje RECTO al frente
// sumado al strafe: en la geometría omni-3 (M1 del-IZQ, M2 del-DER, M3 trasera), avanzar = M1=+, M2=-,
// así que el sesgo SUMA al M1 y RESTA al M2 con signos FIJOS (no sigue el signo del strafe). Lo aplican
// ad/aiproporcional → vale en patrulla y en el rebote (ayuda a no quedar atrás). 0 = off.
// ⚠️ A VALIDAR EN BANCO: subir si sigue derivando atrás; bajar si se va para adelante / se sale del arco.
// -DARQMIX_NO_FORWARD_BIAS lo apaga.
// ⚠️ REVERTIDO A OFF por default (banco Virginia 2026-06-21): con el sesgo en 10 el arquero hacía una
// MEDIALUNA a la izquierda (combinado con el rumbo sin corregir bien). Queda como OPT-IN: prender con
// -DARQMIX_FORWARD_BIAS si se quiere experimentar la "tendencia a avanzar".
#ifdef ARQMIX_FORWARD_BIAS
constexpr int AMIX_FORWARD_BIAS_PWM = 10;   // ~3-4% de empuje neto al frente (experimental)
#else
constexpr int AMIX_FORWARD_BIAS_PWM = 0;    // DEFAULT OFF (= comportamiento "de antes", sin medialuna)
#endif

// ============================================================
// Impulso inicial / anti-traba del borde (FIEL §2, L1018-1020 + impulso_*).
// ============================================================
// impulso_inicial (arquero, estado inicial): strafe corto 40 ms para despegarse.
//   2025: M1=M2=90, M3=153. BAJADO 2026-06-21 (banco Virginia): el arranque daba un tirón
//   fuerte; 153 rozaba el techo térmico. Más suave para que el inicio sea prolijo y verificable.
constexpr int AMIX_IMP_INI_FRONT = 70;   // M1, M2 (era 90)
constexpr int AMIX_IMP_INI_REAR  = 110;  // M3 (era 153)

// ============================================================
// Avance / despeje (FIEL §2/§5).
// ============================================================
// avanzar(): M1=100, M2=100(sentido opuesto), M3=0. (L152-154)
constexpr int AMIX_AVANZAR = 100;
// avanzar_patear() (arquero): RAMPA DE ACELERACIÓN como el DELANTERO (centralmix), pedido Virginia
// 2026-06-21 (el despeje fijo asimétrico "no apuntaba bien" → veraba). Sube la velocidad de a pasos
// desde 0 hasta VEL_FINAL → el golpe arranca suave y sale parejo. Patrón SIMÉTRICO M1=+vel, M2=-vel,
// M3=0 = avance RECTO al frente (a diferencia del 250/150 asimétrico del 2025 que torcía).
// Misma receta que el delantero (mix_config.h KICK_*). Potencia moderada (pedido previo de bajarla).
constexpr int AMIX_KICK_VEL_FINAL    = 180;  // velocidad PICO del golpe — subir si no llega a despejar
constexpr int AMIX_KICK_PASO         = 20;   // incremento de PWM por escalón de la rampa
constexpr int AMIX_KICK_INTERVALO_MS = 10;   // ms entre escalones (rampa 0→180 en ~90 ms)
// PATEANDO_atras_arquero (inline 2025, L1186-1188): retroceso recto M1=ATRAS, M2=ATRAS, M3=0.
constexpr int AMIX_ATRAS = 120;      // retroceso del despeje (PATRULLA) — NO tocar (queda como andaba)
// Retroceso del modo QUIETO — MÁS LENTO que la patrulla (banco Virginia 2026-06-21: a 120 el arquero se
// METÍA en el área chica porque cruzaba la línea por inercia + latencia antes de frenar). Más lento = para
// más JUSTO en la línea sin cruzarla. Separado de AMIX_ATRAS para NO tocar el retroceso de la patrulla.
// ⚠️ A 80 está sobre el piso de las delanteras (70) → se mueve. Si AÚN se mete → bajar a 75; si NO arranca
// (tironea) → subir. <TITRAR EN BANCO>
constexpr int AMIX_ATRAS_QUIETO = 80;
// FRENO DE PATADA (SOLO modo quieto, banco Virginia 2026-06-22): si detecta LÍNEA mientras patea, parar() NO
// alcanza —la INERCIA del golpe lo saca de la cancha—. Da un contra-empuje FUERTE hacia atrás (plugging) por
// un tiempo CORTO para MATAR el impulso y despegarse de la línea, y recién después sigue la secuencia
// post-patada (pausa → orientar → retroceder). Fuerte y corto a propósito.
constexpr int           AMIX_FRENO_PATADA_PWM = 200;  // PWM del freno-retroceso (alto = frena rápido). Subir si igual se sale; bajar si rebota mucho atrás.
constexpr unsigned long AMIX_T_FRENO_PATADA   = 250;  // ms del freno (corto, estilo plugging 2025). Subir si no frena a tiempo; bajar si se va de atrás.

// ============================================================
// ALINEAR AL ARCO RIVAL antes de despejar (pedido Gustavo 2026-06-21).
// ----------------------------------------------------------------------------------
// Antes de patear, el arquero GIRA en el lugar para apuntar su FRENTE al ARCO RIVAL (goal_opp) y
// despejar HACIA AHÍ (no sólo "lejos / recto"). Quién es el arco rival lo resolvió la placa TOP
// (goal_polarity, por ángulo); el arquero NO pregunta el COLOR. Si NO ve el arco rival, patea
// RECTO al frente (comportamiento del arquero 2025 = fallback). Acotado por tolerancia + timeout
// para no demorar el despeje ni sacar al arquero de su arco.
// ============================================================
constexpr float AMIX_TOL_ARCO_OPP_DEG = 12.0f;  // |áng al arco rival| <= esto → ALINEADO → patea.
                                                // Más amplio = patea antes pero menos preciso. <RE-TUNE>
constexpr int   AMIX_GIRO_ALINEAR_PWM = 90;     // PWM del giro de alineación (suave/controlado). <RE-TUNE>
constexpr unsigned long AMIX_T_ALINEAR_OPP = 300; // tope girando: si no logra alinear, patea igual (no
                                                  // demora el despeje). ARRANCA CONSERVADOR (300 ms) para NO
                                                  // sacar al arquero de su arco ni perder la pelota durante el
                                                  // giro; SUBIR en banco si no llega a apuntar. <tunable>
// SENTIDO del giro: ang>0 (arco a la derecha) debe girar para traer el arco al frente. El sentido
// físico de girar() depende del cableado 2026 → se RE-VERIFICA en banco. -DARQMIX_FLIP_GIRO_ALINEAR
// lo invierte si el arquero gira para el lado CONTRARIO al arco.
#ifdef ARQMIX_FLIP_GIRO_ALINEAR
constexpr int AMIX_GIRO_ALINEAR_SIGN = -1;
#else
constexpr int AMIX_GIRO_ALINEAR_SIGN = +1;
#endif

// ============================================================
// Tolerancias de la pelota — POR ÁNGULO (como centralmix usa la cámara). FIX 2026-06-21.
// ----------------------------------------------------------------------------------
// PROBLEMA que arregla (banco Virginia): con umbrales en mm (CENTRADO=30/DESVIO=50/
// CERCANIA=140) la pelota caía en la BANDA MUERTA para la escala REAL del snapshot
// (CAMERA_UNIT_TO_MM=10 sin calibrar) → el FSM hacía parar() → la cámara veía la pelota
// pero el robot NO se movía (freeze).
// FIX: el arquero SIGUE la pelota por su ÁNGULO (g_aio.angulo_pelota_deg = atan2(x,y)),
// que NO depende de la escala — igual que el delantero (centralmix). La banda muerta
// angular es ANGOSTA → el arquero trackea cualquier pelota off-center.
//   angulo_pelota_deg: 0=frente, >0=derecha, <0=izquierda.
// ============================================================
constexpr float AMIX_TOL_CENTRADO_DEG = 8.0f;   // |áng| <= esto → ALINEADO (mantiene posición).
                                                // |áng| > esto  → SIGUE la pelota (strafe a su lado).
constexpr float AMIX_TOL_KICK_DEG     = 30.0f;  // para DESPEJAR: la pelota debe estar dentro de
                                                // este ángulo (más amplio que CENTRADO) Y cerca.
constexpr float AMIX_TOL_CERCANIA_MM  = 250.0f; // DESPEJA si la distancia euclídea a la pelota
                                                // (sqrt(x²+y²)) <= esto. ⚠️ EL knob de tuning
                                                // principal (la escala del snapshot está sin
                                                // calibrar): subir si nunca despeja, bajar si
                                                // despeja de lejos. <RE-TUNE EN BANCO>

// ============================================================
// Tiempos / timeouts del ARQUERO (FIEL §3.5) — ms. Iguales 2025.
// ============================================================
// MOVIMIENTO LATERAL INICIAL (SOLO modo quieto, pedido Virginia 2026-06-21): al GO, ANTES del homing,
// el arquero se mueve un poco a la IZQUIERDA durante este tiempo y recién después va hacia atrás.
constexpr unsigned long AMIX_T_INICIO_LATERAL = 1600;  // cuánto se mueve a la izquierda al arrancar (a ciegas). "un poco".
// AVANCE al BUSCAR la pelota y tocar LÍNEA lateral (SOLO modo quieto, banco Virginia 2026-06-22): en vez de
// seguir lateral (se metía al área), AVANZA al frente. Para despegarse BIEN del borde, sigue avanzando ESTE
// tiempo DESPUÉS de dejar de ver la línea (avance "un poco más grande"). Subir si queda pegado al borde.
constexpr unsigned long AMIX_T_BUSCAR_AVANCE = 400;

// ACOMODARSE antes de quedar quieto (2 estados nuevos, pedido Virginia 2026-06-22): (1) acomodar_linea =
// si TOCA línea (lateral o atrás), despegarse "un poco" hacia el lado OPUESTO; (2) acomodar_orientar =
// re-orientar al FRENTE con el BNO (heading→0, mismo bang-bang que orientar_frente). Así, cuando queda
// quieto, NUNCA está sobre la línea y SIEMPRE mira al frente. Van tras el homing Y tras el retroceso.
constexpr float         AMIX_ACOMODAR_ATRAS_DEG      = 90.0f;  // |line_angle| >= esto → línea ATRÁS → AVANZAR; si no → strafe lateral OPUESTO
constexpr unsigned long AMIX_T_ACOMODAR_LINEA_SAFETY = 1500;   // tope despegándose de la línea (no quedarse atascado)
// SIGNO del lado de la línea: la convención de line_angle del DOWN NO está validada en banco. Si en
// acomodar_linea el arquero despega para el lado EQUIVOCADO → invertir con -DARQMIX_FLIP_ACOMODAR_LINEA.
#ifdef ARQMIX_FLIP_ACOMODAR_LINEA
constexpr float AMIX_ACOMODAR_LINEA_SIGN = -1.0f;
#else
constexpr float AMIX_ACOMODAR_LINEA_SIGN = +1.0f;
#endif
// ORIENTAR AL ARCO RIVAL tras el despeje (SOLO modo quieto). FASE 1 (pedido Virginia 2026-06-21): se
// orienta por GIROSCOPIO (heading_error_deg → 0), NO por cámara (goal_opp se ensucia al girar y llega a
// ~4 Hz). Control BANG-BANG con BANDA ANCHA (skill control-pid-zona-muerta): gira al piso
// (AMIX_GIRO_FRENTE_PWM) o para — nunca pide una corrección por debajo del piso (esa era la zona muerta que
// hacía serpentear). El cero del heading = "mirando al rumbo de colocación" = al arco rival (MISMA
// referencia que el strafe lateral que YA anda). Gateado por heading_valid: sin heading bueno NO gira a ciegas.
constexpr unsigned long AMIX_T_ORIENTAR_SAFETY = 3000;  // tope girando: si no logra orientarse, sigue igual
// BANDA MUERTA de orientación: |heading_error| <= esto → "mirando al oponente", para. ANCHA a propósito:
// a ~4-6 Hz (latencia del heading del TOP) girando al piso el robot rota varios grados por ciclo; una banda
// fina (±2°) haría overshoot y serpenteo. ±8° = misma tolerancia que el resto del arquero
// (AMIX_TOL_CENTRADO_DEG). SUBIR si serpentea alrededor del cero; bajar si queda muy torcido. <TITRAR>
constexpr float AMIX_TOL_ORIENTAR_DEG = 8.0f;
// PWM del giro de orientación (FASE 1: BANG-BANG por giroscopio). Gira a ESTE PWM fijo hacia el rumbo
// objetivo mientras |heading_error| > AMIX_TOL_ORIENTAR_DEG, y para dentro de la banda. Es "continuo y
// lento" (gira al piso del motor) SIN caer en zona muerta (bang-bang, no proporcional fino: o gira al piso
// o para).
// BANCO Virginia 2026-06-21: a 70 giraba continuo pero MUY rápido; bajado a 50 (la velocidad "tal vez está
// bien"). Si TIRONEA (stick-slip = por debajo del piso) → subir de a 5; si gira muy rápido → no se puede más
// lento en continuo (es el piso del motor en rotación). <TITRAR EN BANCO>
constexpr int AMIX_GIRO_FRENTE_PWM = 50;  // PWM fijo del giro de orientación (bang-bang). Si tironea, subir; no baja del piso.
// (OBSOLETAS: el giro ya NO es pulsado. Se conservan por si se decide volver al esquema PFM.)
constexpr unsigned long AMIX_T_GIRO_VENTANA = 350;  // [sin uso] ventana del pulso PFM viejo
constexpr unsigned long AMIX_T_GIRO_ON      = 90;   // [sin uso] ON del pulso PFM viejo
// RETROCESO CON RUMBO al arco rival (SOLO modo quieto, pedido Virginia 2026-06-21: al frenar la
// alineación, la INERCIA lo desalineaba; ahora el retroceso MANTIENE el frente al arco rival con un
// control PROPORCIONAL). Guía skill control-pid-zona-muerta: P con BANDA MUERTA + TOPE (sin D contra
// actuador cuantizado; I sólo si deriva). La autoridad de rotación sale del desbalance M1/M2 (el M3
// queda bajo su piso). Valores de arranque ~ los validados del arquero (kp 2.0, trim_max 30).
constexpr float AMIX_KP_RUMBO_OPP    = 2.0f;  // ganancia proporcional (PWM por grado de error al arco rival). Subir si no alcanza a corregir; bajar si serpentea.
constexpr float AMIX_DEADBAND_OPP_DEG = 5.0f; // banda muerta: |áng al arco| < esto → corrección 0 (no perseguir ruido)
constexpr int   AMIX_ROT_MAX          = 30;   // tope de la corrección de rotación (PWM). Sumado al retroceso M1/M2.
// INICIO — homing al área chica (banco Virginia 2026-06-21): retrocede hasta ver la línea del
// área, avanza un poco a ciegas, y recién ahí patrulla.
// ⚠️ SAFETY a 50 s TEMPORAL (pedido Virginia, para observar el retroceso): después bajar a ~4 s.
constexpr unsigned long AMIX_T_INICIO_RETRO_SAFETY = 50000; // TEMP 50 s (era 4000) — bajar luego
// Avance de SALIDA de la línea del área (homing). Banco Virginia 2026-06-21: con tiempo FIJO a veces
// el arquero quedaba muy cerca de la línea de fondo / medio metido en el área chica. FIX: el arranque
// NO termina por reloj — avanza el impulso MÍNIMO y sale recién cuando YA NO PISA la línea (no ve
// blanco), con un TOPE de seguridad para no quedarse trabado si la línea nunca se "apaga". El avance es
// recto al frente (hacia el campo, lejos del fondo) → es la dirección que lo SACA del área.
constexpr unsigned long AMIX_T_INICIO_AVANCE_MIN    = 400;   // REVERTIDO a 400 (volvió al estado que andaba mejor; era 500). Impulso mínimo del avance de salida de la línea. Sube si parpadea la línea / queda cerca del fondo.
constexpr unsigned long AMIX_T_INICIO_AVANCE_SAFETY = 1200;  // TOPE de seguridad: si a los 1200 ms sigue viendo línea, patrulla igual
// VELOCIDAD del avance del homing (el movimiento a ciegas tras detectar la línea). Banco Virginia
// 2026-06-21: "anda de golpe / fuerte" → SOLO se baja la velocidad (90→75... acá 75). MISMO sentido y
// MISMOS 400 ms que el base; lo ÚNICO distinto vs avanzar() es el PWM. 75 queda apenas sobre el piso
// de las delanteras (70): si STUTTEA/no arranca, subir hacia 85; bajar más NO lo hace más suave (zona
// muerta → tironea). Si aún va "de golpe" el tema es el arranque sin rampa, no el PWM (avisar).
constexpr int AMIX_INICIO_AVANCE_PWM = 75;
// Retroceso del homing: primitiva DEDICADA con PWM propio (no acoplada al despeje) y dirección
// flippable. retroceder_inicio() = M1=-PWM, M2=+PWM (= patrón patear_atras = hacia ATRÁS) × SIGN.
// ⚠️ Si al GO el robot va hacia ADELANTE en vez de atrás → flashear con -DARQMIX_FLIP_INICIO_RETRO.
constexpr int AMIX_INICIO_RETRO_PWM = 100;  // PWM del retroceso de inicio (controlado, no a tope)
#ifdef ARQMIX_FLIP_INICIO_RETRO
constexpr int AMIX_INICIO_RETRO_SIGN = -1;  // invierte la dirección del retroceso de inicio
#else
constexpr int AMIX_INICIO_RETRO_SIGN = +1;
#endif
constexpr unsigned long AMIX_T_PAT_PAUSA_INI = 200;   // PATEANDO_pausa_inicial_arquero
constexpr unsigned long AMIX_T_PAT_ADELANTE  = 450;   // PATEANDO_adelante_arquero (golpe COMPLETO)
#ifdef ARQMIX_KICK_SHORT_ON_LINE
// Golpe CORTO cuando el despeje arranca SOBRE la línea (gateado, pedido Virginia 2026-06-29): menos tiempo de
// empuje → menos envión hacia adelante → no se sale de la cancha. ~250 ms (vs 450). Subir si no despeja la
// pelota lo suficiente; bajar si todavía se sale. <TITRAR EN BANCO>
constexpr unsigned long AMIX_T_PAT_ADELANTE_CORTO = 250;
#endif
constexpr unsigned long AMIX_T_PAT_PAUSA     = 1000;  // PATEANDO_pausa_arquero
// (PATEANDO_atras_arquero NO tiene timeout en 2025: retrocede hasta ver blanco. Acá se
//  agrega un timeout de SEGURIDAD para no colgarse — ver amix_fsm. <MEJORA 2026>)
constexpr unsigned long AMIX_T_ATRAS_SAFETY  = 4000;  // tope de seguridad del retroceso (NO estaba en 2025)

// ============================================================
// Línea (de DOWN) — reemplaza los 3 sensores de luz locales del 2025.
// El 2025 detectaba el borde con s1>=blanco1 || s2>=blanco2 (no s3) durante la
// patrulla, y s1||s2||s3 para "volví a la línea" tras el retroceso. Acá la línea
// llega como line_present/line_depth (amix_io, de DOWN). line_depth>=TRIGGER = tocando.
// ============================================================
constexpr uint8_t AMIX_LINE_DEPTH_TRIGGER = 1;  // ≥1 sensor en blanco = línea presente


// ============================================================
// Comunicación — enlaces TOP (Serial7) y DOWN (Serial1), 230400 (= comm_top/comm_down).
// ============================================================
constexpr long AMIX_UART_BAUD = 230400;

// ============================================================
// Heading — FUENTE = SNAPSHOT del TOP por default (pedido de Virginia: el BNO se lee
// por serie de la placa superior, NO local). Con -DARQMIX_HEADING_OTOS usa el heading
// crudo del OTOS (Pose2D de DOWN) para A/B. NO se inicializa ningún BNO local.
// ============================================================
#ifdef ARQMIX_HEADING_OTOS
constexpr bool AMIX_HEADING_SOURCE_IS_OTOS = true;
#else
constexpr bool AMIX_HEADING_SOURCE_IS_OTOS = false;  // DEFAULT: heading del TOP (snapshot)
#endif

// SIGNO de la corrección de rumbo de la patrulla (las 3 bandas de error en ad/aiproporcional).
// ✅ VALIDADO EN BANCO (Virginia 2026-06-21): el signo CORRECTO para este robot es -1 (con +1 el
// arquero se daba vuelta 180° mientras patrullaba; con -1 queda derecho). Es el DEFAULT.
// El flag -DARQMIX_HEADING_SIGN_OLD vuelve al +1 viejo (NO usar salvo diagnóstico). Requiere
// heading válido del TOP para que la corrección actúe.
#ifdef ARQMIX_HEADING_SIGN_OLD
constexpr float AMIX_HEADING_CORRECT_SIGN = +1.0f;  // viejo (daba 180°) — solo fallback/diagnóstico
#else
constexpr float AMIX_HEADING_CORRECT_SIGN = -1.0f;  // ✅ CORRECTO (banco Virginia 2026-06-21)
#endif

// ============================================================
// ANTICIPACIÓN (S2) — moverse por el SIGNO de la velocidad de la pelota (banco Virginia 2026-06-22).
// ----------------------------------------------------------------------------------
// PROBLEMA: con la pelota CENTRADA (|áng|≤8°) pero viniendo hacia un lado, el arquero esperaba QUIETO hasta
// que el ÁNGULO cruzaba la banda muerta → reaccionaba 1-2 frames tarde y pasaba por al lado. S2: si la pelota
// se ACERCA con componente LATERAL, arranca el strafe hacia el lado del SIGNO de vx ANTES de que cruce la
// banda. Usa SÓLO el SIGNO (la MAGNITUD de vx/vy NO está calibrada). Gateado por -DARQMIX_ANTICIPA (env
// central_robot2_arqueromix_quieto_anticipa). Default OFF → el candidato de competencia es byte-idéntico.
// ⚠️ Los SIGNOS (qué vx es "derecha", qué vy es "acercándose") NO están validados → flags de inversión.
#ifdef ARQMIX_ANTICIPA
constexpr bool AMIX_ANTICIPA = true;
#else
constexpr bool AMIX_ANTICIPA = false;
#endif
// SIGNO de vx para el LADO: si el arquero anticipa para el lado CONTRARIO → -DARQMIX_FLIP_BALL_VX.
#ifdef ARQMIX_FLIP_BALL_VX
constexpr float AMIX_BALL_VX_SIGN = -1.0f;
#else
constexpr float AMIX_BALL_VX_SIGN = +1.0f;
#endif
// SIGNO de vy para "ACERCÁNDOSE": si anticipa cuando la pelota se ALEJA (al revés) → -DARQMIX_FLIP_BALL_VY.
#ifdef ARQMIX_FLIP_BALL_VY
constexpr float AMIX_BALL_VY_SIGN = -1.0f;
#else
constexpr float AMIX_BALL_VY_SIGN = +1.0f;
#endif
// Umbrales (UNIDADES CRUDAS del snapshot, SIN calibrar) — TITULAR EN BANCO. Evitan reaccionar al ruido de la
// velocidad: subir si anticipa con la pelota casi quieta (falsos positivos); bajar si no reacciona a tiempo.
constexpr float AMIX_ANTICIPA_VX_MIN = 50.0f;  // componente LATERAL mínima para anticipar (|vx| > esto)
constexpr float AMIX_ANTICIPA_VY_MIN = 50.0f;  // componente de ACERCAMIENTO mínima (vy·signo > esto)

}  // namespace arqmix
}  // namespace iitasoccer
