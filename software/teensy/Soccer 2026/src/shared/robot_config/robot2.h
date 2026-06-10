// ============================================================================
//  PERFIL ROBOT2 — BORRADOR, NO CABLEADO AUN.  Ver ROBOT-DEFINITION-DESIGN.md
// ----------------------------------------------------------------------------
//  Este header es el SEED del "robot definition" unico para ROBOT2 (delantero).
//
//  ⚠️ NO LO INCLUYE NINGUN BUILD TODAVIA. Es 100% ADITIVO:
//     - No se compila en ningun env de platformio.ini.
//     - ROBOT1 queda BYTE-IDENTICO. El gate host no lo ve.
//     - La migracion real (que los config_*.h tiren de aca) se hace en una
//       sesion APARTE con compilacion PlatformIO (Teensy). Aca NO se puede
//       compilar Teensy. Ver el PLAN DE MIGRACION en ROBOT-DEFINITION-DESIGN.md.
//
//  OBJETIVO: que el dia que ROBOT2 este armado, este sea EL UNICO archivo a
//  editar/confirmar para subir el firmware y que funcione directo. Todo lo que
//  varia por robot vive aca como #define / constexpr, con PLACEHOLDERS y la
//  marca  // TODO: confirmar en banco  donde el dato es tentativo.
//
//  Fuente de la auditoria: docs/robot-variants/REFERENCIAS-POR-ROBOT.md
//  Diseño:                 docs/robot-variants/ROBOT-DEFINITION-DESIGN.md
//
//  CONVENCION DE MARCAS:
//    [R1=...]   = valor real de ROBOT1 hoy (copiado de los config) para referencia.
//    TODO-BANCO = hay que medirlo/confirmarlo en el ROBOT2 fisico.
//    DELTA-R2   = difiere a-proposito de ROBOT1 segun datos del usuario.
// ============================================================================
#pragma once
#include <stdint.h>

// Identidad del robot (analogo a -DROBOT2; el robot-def lo expone tambien como
// constante por si algun modulo PURO quiere ramificar sin macro de compilacion).
#define ROBOT_ID 2
#define ROBOT_IS_ROBOT2 1
#define ROBOT_NAME_STR "ROBOT2-delantero"

