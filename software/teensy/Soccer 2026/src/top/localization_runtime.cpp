// localization_runtime.cpp — Glue de hardware para el modulo localization.
//
// Orquesta el polling de sensors_tof + sensors_imu, llama a
// localization_compute() (logica pura del shared/) y cachea el resultado para
// que main_top.cpp lo consuma al armar el WorldSnapshot.
//
// Notas de diseno:
//   • localization.h define NUM_TOF=4 en LocalizationInputs.tof_distance_mm[4].
//     config_top.h define NUM_TOF=4 tambien. Si en algun futuro cambia, hay
//     que tocar ambos lados (es por eso que iteramos hasta NUM_TOF de
//     config_top.h, que es la fuente de verdad del hardware).
//   • prev_x_mm/prev_y_mm/prev_valid se mantienen entre ticks para que el
//     outlier rejection por consistencia funcione del segundo ciclo en
//     adelante. El primer ciclo no usa outlier rejection (prev_valid=false).
//   • bno_offset se calibra UNA vez en init(): el heading actual del BNO
//     queda asociado a "robot apunta al arco rival" (+Y de la cancha).
//   • single-producer single-consumer: tick() escribe g_last_pose,
//     get_pose() lo lee. No hay mutex porque main_top corre en un solo hilo.

#include "localization_runtime.h"
#include "config_top.h"
#include "sensors_tof.h"
#include "sensors_imu.h"
#include "keeper_xy_walls.h"     // estimador XY heading-free del arquero (flag TOP_KEEPER_XY_WALLS)
#include "top_eeprom_config.h"   // g_top_cfg.tof[i].mount_bearing_deg (A2.1 ubicación)
#include <Arduino.h>

