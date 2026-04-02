# 👁️ Multi-Cámara y World Model — Skill de Programación

## Fusión de visión, estimación bajo oclusión, comunicación de equipo

---

## 1. CUÁNDO USAR

Siempre que el robot tenga:
- Más de 1 cámara (resolver ambigüedades, fusionar datos)
- Comunicación con compañero de equipo (WiFi/BT)
- Necesidad de rastrear objetos cuando no los ve (pelota ocluida)
- Necesidad de saber posición de rivales

---

## 2. WORLDMODEL: LA CLASE CENTRAL

```cpp
#include "BallKalman.h"  // Ver ball-tracking-advanced.md

class WorldModel {
public:
    // ===== ENTIDADES =====

    // Mi estado
    float my_x = 0, my_y = 0, my_heading = 0;
    float my_vx = 0, my_vy = 0;

    // Pelota (Kalman filter integrado)
    BallKalman ball;

    // Compañero (recibido por WiFi)
    float partner_x = 0, partner_y = 0, partner_heading = 0;
    uint8_t partner_state = 0;
    uint32_t partner_last_update = 0;
    bool partner_alive = false;

    // Arcos
    float goal_own_angle = 0, goal_own_dist = 0;
    bool goal_own_visible = false;
    float goal_opp_angle = 0, goal_opp_dist = 0;
    bool goal_opp_visible = false;

    // Rivales
    struct Rival { float x, y; uint32_t last_seen; bool active; };
    Rival rivals[2] = {};

    // ===== CONFIGURACIÓN DE CÁMARAS =====

    struct CameraConfig {
        float angle_offset;  // Ángulo de montaje relativo al frente (°)
        float fov;           // Campo de visión horizontal (°)
    };
    CameraConfig cameras[4];
    int num_cameras = 1;

    void setup_single_camera() {
        num_cameras = 1;
        cameras[0] = {0, 47.5f};  // Frente, FOV 47.5°
    }

    void setup_dual_camera() {
        num_cameras = 2;
        cameras[0] = {0, 47.5f};    // Frente
        cameras[1] = {180, 47.5f};  // Atrás
    }

    void setup_quad_camera() {
        num_cameras = 4;
        cameras[0] = {0, 47.5f};    // Frente
        cameras[1] = {90, 47.5f};   // Derecha
        cameras[2] = {180, 47.5f};  // Atrás
        cameras[3] = {270, 47.5f};  // Izquierda
    }

    // ===== UPDATE PRINCIPAL (llamar cada 10ms) =====

    void tick(float dt) {
        // Predict pelota siempre
        ball.predict(dt);

        // Degradar compañero si no hay update
        if (millis() - partner_last_update > 500) {
            partner_alive = false;
        }

        // Degradar rivales
        for (auto& r : rivals) {
            if (r.active && millis() - r.last_seen > 1000) r.active = false;
        }
    }

    // ===== ACTUALIZAR DESDE CÁMARAS =====

    struct CameraDetection {
        float angle;       // Ángulo relativo a la cámara (°)
        float distance;    // Distancia estimada (mm)
        uint8_t type;      // BALL, GOAL_OWN, GOAL_OPP, OBSTACLE
        float confidence;  // 0-100
        int camera_id;     // Qué cámara lo detectó
    };

    void process_camera_detections(CameraDetection* dets, int n) {
        // 1. Convertir todas las detecciones a coordenadas del campo
        struct FieldDet {
            float x, y, confidence;
            uint8_t type;
            int camera_id;
        };
        FieldDet field_dets[16];
        int n_field = 0;

        for (int i = 0; i < n && n_field < 16; i++) {
            float cam_global = my_heading + cameras[dets[i].camera_id].angle_offset;
            float global_angle = (cam_global + dets[i].angle) * DEG2RAD;
            field_dets[n_field] = {
                my_x + dets[i].distance * sinf(global_angle),
                my_y + dets[i].distance * cosf(global_angle),
                dets[i].confidence,
                dets[i].type,
                dets[i].camera_id
            };
            n_field++;
        }

        // 2. Fusionar detecciones de pelota (resolver multi-cámara)
        float best_ball_x = 0, best_ball_y = 0;
        float best_ball_conf = 0;
        float ball_weight_sum = 0;
        int ball_count = 0;

        for (int i = 0; i < n_field; i++) {
            if (field_dets[i].type != TYPE_BALL) continue;

            if (ball_count == 0) {
                best_ball_x = field_dets[i].x;
                best_ball_y = field_dets[i].y;
                best_ball_conf = field_dets[i].confidence;
                ball_weight_sum = field_dets[i].confidence;
            } else {
                // Verificar si es la misma pelota (cercana)
                float d = sqrtf(sq(best_ball_x - field_dets[i].x) +
                                sq(best_ball_y - field_dets[i].y));
                if (d < 200) {
                    // Misma pelota vista por otra cámara → promediar
                    float w = field_dets[i].confidence;
                    best_ball_x = (best_ball_x * ball_weight_sum + field_dets[i].x * w) /
                                  (ball_weight_sum + w);
                    best_ball_y = (best_ball_y * ball_weight_sum + field_dets[i].y * w) /
                                  (ball_weight_sum + w);
                    ball_weight_sum += w;
                    best_ball_conf = min(100.0f, best_ball_conf + 10);
                }
                // Si d > 200: probablemente falso positivo, ignorar el de menor confianza
            }
            ball_count++;
        }

        if (ball_count > 0) {
            float R = 400.0f / (best_ball_conf / 100.0f);  // Menor confianza = más ruido
            ball.update(best_ball_x, best_ball_y, R);
        }

        // 3. Actualizar arcos
        for (int i = 0; i < n_field; i++) {
            if (field_dets[i].type == TYPE_GOAL_OWN) {
                goal_own_visible = true;
                // Usar arco como landmark para corregir posición
                correct_position_from_goal(field_dets[i], true);
            }
            if (field_dets[i].type == TYPE_GOAL_OPP) {
                goal_opp_visible = true;
                correct_position_from_goal(field_dets[i], false);
            }
        }

        // 4. Obstáculos → posibles rivales
        for (int i = 0; i < n_field; i++) {
            if (field_dets[i].type != TYPE_OBSTACLE) continue;
            // ¿Es mi compañero? (posición conocida por WiFi)
            if (partner_alive) {
                float d_partner = sqrtf(sq(field_dets[i].x - partner_x) +
                                        sq(field_dets[i].y - partner_y));
                if (d_partner < 200) continue;  // Es mi compañero, no rival
            }
            // No es compañero → rival
            add_or_update_rival(field_dets[i].x, field_dets[i].y);
        }
    }

    // ===== ACTUALIZAR DESDE COMPAÑERO (WiFi) =====

    void process_partner_message(float px, float py, float ph,
                                  float p_ball_x, float p_ball_y,
                                  float p_ball_conf, uint8_t p_state) {
        partner_x = px;
        partner_y = py;
        partner_heading = ph;
        partner_state = p_state;
        partner_last_update = millis();
        partner_alive = true;

        // Si el compañero ve la pelota y yo no (o con baja confianza)
        if (p_ball_conf > 30 && ball.confidence < 50) {
            float R = 800.0f;  // Alta incertidumbre (dato indirecto)
            ball.update(p_ball_x, p_ball_y, R);
        }
    }

    // ===== CONSULTAS ESTRATÉGICAS =====

    // ¿Quién está más cerca de la pelota?
    bool am_i_closer_to_ball() const {
        float my_dist = sqrtf(sq(my_x - ball.x[0]) + sq(my_y - ball.x[1]));
        float partner_dist = sqrtf(sq(partner_x - ball.x[0]) + sq(partner_y - ball.x[1]));
        return my_dist < partner_dist;
    }

    // ¿La pelota viene hacia mi arco?
    bool ball_approaching_own_goal() const {
        return ball.x[3] < -100;  // vy negativo = viene hacia y=0 (nuestro arco)
    }

    // ¿Hay rival entre yo y la pelota?
    bool rival_blocking_ball() const {
        for (const auto& r : rivals) {
            if (!r.active) continue;
            // Verificar si el rival está en la línea entre yo y la pelota
            float dx_ball = ball.x[0] - my_x;
            float dy_ball = ball.x[1] - my_y;
            float dx_rival = r.x - my_x;
            float dy_rival = r.y - my_y;
            float dot = dx_ball * dx_rival + dy_ball * dy_rival;
            float dist_ball = sqrtf(dx_ball*dx_ball + dy_ball*dy_ball);
            if (dot > 0 && dot < dist_ball * dist_ball) {
                // El rival está en el camino
                float cross = fabsf(dx_ball * dy_rival - dy_ball * dx_rival);
                float perp_dist = cross / dist_ball;
                if (perp_dist < 200) return true;  // Bloqueado
            }
        }
        return false;
    }

    // ¿Cuánto hace que no veo la pelota directamente?
    uint32_t ball_age_ms() const { return ball.age_ms(); }

    // ¿La pelota está perdida?
    bool ball_lost() const { return ball.is_lost(); }

private:
    static constexpr float DEG2RAD = 0.017453f;
    static constexpr uint8_t TYPE_BALL = 1;
    static constexpr uint8_t TYPE_GOAL_OWN = 2;
    static constexpr uint8_t TYPE_GOAL_OPP = 3;
    static constexpr uint8_t TYPE_OBSTACLE = 4;

    void correct_position_from_goal(FieldDet& det, bool is_own) {
        float goal_y = is_own ? 0.0f : 2430.0f;
        float goal_x = 910.0f;  // Centro del campo
        float est_x = goal_x - (det.x - my_x);
        float est_y = goal_y - (det.y - my_y);
        my_x = my_x * 0.85f + est_x * 0.15f;
        my_y = my_y * 0.85f + est_y * 0.15f;
    }

    void add_or_update_rival(float rx, float ry) {
        // Buscar rival existente cercano
        for (auto& r : rivals) {
            if (r.active && sqrtf(sq(r.x - rx) + sq(r.y - ry)) < 300) {
                r.x = r.x * 0.7f + rx * 0.3f;
                r.y = r.y * 0.7f + ry * 0.3f;
                r.last_seen = millis();
                return;
            }
        }
        // Nuevo rival
        for (auto& r : rivals) {
            if (!r.active) {
                r = {rx, ry, millis(), true};
                return;
            }
        }
    }

    struct FieldDet { float x, y, confidence; uint8_t type; int camera_id; };
    static float sq(float v) { return v * v; }
};
```

