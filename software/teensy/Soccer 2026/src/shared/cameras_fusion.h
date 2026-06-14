// cameras_fusion.h — Fusión dual de cámaras (front + back) → estimación única.
//
// Implementa el algoritmo de la sección 7.2 de docs/firmware/FIRMWARE-PLACA-ARRIBA.md:
//   • La cámara trasera ve el mundo rotado 180° → invertimos signo de (x, y).
//   • Si AMBAS cámaras ven el objeto, promedio ponderado por confianza.
//   • Si solo una lo ve, usar esa.
//   • Si ninguna o ambas están caídas (watchdog), reportar invisible.
//
// Pure functions — sin dependencia de Arduino para que se pueda testear host-native
// con `pio test -e test_native -f test_cameras_fusion`.
//
// El watchdog (timeout por cámara) se aplica AFUERA — esta capa recibe el flag
// `front_alive` / `back_alive` ya calculado por la capa de hardware.

#pragma once
#include <stdint.h>

namespace iitasoccer {

// Observación cruda de una cámara, ya transformada al marco del robot (mm).
//   • Para cam_id=0 (frontal), las coords vienen tal cual del parser.
//   • Para cam_id=1 (trasera), las coords se rotan 180° (cam_obs_to_robot_frame).
struct CamObs {
    float x_mm;
    float y_mm;
    bool visible;
    uint8_t camera_id;   // 0 = frontal, 1 = trasera
};

// Resultado fusionado de la pelota (relativo al robot, mm).
struct BallFused {
    int16_t x_mm;
    int16_t y_mm;
    bool visible;
    uint8_t confidence;     // 0-100
};

// Resultado fusionado de un arco (ángulo + distancia relativos al robot).
//   • angle_centideg: ángulo respecto al frente del robot, +x lateral.
//     Convención: atan2(x_mm, y_mm) (cero = frente, +90° = DERECHA del robot).
//     Ver docs/CONVENCION-EJES-ROBOT.md (+x = derecha, primera persona).
//     [CORREGIDO 2026-05-31: este comentario decía "+90° = izquierda", que
//      contradecía behind_ball.h y la convención canónica. La fórmula es la
//      misma; solo la etiqueta estaba mal.]
//   • distance_mm: módulo del vector.
struct GoalFused {
    int16_t angle_centideg;
    int16_t distance_mm;
    bool visible;
};

// Convierte coords crudas de cámara (unidades ≈ pixel-space) al marco del robot
// en mm. Esta capa es AGNÓSTICA al wire: las coords ya vienen decodificadas del
// packet cámara→TOP v2 (11 B, CRC8+END) por el parser de cameras_runtime; aquí
// solo se escalan. El factor `unit_to_mm` se calibra offline — por ahora
// ~10 mm/unit (placeholder, ver TODO en cameras_runtime.cpp).
//
// cam_id == 1 (trasera) implica rotación 180° → invertir signo de x e y.
CamObs cam_obs_to_robot_frame(int16_t x_raw,
                              int16_t y_raw,
                              bool visible,
                              uint8_t cam_id,
                              float unit_to_mm);

// Fusión dual de la pelota. `front_alive` / `back_alive` reflejan el watchdog
// de la capa runtime (true = la cámara reportó un packet hace < timeout).
//
// Reglas:
//   • Ambas ven   → promedio ponderado por confianza + bonus por consenso.
//   • Solo una ve → esa.
//   • Ninguna ve  → visible=false.
//
// Cuando la cámara está marcada como caída por watchdog, sus datos se ignoran
// aunque el último packet decía visible=true (puede ser stale).
BallFused fuse_ball_dual(const CamObs& front,
                         const CamObs& back,
                         bool front_alive,
                         bool back_alive);

// Idem para un arco. Reporta ángulo + distancia respecto al frente del robot
// (no se devuelven (x, y) porque WorldSnapshot ya tiene goal_opp_angle_centideg
// y goal_opp_distance_mm como campos canónicos).
GoalFused fuse_goal_dual(const CamObs& front,
                        const CamObs& back,
                        bool front_alive,
                        bool back_alive);

// Convierte UNA observación de cámara (ya en marco robot) a polar
// (ángulo_centideg + distancia_mm), con la MISMA convención que fuse_goal_dual
// (atan2(x,y), +90° = derecha). Sirve para exponer las detecciones POR CÁMARA en
// la telemetría sin re-derivar la trigonometría en el glue Arduino. Si la obs no
// es visible, devuelve {0,0,false}. Puro/host-testeable.
GoalFused cam_obs_to_polar(const CamObs& o);

}  // namespace iitasoccer