namespace iitasoccer {

namespace {

LocalizationConfig g_config;
LocalizationPose   g_last_pose;
bool               g_initialized = false;

#ifdef TOP_KEEPER_XY_WALLS
// --- Estimador XY HEADING-FREE del arquero (ver src/shared/keeper_xy_walls.h) ---
// Tunables de banco (si hace falta, ajustar y reflashear):
constexpr uint8_t  KEEPER_TRIM_PCT       = 35;    // recorte simétrico de la mediana
#ifdef TOP_KEEPER_HEADING_CORR
// Variante rotación-aware (ToF + BNO). Perillas FIJAS en v1 (no tocan la EEPROM; ver
// informe 2026-06-25). Ajuste = reflashear.
constexpr uint16_t KEEPER_WALL_REACH_MM  = 1300;  // max-range ve la pared NEGRA hasta ~1.4 m (medido)
constexpr uint16_t KEEPER_HEAD_GATE_CDEG = 3000;  // |heading|>30° -> pose no confiable -> CONGELA
#else
constexpr uint16_t KEEPER_WALL_REACH_MM  = 1000;  // pared reducida más lejos = no confiable
#endif
constexpr uint16_t KEEPER_XY_CONSISTENCY = 200;   // x_izq≈x_der dentro de esto -> promedio
constexpr uint16_t KEEPER_XY_SYMMETRIC   = 150;   // |d_izq-d_der| chico e inconsistente -> ambiguo

// Lee las 16 zonas crudas de los ToF izquierdo/derecho/trasero, las reduce por
// mediana+recorte (robusto a piso, por-encima y picos), y arma la pose XY
// asumiendo que el arquero mira al frente. Heading del snapshot lo sigue
// poniendo el BNO (esta pose solo escribe x/y/valid).
LocalizationPose compute_keeper_xy_pose() {
    uint16_t zl[16], zr[16], zb[16];
    for (uint8_t k = 0; k < 16; ++k) {
        zl[k] = sensors_tof_get_zone_mm(3, k);  // [3] = izquierda
        zr[k] = sensors_tof_get_zone_mm(2, k);  // [2] = derecha
        zb[k] = sensors_tof_get_zone_mm(1, k);  // [1] = atrás (pared propia)
    }

    KeeperWallDist d;
    d.left_mm  = sensors_tof_is_ready(3)
        ? keeper_wall_dist_mm(zl, 16, TOF_NO_READING, KEEPER_TRIM_PCT) : KEEPER_TOF_NO_READING;
    d.right_mm = sensors_tof_is_ready(2)
        ? keeper_wall_dist_mm(zr, 16, TOF_NO_READING, KEEPER_TRIM_PCT) : KEEPER_TOF_NO_READING;
    d.back_mm  = sensors_tof_is_ready(1)
        ? keeper_wall_dist_mm(zb, 16, TOF_NO_READING, KEEPER_TRIM_PCT) : KEEPER_TOF_NO_READING;

    KeeperXYConfig c;
    c.field_width_mm = FIELD_WIDTH_MM;
    c.tof_offset_mm  = TOF_OFFSET_MM;
    c.wall_reach_mm  = KEEPER_WALL_REACH_MM;
    c.consistency_mm = KEEPER_XY_CONSISTENCY;
    c.symmetric_mm   = KEEPER_XY_SYMMETRIC;
#ifdef TOP_KEEPER_HEADING_CORR
    // Rotación-aware: corrige las distancias por el giro del BNO (cos a la DISTANCIA,
    // no al x/y) y congela la pose si |heading|>gate. El heading 0 debe estar anclado
    // al arco rival (boot mirando al arco o comando IMU ZERO).
    const int16_t hdg_cdeg = sensors_imu_get_heading_centideg();
    const KeeperXY r = keeper_xy_from_walls_heading(d, c, hdg_cdeg, KEEPER_HEAD_GATE_CDEG);
#else
    const KeeperXY r = keeper_xy_from_walls(d, c);
#endif

    LocalizationPose p{};
    // X desconocida -> centro de cancha (no dispara el X-bound del arquero).
    p.x_mm = r.x_valid ? r.x_mm : (int16_t)(FIELD_WIDTH_MM / 2);
    p.y_mm = r.y_mm;
    // La conf la maneja la Y (profundidad): eje confiable en la zona de fondo.
    p.valid = r.y_valid;
#ifdef TOP_KEEPER_HEADING_CORR
    p.heading_centideg = hdg_cdeg;   // el rumbo del BNO acompaña la pose
#else
    p.heading_centideg = 0;
#endif
    p.source_flags = (uint8_t)(
        (d.left_mm  != KEEPER_TOF_NO_READING ? 0x08 : 0) |
        (d.right_mm != KEEPER_TOF_NO_READING ? 0x04 : 0) |
        (d.back_mm  != KEEPER_TOF_NO_READING ? 0x02 : 0));
    return p;
}
#endif  // TOP_KEEPER_XY_WALLS

}  // namespace

void localization_runtime_init() {
    // Inicializar config con valores de config_top.h.
    g_config.field_width_mm       = FIELD_WIDTH_MM;
    g_config.field_height_mm      = FIELD_HEIGHT_MM;
    g_config.outlier_threshold_mm = LOCALIZATION_OUTLIER_THRESHOLD_MM;
    // F1a: radio del robot (plano-sensor ToF -> centro). Valor PLACEHOLDER en
    // pinout_common.h; medir en HW y validar en banco (TASK del equipo).
    g_config.tof_offset_mm = TOF_OFFSET_MM;
    for (int i = 0; i < NUM_TOF; ++i) {
        // A2.1: la UBICACIÓN de cada ToF viene de la config (default = el mapeo
        // hardcodeado TOF_MOUNT_ANGLE_DEG → byte-idéntico salvo que se reasigne).
        g_config.tof_mount_angle_deg[i] =
            (i < TOP_CFG_NUM_TOF) ? (uint16_t)g_top_cfg.tof[i].mount_bearing_deg
                                  : TOF_MOUNT_ANGLE_DEG[i];
    }
    g_config.prev_x_mm  = FIELD_WIDTH_MM / 2;   // centro como guess inicial
    g_config.prev_y_mm  = FIELD_HEIGHT_MM / 2;
    g_config.prev_valid = false;  // primer ciclo no usa outlier rejection

    // Esperar estabilizacion del BNO antes de leer heading.
    // (El sensors_imu_init() ya hace su propio delay STABILIZE_MS, pero damos
    // un margen extra antes de capturar el offset.)
    delay(100);

    // Calibrar offset: el heading actual del BNO se asocia a "robot apunta al
    // arco rival" (+Y de la cancha). De ahi en mas, localization_compute()
    // restara este offset para obtener heading relativo a la cancha.
    g_config.bno_offset_centideg = sensors_imu_get_heading_centideg();

    Serial.print("[localization] BNO offset = ");
    Serial.print(g_config.bno_offset_centideg / 100.0);
    Serial.println(" deg (robot DEBE apuntar al arco rival al encender)");

    g_initialized       = true;
    g_last_pose         = LocalizationPose{};
    g_last_pose.valid   = false;
}

void localization_runtime_tick() {
    if (!g_initialized) return;

#ifdef TOP_KEEPER_XY_WALLS
    // Modo ARQUERO heading-free: la pose XY sale de las paredes (izq/der/atrás),
    // sin BNO. Reemplaza la trilateración clásica. (Default OFF -> byte-idéntico.)
    g_last_pose = compute_keeper_xy_pose();
    return;
#endif

    // Armar inputs leyendo de los modulos de sensores.
    LocalizationInputs in{};
    for (int i = 0; i < NUM_TOF; ++i) {
        in.tof_distance_mm[i] = sensors_tof_get_distance_mm(i);
        in.tof_valid[i] = (in.tof_distance_mm[i] != TOF_NO_READING)
                          && sensors_tof_is_ready(i);
    }
    in.bno_heading_centideg = sensors_imu_get_heading_centideg();

    // Llamar al algoritmo puro.
    g_last_pose = localization_compute(in, g_config);

    // Actualizar prev_* para el outlier rejection del proximo ciclo.
    if (g_last_pose.valid) {
        g_config.prev_x_mm  = g_last_pose.x_mm;
        g_config.prev_y_mm  = g_last_pose.y_mm;
        g_config.prev_valid = true;
    }
    // Nota: si el pose actual es invalid, NO reseteamos prev_valid a false —
    // mantenemos el ultimo pose bueno como referencia para el proximo ciclo.
    // Esto evita perder la referencia historica por un ciclo malo aislado.
}

LocalizationPose localization_runtime_get_pose() {
    return g_last_pose;
}

}  // namespace iitasoccer
