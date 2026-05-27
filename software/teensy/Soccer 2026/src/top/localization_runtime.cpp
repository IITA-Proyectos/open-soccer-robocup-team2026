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
#include <Arduino.h>

namespace iitasoccer {

namespace {

LocalizationConfig g_config;
LocalizationPose   g_last_pose;
bool               g_initialized = false;

}  // namespace

void localization_runtime_init() {
    // Inicializar config con valores de config_top.h.
    g_config.field_width_mm       = FIELD_WIDTH_MM;
    g_config.field_height_mm      = FIELD_HEIGHT_MM;
    g_config.outlier_threshold_mm = LOCALIZATION_OUTLIER_THRESHOLD_MM;
    for (int i = 0; i < NUM_TOF; ++i) {
        g_config.tof_mount_angle_deg[i] = TOF_MOUNT_ANGLE_DEG[i];
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