---

## 3. EJEMPLO DE USO EN EL LOOP

```cpp
WorldModel world;
BNO055_Competition imu;
OmniDriveBase drive;
RobustComm cam_front(Serial2, 230400);
RobustComm cam_back(Serial3, 230400);  // Si hay 2da cámara

void setup() {
    world.setup_dual_camera();  // o setup_single_camera()
    // ...
}

void loop() {
    float dt = 0.01f;

    // 1. Update posición propia
    drive.update_odometry();
    world.my_x = drive.get_x();
    world.my_y = drive.get_y();
    world.my_heading = imu.get_heading();

    // 2. Tick del world model
    world.tick(dt);

    // 3. Procesar cámaras
    WorldModel::CameraDetection dets[8];
    int n_dets = 0;
    // Drenar cámara frontal
    while (cam_front.update()) {
        auto pkt = cam_front.last_packet();
        if (pkt.msg_id == MSG_BALL) {
            dets[n_dets++] = {decode_angle(pkt), decode_dist(pkt),
                              TYPE_BALL, decode_conf(pkt), 0};
        }
    }
    // Drenar cámara trasera (si existe)
    if (num_cameras >= 2) {
        while (cam_back.update()) {
            auto pkt = cam_back.last_packet();
            if (pkt.msg_id == MSG_BALL) {
                dets[n_dets++] = {decode_angle(pkt), decode_dist(pkt),
                                  TYPE_BALL, decode_conf(pkt), 1};
            }
        }
    }
    world.process_camera_detections(dets, n_dets);

    // 4. Procesar WiFi del compañero
    if (wifi_has_message()) {
        auto msg = wifi_read_team_message();
        world.process_partner_message(msg.x, msg.y, msg.heading,
                                       msg.ball_x, msg.ball_y,
                                       msg.ball_conf, msg.state);
    }

    // 5. Tomar decisiones estratégicas
    if (world.ball_lost()) {
        search_pattern();
    } else if (world.ball_approaching_own_goal() && my_role == GOALKEEPER) {
        defend_goal();
    } else if (world.am_i_closer_to_ball() && my_role == STRIKER) {
        go_for_ball();
    }
}
```

---

## FUENTES

- Ver docs/multi-camera-world-model.md para análisis completo
- PCBWay team: 4 cámaras, RoboCup Junior 2024-2025
- CAMBADA MSL: world model cooperativo
- ball-tracking-advanced.md: BallKalman con oclusión
- soccer-match-fsm.md: comunicación entre robots
- omni3-drive-base.md: OmniDriveBase para posición