namespace iitasoccer {
namespace robot2 {

// ============================================================================
// 1. MOTORES (placa CENTRAL — Teensy 4.1, Zircon Rev v15)
//    Mapea config_central.h. PER-ROBOT. ⚠️ DELTA-R2 + TODO-BANCO (armado dudoso).
// ============================================================================
// Pines INA/INB/PWM por motor. En R2 los DRIVERS estan ROTADOS respecto a R1:
//   R2: M1=U17(8/7/6)  M2=U7(11/12/4)  M3=U5(2/5/3)
//   [R1=  M1=U5(2/5/3)  M2=U17(8/7/6)  M3=U7(11/12/4) ]
// TODO-BANCO: el usuario advierte que los motores de R2 PUEDEN estar soldados en
// PINES o DIRECCION distintos por error de armado. Correr diag_central_motors en
// R2 y FIJAR aca los pines reales antes de competir.
constexpr int PIN_INA1 = 8;   // M1 → driver U17   // TODO: confirmar en banco
constexpr int PIN_INB1 = 7;                          // TODO: confirmar en banco
constexpr int PIN_PWM1 = 6;                          // TODO: confirmar en banco

constexpr int PIN_INA2 = 11;  // M2 → driver U7     // TODO: confirmar en banco
constexpr int PIN_INB2 = 12;                         // TODO: confirmar en banco
constexpr int PIN_PWM2 = 4;                          // TODO: confirmar en banco

constexpr int PIN_INA3 = 2;   // M3 → driver U5     // TODO: confirmar en banco
constexpr int PIN_INB3 = 5;                          // TODO: confirmar en banco
constexpr int PIN_PWM3 = 3;                          // TODO: confirmar en banco

// Sentido por motor (+1 normal, -1 invertido por HW). Copiado de R1 SIN validar.
// ⚠️ OJO indices: en R2 los drivers estan rotados → el indice 1 de este array es
// el driver U7 (11/12/4), NO el U17. Si la inversion real es del U17 (que en R2 es
// el indice 0), el correcto seria { -1, +1, +1 }. El banco lo dirime.
constexpr int MOTOR_INVERT[3] = { +1, -1, +1 };     // [R1={+1,-1,+1}] TODO: confirmar en banco

// Cinematica omni-3. HOY comun a ambos (config_central.h, fuera del #if ROBOT).
// Si las ruedas de R2 montan a otros angulos, esto se vuelve per-robot AQUI.
constexpr float WHEEL_RADIUS_MM      = 100.0f;       // [R1=100] tentativo // TODO: confirmar en banco
constexpr float WHEEL_ANGLES_DEG[3]  = { 330.0f, 210.0f, 90.0f };  // [R1 igual] calibrado 2026-06-08 (M1=del-izq,M2=del-der,M3=trasera) // R2: confirmar en banco

// Piso de PWM POR RUEDA (deadzone). En R1 es {70,70,42} (delanteras oblicuas necesitan mas
// piso que la trasera paralela). Aca NEUTRO {0,0,0} hasta calibrar el delantero (pines
// rotados -> idx2 NO es la trasera; el banco define que indice sube/baja).
constexpr int MOTOR_MIN_PWM[3]       = { 0, 0, 0 };  // [R1={70,70,42}] // TODO: tunear en banco
constexpr int MOTOR_PWM_NOISE_THRESH = 0;            // [R1=5]

constexpr int   MAX_PWM         = 255;               // constante del core (comun)
constexpr float MAX_SPEED_MM_S  = 1000.0f;           // [R1=1000] estimado (comun)

// ============================================================================
// 2. IMU / BNO (placa TOP — Teensy 4.0)
//    Mapea sensors_imu.cpp + pinout_common.h. ⚠️ DELTA-R2 GRANDE: 2 buses.
// ============================================================================
// R2 tiene 2 BNO FUNCIONALES, uno por bus I2C, AMBOS en la misma direccion 0x28
// (no se distinguen por direccion → se distinguen por BUS). El 2do BNO se soldó a la
// parte INFERIOR del Teensy 4.0 (back-pad) = pines 24/25 = bus **Wire2** (LPI2C4).
// ⚠️ CORRECCION 2026-06-09: 24/25 es Wire2, NO Wire1 (Wire1 = 16/17). CONFIRMADO en
// banco (diag_top_i2c_scan, commit 9da8e9e): BNO2 0x28 VIVO en Wire2; Wire1 vacío.
//   [R1: 2 slots LEFT/RIGHT en el MISMO Wire, 0x28 / 0x29; hoy solo 1 sano (0x28).]
//
// ★ CONVENCION PRIMARIO/SECUNDARIO (Gustavo 2026-06-09): el BNO PRIMARIO (fuente de
// heading preferida) es el que está SOLO en Wire2 (sin ToF → sin contencion i2c → NO
// se congela). El SECUNDARIO es el que comparte Wire(18/19) con los 4 ToF (respaldo).
// → en la tabla: indice 0 = PRIMARIO (Wire2) · indice 1 = SECUNDARIO (Wire).
//
// ⚠️ REQUIERE CAMBIO DE CODIGO en sensors_imu.cpp (hoy ambos BNO hardcodeados a &Wire).
// El read del PRIMARIO por Wire2 + la prioridad primario→secundario está PENDIENTE
// (gateado ROBOT2). El robot-def describe el HW. Ver ROBOT-DEFINITION-DESIGN.md §IMU.
constexpr int     IMU_NUM_BNO = 2;                   // DELTA-R2: 2 BNO funcionales

// Tabla {bus, addr} por BNO. bus: 0 = Wire (18/19) · 2 = Wire2 (24/25). (Wire1 = 16/17, vacío.)
// Indice 0 = PRIMARIO (Wire2, solo) · indice 1 = SECUNDARIO (Wire, con los 4 ToF).
// Ambos en 0x28 pero en buses DISTINTOS.  // CONFIRMADO banco 2026-06-09
constexpr uint8_t IMU_BNO_BUS [2] = { 2, 0 };       // BNO[0]=PRIM Wire2 · BNO[1]=SEC Wire  // DELTA-R2
constexpr uint8_t IMU_BNO_ADDR[2] = { 0x28, 0x28 }; // ambos 0x28               // DELTA-R2

// Signo del heading (chip CW+ → firmware CCW+). Medido en banco R1 = -1.
// Si en R2 el BNO PRIMARIO se monto con otra orientacion, el signo puede diferir.
constexpr float HEADING_SIGN = -1.0f;               // [R1=-1.0] // TODO: confirmar en banco

// Intervalo de lectura y clock I2C. En R1 estan castigados (50ms / 100kHz) por la
// contencion BNO+ToF en el MISMO Wire. En R2, con el BNO PRIMARIO en Wire2 (bus aparte,
// SIN ToF), la contencion no existe en ese bus → oportunidad de leer mas rapido / 400 kHz
// en Wire2. NO bloqueante; arrancar igual que R1 y optimizar en banco.
constexpr uint32_t BNO_READ_INTERVAL_MS = 50;       // [R1=50] // TODO: revisar en banco (Wire2)
constexpr uint32_t I2C_CLOCK_HZ         = 100000;   // [R1=100000] // TODO: probar 400k en Wire2 (primario solo)

// ============================================================================
// 3. ToF (placa TOP — Teensy 4.0)
//    Mapea pinout_robot2.h + pinout_common.h + sensors_tof.cpp.
//    ⚠️ DELTA-R2: modelo distinto, rotados ~90°, uno con FOV ~40°.
// ============================================================================
constexpr int NUM_TOF = 4;                          // [R1=4] (static_assert ata a 4)

// Pines LP (XSHUT). Copiados de R1 SIN validar (construccion manual separada).
// ⚠️ Correr diag_top_tof_census en R2 (con POWER-CYCLE) y fijar los reales.
constexpr int     PIN_TOF_XSHUT[4]        = { 9, 10, 11, 12 };          // [R1 igual] // TODO: confirmar en banco
constexpr uint8_t TOF_I2C_ADDR_ASSIGNED[4] = { 0x2A, 0x2B, 0x2C, 0x2D }; // [R1 igual] // TODO: confirmar en banco

// Mapeo indice → posicion fisica. En R1: [0]=FRENTE [1]=ATRAS [2]=DERECHA [3]=IZQUIERDA.
// ⚠️ DELTA-R2: el usuario dice que en R2 los ToF estan ROTADOS ~90°. Esto cambia el
// mapeo indice→posicion Y/O los mount angles. Confirmar y fijar aca.
// Mount angle por indice (se SUMA al heading; 0=mira +Y arco rival).
// ⚠️ TOF_MOUNT_ANGLE_DEG hoy vive en pinout_common.h (COMUN). En R2 DEBE ser per-robot.
// Placeholder = R1 rotado 90° como HIPOTESIS (NO confirmado).  // TODO: confirmar en banco
constexpr uint16_t TOF_MOUNT_ANGLE_DEG[4] = { 0, 180, 270, 90 };       // [R1={0,180,270,90}] DELTA-R2 (rotacion ~90° A CONFIRMAR)

// Modelo de sensor por slot. R1 = VL53L7CX (Adafruit). R2 usa modelo DISTINTO
// ("mismo circuito, dispuesto distinto"). Si difiere de verdad (p.ej. VL53L5CX /
// VL53L8CX — libs ya estan en lib/), sensors_tof.cpp instancia Adafruit_VL53L7CX
// FIJO → requiere parametrizar el modelo per-robot (cambio de codigo).
// Codigos: 0=VL53L7CX, 1=VL53L5CX, 2=VL53L8CX (enum propuesto, ver design doc).
constexpr uint8_t TOF_MODEL[4] = { 0, 0, 0, 0 };    // [R1=todos VL53L7CX] // TODO: confirmar modelo en banco

// FOV / apertura por sensor en grados. ⚠️ CONSTANTE AUSENTE en TODO el firmware hoy.
// R1 ~60° (VL53L7CX 4x4). R2 tiene UN ToF con ~40° de apertura (el resto ~60°).
// El indice del de 40° depende del montaje rotado → confirmar cual es.
constexpr uint8_t TOF_FOV_DEG[4] = { 60, 60, 60, 40 };  // DELTA-R2: un sensor a 40° // TODO: confirmar cual indice en banco

// Resolucion / frecuencia de ranging. Comun en R1; revisar que el modelo de R2 las soporte.
constexpr uint8_t  TOF_RESOLUTION_ZONES = 16;       // [R1=16 (4x4)]
constexpr uint8_t  TOF_RANGING_FREQ_HZ  = 15;       // [R1=15]

// Offset plano-sensor → centro del robot (mm). Per-robot si el chasis difiere.
constexpr uint16_t TOF_OFFSET_MM = 95;              // [R1=95] placeholder // TODO: confirmar en banco

// Feature flags de presencia (analogos a ROBOT_HAS_TOF_* de pinout_robot2.h).
#define ROBOT2_HAS_TOF_FRONT 1
#define ROBOT2_HAS_TOF_BACK  1
#define ROBOT2_HAS_TOF_LEFT  1
#define ROBOT2_HAS_TOF_RIGHT 1
constexpr int NUM_TOF_ACTIVE = 4;                   // [R1=4] // TODO: confirmar en banco

// HC-SR04 frontal (comun a ambos, listado para completitud del robot-def).
constexpr int PIN_HCSR04_TRIG = 4;                  // [R1=4] (comun)
constexpr int PIN_HCSR04_ECHO = 3;                  // [R1=3] (comun)

// ============================================================================
// 4. OTOS (placa DOWN — Teensy 4.0)   ⚠️ DELTA-R2: R2 NO TIENE OTOS.
// ============================================================================
// R2 = 0 OTOS (no llegaron). El firmware YA soporta 0 (otos.cpp guard
// NUM_OTOS>=1 / >=2): odometria/pose que dependen de OTOS quedan N/A y los
// consumidores caen al fallback exacto (drive_straight, GK paralelo, cross_track).
// Migracion: crear [env:down_robot2] con -DDOWN_NUM_OTOS_CONNECTED=0. Hoy hay UN
// solo [env:down] con =2. Ver ROBOT-DEFINITION-DESIGN.md §OTOS.
#define ROBOT2_HAS_OTOS 0                           // DELTA-R2 (R1 tampoco tiene; arquero)
constexpr int   NUM_OTOS           = 0;             // DELTA-R2: SIN OTOS
constexpr float OTOS_SEPARATION_MM = 0.0f;          // N/A en R2 (sin OTOS) [R1=200 tentativo]

// Muxes de linea: IGUAL que R1/R2 (misma placa DOWN). 4 muxes = 32 sensores.
constexpr int NUM_MUXES_CONNECTED = 4;              // [R1=4] (comun, placa DOWN compartida)

// ============================================================================
// 5. GEOMETRIA del robot (comun salvo que difieran los chasis)
// ============================================================================
// Posiciones de los 32 sensores de linea: LUT del PCB DOWN (sensor_geometry.h),
// comun si ambos robots usan la misma placa DOWN. No se duplica aca (es del PCB).
// Los radios candidatos a per-robot ya estan arriba: WHEEL_RADIUS_MM, TOF_OFFSET_MM.

// ============================================================================
// 6. CANCHA (NO per-robot — listado para que el robot-def sea autocontenido)
// ============================================================================
constexpr uint16_t FIELD_WIDTH_MM  = 1820;          // eje X lado corto (comun)
constexpr uint16_t FIELD_HEIGHT_MM = 2430;          // eje Y arco-a-arco (comun)

// ============================================================================
// 7. CALIBRACION DE CAMARA — DISTANCIA (homografia). PER-ROBOT-Y-PER-CAMARA.
//    Vive HOY en los .py de cada camara (fuera del Teensy). El robot-def la
//    CENTRALIZA como referencia versionada por robot. El generador de .py la
//    consume (ver design doc). Aca van los 2 juegos (front/back) de R2.
//    ⚠️ TODOS placeholder = los del generic, SIN recalibrar (TASK-022).
// ============================================================================
// Homografia 3x3 frontal de R2 (suelo → pixeles). // TODO: recalibrar 4 puntos en banco
constexpr float CAM_FRONT_H_MATRIX[3][3] = {
    {  4.49341044e-02f, -9.48228474e-01f,  7.78932109e+02f },
    { -2.39913185e+00f, -5.65934886e-02f,  3.91128921e+02f },
    { -1.81344856e-03f,  1.15408531e-01f,  1.00000000e+00f },
};  // placeholder generic // TODO: confirmar en banco (R2 front)
constexpr float CAM_FRONT_HEIGHT_CM = 18.7f;        // // TODO: medir en R2

// Homografia 3x3 trasera de R2 (suelo DETRAS → pixeles). // TODO: recalibrar
constexpr float CAM_BACK_H_MATRIX[3][3] = {
    {  4.49341044e-02f, -9.48228474e-01f,  7.78932109e+02f },
    { -2.39913185e+00f, -5.65934886e-02f,  3.91128921e+02f },
    { -1.81344856e-03f,  1.15408531e-01f,  1.00000000e+00f },
};  // placeholder generic (NO sirve para la trasera) // TODO: confirmar en banco (R2 back)
constexpr float CAM_BACK_HEIGHT_CM = 18.7f;         // // TODO: medir en R2

// Factor crudo→mm que aplica el TOP (cameras_runtime.cpp). Placeholder comun.
constexpr float CAMERA_UNIT_TO_MM = 10.0f;          // [R1=10 placeholder] // TODO: calibrar en banco

// ============================================================================
// 8. CALIBRACION DE CAMARA — COLOR / LAB.   *** DECISION DE DISEÑO ***
//    El color LAB es mas per-VENUE/iluminacion que per-robot. Recomendacion del
//    design doc: el LAB NO es parte "dura" del robot-def; va un BASELINE por
//    robot/camara (para no arrancar de cero en Incheon) que se RE-CALIBRA en sede.
//    Se incluye aca SOLO como baseline de arranque (valores generic = placeholder).
//    ⚠️ RECALIBRAR en cada N6 en la sede (TASK-022, bloqueante real #1).
// ============================================================================
// Baseline LAB pelota/arcos (igual front y back hoy; placeholder generic).
constexpr int CAM_NARANJA_THRESHOLD [6] = { 21, 67, 18, 79, -32, 127 };  // pelota // TODO: recalibrar en sede
constexpr int CAM_AMARILLO_THRESHOLD[6] = { 17, 70, -27, 14, 38, 111 };  // arco   // TODO: recalibrar en sede
constexpr int CAM_AZUL_THRESHOLD    [6] = {  4, 36, -13, 57, -64,  -4 };  // arco   // TODO: recalibrar en sede

// Montaje de camara (180° → HMIRROR+VFLIP). Potencialmente per-robot si el
// montaje fisico de R2 difiere. // TODO: confirmar preview en R2
constexpr bool CAM_FRONT_HMIRROR = true;
constexpr bool CAM_FRONT_VFLIP   = true;
constexpr bool CAM_BACK_HMIRROR  = true;
constexpr bool CAM_BACK_VFLIP    = true;

// ============================================================================
// 9. PUERTOS DE COMUNICACION entre placas y camaras — IGUALES en ambos robots.
//    (Confirmado por la auditoria: todo en pinout_common/config_* sin #if ROBOT.)
//    Se listan como CONSTANTES INFORMATIVAS para que el robot-def sea completo;
//    los pines reales de Serial son FIJOS en el Teensy (no se reasignan por #define).
// ============================================================================
constexpr long UART_FROM_DOWN_BAUD = 230400;  // Serial1
constexpr long UART_TO_ZIRCON_BAUD = 230400;  // Serial4 (TOP TX17 → CENTRAL RX28)
constexpr long UART_TO_COMM_BAUD   = 115200;  // Serial2 (7/8) ↔ COMM ESP32-C6
constexpr long UART_CAMERA1_BAUD   = 19200;   // Serial3 (camara frontal)
constexpr long UART_CAMERA2_BAUD   = 19200;   // Serial5 (camara trasera)
// Arbitro = GPIO NIVEL en pin5/6 del TOP (NO UART); match = pin5 OR pin6.

}  // namespace robot2
}  // namespace iitasoccer
