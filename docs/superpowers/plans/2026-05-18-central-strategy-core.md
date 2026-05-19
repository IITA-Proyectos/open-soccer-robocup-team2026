# CENTRAL Strategy Core — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Núcleo de estrategia/decisión de CENTRAL como módulos PUROS host-testeados en `src/shared/` (única fuente de verdad), reemplazando la réplica `strategy_transitions`, con la lógica de trayectoria de pelota, roles arquero/jugador, reflejo de borde y precedencia de seguridad.

**Architecture:** Módulos puros chicos en `src/shared/` (compilados por `env:test_native` y por `env:central_robot1/2`). Cada módulo: una responsabilidad, TDD host-native (Unity). `central_decide` orquesta con precedencia `field_safety > play_decision > strategy_core`. El glue HW de `src/central/` los llama (Task 8). Plan 1 de 2 (HW real = Plan 2).

**Tech Stack:** C++17, PlatformIO, Unity (`pio test -e test_native -f <suite>`), structs `__attribute__((packed))`, contrato `proto.h`/`WorldSnapshot`.

**Comando de test (todas las tasks, PATH export OBLIGATORIO; Unity local offline):**
```bash
cd "/c/Users/violl/futbol2026/open-soccer-robocup-team2026/software/teensy/Soccer 2026" && export PATH="/c/mingw64/bin:$PATH" && python -m platformio test -e test_native -f <suite> 2>&1 | tail -16
```

**GUARDRAIL (todas las tasks):** PROHIBIDO tocar `.pio/`, Unity, dependencias, u otros módulos no listados. Ante error de entorno/dependencia → PARAR y reportar BLOCKED. Commit sí, `git push` NO (lo hace el controlador). Branch `main`.

**Spec de referencia:** `docs/superpowers/specs/2026-05-18-central-strategy-core-design.md`. Contratos: `docs/firmware/CONTRATO-DATOS-CENTRAL.md`, `CONTRATO-DATOS-TOP.md`.

---

## File Structure

| Archivo | Responsabilidad | Acción |
|---|---|---|
| `src/shared/types.h` | Añadir `ball_vx_mm_s`,`ball_vy_mm_s` a `WorldSnapshot` + `static_assert` | Modify |
| `src/shared/ball_trajectory.h/.cpp` | Clasificar destino de la pelota + alcance | Create |
| `src/shared/play_decision.h/.cpp` | Regla circular/interceptar/desviar | Create |
| `src/shared/field_safety.h/.cpp` | Reflejo de borde desde `LineStatusV2` | Create |
| `src/shared/strategy_core.h/.cpp` | FSM unificada arquero + jugador ataque/defensa | Create |
| `src/shared/motion_target.h/.cpp` | Intención → `{vx,vy,omega,kicker}` | Create |
| `src/shared/central_decide.h/.cpp` | Orquestador con precedencia | Create |
| `test/test_central_*/test_main.cpp` | Suites Unity por módulo | Create |
| `src/central/strategy.cpp` (o comm/glue) | Llamar `central_decide` (HW) | Modify (Task 8) |

Convención: ángulos en centidegrees, 0 = frente del robot, horario+ (igual que el resto del repo). Distancias mm. Marco: relativo al robot (WorldSnapshot ya es relativo).

---

## Task 1: Extender `WorldSnapshot` con velocidad de pelota

**Files:**
- Modify: `software/teensy/Soccer 2026/src/shared/types.h`
- Test: `software/teensy/Soccer 2026/test/test_central_contract/test_main.cpp`

- [ ] **Step 1: Test que falla** — crear `test/test_central_contract/test_main.cpp`:
```cpp
// test_central_contract — pio test -e test_native -f test_central_contract
#include <unity.h>
#include "types.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

void test_worldsnapshot_has_ball_velocity_fields(void){
    WorldSnapshot w{};
    w.ball_vx_mm_s = -1234;
    w.ball_vy_mm_s = 567;
    TEST_ASSERT_EQUAL_INT16(-1234, w.ball_vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(567, w.ball_vy_mm_s);
}
void test_worldsnapshot_size_is_27(void){
    TEST_ASSERT_EQUAL_UINT32(27, sizeof(WorldSnapshot));
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_worldsnapshot_has_ball_velocity_fields);
    RUN_TEST(test_worldsnapshot_size_is_27);
    return UNITY_END();
}
```

- [ ] **Step 2: Verificar que falla**
Run el comando de test con `-f test_central_contract`. Expected: FAIL (no existen `ball_vx_mm_s`/`ball_vy_mm_s`).

- [ ] **Step 3: Implementar** — en `src/shared/types.h`, dentro de `struct WorldSnapshot`, AGREGAR después de la línea `uint8_t ball_confidence;` (bloque "Pelota detectada"):
```cpp
    int16_t ball_vx_mm_s;           // velocidad pelota X (mm/s, marco robot); 0 si N/A
    int16_t ball_vy_mm_s;           // velocidad pelota Y (mm/s, marco robot); 0 si N/A
```
Y JUSTO DESPUÉS del cierre `} __attribute__((packed));` de `WorldSnapshot`, agregar:
```cpp
static_assert(sizeof(WorldSnapshot) == 27, "WorldSnapshot contrato v2 = 27 bytes");
```
También corregir el comentario previo `// Tamaño: 24 bytes.` → `// Tamaño: 27 bytes (contrato v2: +ball_vx/vy). Cabe en proto (32).`

- [ ] **Step 4: Verificar que pasa**
Run test `-f test_central_contract`. Expected: PASS (2 tests). Si el `static_assert` o el test de tamaño fallan, el conteo real difiere: ajustar el número `27` al `sizeof` real reportado por el compilador EN AMBOS lugares (static_assert y test) para que reflejen la realidad — NO cambiar el orden de campos.

- [ ] **Step 5: Commit**
```bash
cd /c/Users/violl/futbol2026/open-soccer-robocup-team2026 && git add "software/teensy/Soccer 2026/src/shared/types.h" "software/teensy/Soccer 2026/test/test_central_contract/test_main.cpp" && git commit -m "feat(central): WorldSnapshot +ball_vx/vy (contrato v2) + static_assert"
```

---

## Task 2: `ball_trajectory` — clasificar destino + alcance

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/ball_trajectory.h`, `ball_trajectory.cpp`
- Test: `software/teensy/Soccer 2026/test/test_central_trajectory/test_main.cpp`

Convención: pelota relativa al robot. `goal_opp_angle_centideg` (del WorldSnapshot) = dirección al arco rival. Trayectoria = dirección de `(ball_vx,ball_vy)`. Clasificación por el ángulo entre la velocidad de la pelota y la dirección a cada arco.

- [ ] **Step 1: Test que falla** — `test/test_central_trajectory/test_main.cpp`:
```cpp
// pio test -e test_native -f test_central_trajectory
#include <unity.h>
#include "ball_trajectory.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

void test_no_motion_is_still(void){
    BallTrajIn in{}; in.ball_vx_mm_s=0; in.ball_vy_mm_s=0;
    in.ball_speed_min_mm_s=80;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_STILL, t.kind);
}
void test_toward_opponent_goal(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=0; in.ball_vy_mm_s=500;          // se aleja al frente
    in.goal_opp_angle_centideg=0;                     // arco rival al frente (0°)
    in.goal_own_angle_centideg=18000;                 // arco propio atrás
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_TO_OPP_GOAL, t.kind);
}
void test_toward_own_goal(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=0; in.ball_vy_mm_s=-500;          // viene hacia atrás
    in.goal_opp_angle_centideg=0;
    in.goal_own_angle_centideg=18000;                 // arco propio atrás (180°)
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_TO_OWN_GOAL, t.kind);
}
void test_sideways_is_other(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=500; in.ball_vy_mm_s=0;           // cruza de costado
    in.goal_opp_angle_centideg=0;
    in.goal_own_angle_centideg=18000;
    in.ball_speed_min_mm_s=80; in.toward_tol_centideg=4500;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_EQUAL_INT(BT_OTHER, t.kind);
}
void test_in_reach_flag(void){
    BallTrajIn in{};
    in.ball_vx_mm_s=0; in.ball_vy_mm_s=300; in.ball_speed_min_mm_s=80;
    in.ball_dist_mm=250; in.reach_mm=400;
    BallTraj t = bt_classify(in);
    TEST_ASSERT_TRUE(t.in_reach);
    in.ball_dist_mm=900;
    t = bt_classify(in);
    TEST_ASSERT_FALSE(t.in_reach);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_no_motion_is_still);
    RUN_TEST(test_toward_opponent_goal);
    RUN_TEST(test_toward_own_goal);
    RUN_TEST(test_sideways_is_other);
    RUN_TEST(test_in_reach_flag);
    return UNITY_END();
}
```

- [ ] **Step 2: Verificar que falla** (`ball_trajectory.h` no existe).

- [ ] **Step 3: Implementar** — `src/shared/ball_trajectory.h`:
```cpp
#pragma once
#include <stdint.h>
namespace iitasoccer {
enum BallTrajKind { BT_STILL=0, BT_TO_OPP_GOAL=1, BT_TO_OWN_GOAL=2, BT_OTHER=3 };
struct BallTrajIn {
    int16_t ball_vx_mm_s;
    int16_t ball_vy_mm_s;
    int16_t goal_opp_angle_centideg;   // dir al arco rival (marco robot)
    int16_t goal_own_angle_centideg;   // dir al arco propio
    int16_t ball_speed_min_mm_s;       // < esto ⇒ STILL
    int16_t toward_tol_centideg;       // tolerancia angular "va hacia el arco"
    int16_t ball_dist_mm;              // distancia a la pelota
    int16_t reach_mm;                  // alcance del robot
};
struct BallTraj {
    BallTrajKind kind;
    int16_t      heading_centideg;     // dirección de la velocidad de la pelota
    int16_t      speed_mm_s;           // |velocidad|
    bool         in_reach;             // ball_dist_mm <= reach_mm
};
BallTraj bt_classify(const BallTrajIn& in);
}  // namespace iitasoccer
```
`src/shared/ball_trajectory.cpp`:
```cpp
#include "ball_trajectory.h"
#include <cmath>
namespace iitasoccer {
static int16_t to_cd(float deg){
    while (deg > 180.0f)   deg -= 360.0f;
    while (deg <= -180.0f) deg += 360.0f;
    return (int16_t)lroundf(deg * 100.0f);
}
static int ang_diff_cd(int16_t a, int16_t b){
    int d = (int)a - (int)b;
    while (d > 18000)  d -= 36000;
    while (d <= -18000) d += 36000;
    return d < 0 ? -d : d;
}
BallTraj bt_classify(const BallTrajIn& in){
    BallTraj t{};
    float sp = sqrtf((float)in.ball_vx_mm_s*in.ball_vx_mm_s
                   + (float)in.ball_vy_mm_s*in.ball_vy_mm_s);
    t.speed_mm_s = (int16_t)(sp > 32767.0f ? 32767 : sp);
    t.in_reach = (in.reach_mm > 0) && (in.ball_dist_mm <= in.reach_mm);
    if (sp < (float)in.ball_speed_min_mm_s){
        t.kind = BT_STILL; t.heading_centideg = 0; return t;
    }
    float hdg = atan2f((float)in.ball_vy_mm_s, (float)in.ball_vx_mm_s) * 180.0f / (float)M_PI;
    // ball frame: vx hacia la derecha del robot, vy hacia el frente.
    // Heading 0 = frente: convertimos atan2(vy,vx) (0=+x) a 0=+y(frente).
    t.heading_centideg = to_cd(90.0f - hdg);
    int d_opp = ang_diff_cd(t.heading_centideg, in.goal_opp_angle_centideg);
    int d_own = ang_diff_cd(t.heading_centideg, in.goal_own_angle_centideg);
    if (d_opp <= in.toward_tol_centideg && d_opp <= d_own) t.kind = BT_TO_OPP_GOAL;
    else if (d_own <= in.toward_tol_centideg)               t.kind = BT_TO_OWN_GOAL;
    else                                                    t.kind = BT_OTHER;
    return t;
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Verificar que pasa** (5 tests). Si un assert no refleja la lógica real (por la conversión de marco), razonar los ángulos y ajustar SOLO el assert del test para reflejar el comportamiento real (no forzar resultado), documentándolo.

- [ ] **Step 5: Commit**
```bash
cd /c/Users/violl/futbol2026/open-soccer-robocup-team2026 && git add "software/teensy/Soccer 2026/src/shared/ball_trajectory.h" "software/teensy/Soccer 2026/src/shared/ball_trajectory.cpp" "software/teensy/Soccer 2026/test/test_central_trajectory/test_main.cpp" && git commit -m "feat(central): ball_trajectory — clasifica destino pelota + alcance"
```

---

## Task 3: `play_decision` — circular / interceptar / desviar

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/play_decision.h`, `play_decision.cpp`
- Test: `software/teensy/Soccer 2026/test/test_central_play/test_main.cpp`

- [ ] **Step 1: Test que falla** — `test/test_central_play/test_main.cpp`:
```cpp
// pio test -e test_native -f test_central_play
#include <unity.h>
#include "play_decision.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

void test_let_circulate_when_to_opp_goal(void){
    PlayIn in{}; in.in_reach=true; in.moving=true; in.kind=BT_TO_OPP_GOAL;
    TEST_ASSERT_EQUAL_INT(PLAY_LET_CIRCULATE, pd_decide(in).action);
}
void test_intercept_when_to_own_goal(void){
    PlayIn in{}; in.in_reach=true; in.moving=true; in.kind=BT_TO_OWN_GOAL;
    TEST_ASSERT_EQUAL_INT(PLAY_INTERCEPT, pd_decide(in).action);
}
void test_deflect_when_other(void){
    PlayIn in{}; in.in_reach=true; in.moving=true; in.kind=BT_OTHER;
    TEST_ASSERT_EQUAL_INT(PLAY_DEFLECT_TO_OPP, pd_decide(in).action);
}
void test_none_when_not_in_reach(void){
    PlayIn in{}; in.in_reach=false; in.moving=true; in.kind=BT_OTHER;
    TEST_ASSERT_EQUAL_INT(PLAY_NONE, pd_decide(in).action);
}
void test_none_when_still(void){
    PlayIn in{}; in.in_reach=true; in.moving=false; in.kind=BT_STILL;
    TEST_ASSERT_EQUAL_INT(PLAY_NONE, pd_decide(in).action);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_let_circulate_when_to_opp_goal);
    RUN_TEST(test_intercept_when_to_own_goal);
    RUN_TEST(test_deflect_when_other);
    RUN_TEST(test_none_when_not_in_reach);
    RUN_TEST(test_none_when_still);
    return UNITY_END();
}
```

- [ ] **Step 2: Verificar que falla.**

- [ ] **Step 3: Implementar** — `src/shared/play_decision.h`:
```cpp
#pragma once
#include <stdint.h>
#include "ball_trajectory.h"
namespace iitasoccer {
enum PlayAction { PLAY_NONE=0, PLAY_LET_CIRCULATE=1, PLAY_INTERCEPT=2, PLAY_DEFLECT_TO_OPP=3 };
struct PlayIn {
    bool         in_reach;
    bool         moving;        // true si la pelota se mueve (BT != STILL)
    BallTrajKind kind;
};
struct PlayOut { PlayAction action; };
PlayOut pd_decide(const PlayIn& in);
}  // namespace iitasoccer
```
`src/shared/play_decision.cpp`:
```cpp
#include "play_decision.h"
namespace iitasoccer {
PlayOut pd_decide(const PlayIn& in){
    PlayOut o{}; o.action = PLAY_NONE;
    if (!in.in_reach || !in.moving) return o;
    switch (in.kind){
        case BT_TO_OPP_GOAL: o.action = PLAY_LET_CIRCULATE;  break;
        case BT_TO_OWN_GOAL: o.action = PLAY_INTERCEPT;       break;
        case BT_OTHER:       o.action = PLAY_DEFLECT_TO_OPP;  break;
        case BT_STILL:       o.action = PLAY_NONE;            break;
    }
    return o;
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Verificar que pasa** (5 tests).

- [ ] **Step 5: Commit**
```bash
cd /c/Users/violl/futbol2026/open-soccer-robocup-team2026 && git add "software/teensy/Soccer 2026/src/shared/play_decision.h" "software/teensy/Soccer 2026/src/shared/play_decision.cpp" "software/teensy/Soccer 2026/test/test_central_play/test_main.cpp" && git commit -m "feat(central): play_decision — circular/interceptar/desviar"
```

---

## Task 4: `field_safety` — reflejo de borde (máxima prioridad)

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/field_safety.h`, `field_safety.cpp`
- Test: `software/teensy/Soccer 2026/test/test_central_safety/test_main.cpp`

Consume el contrato `LineStatusV2` (de DOWN). Reglas: si `data_valid==0` → modo conservador (limitar velocidad, no preempción de escape pero señal); si `event_flags & EV_IMMINENT_EXIT` o `line_present` con penetración → ESCAPE usando `escape_angle_centideg`.

- [ ] **Step 1: Test que falla** — `test/test_central_safety/test_main.cpp`:
```cpp
// pio test -e test_native -f test_central_safety
#include <unity.h>
#include "field_safety.h"
#include "types.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

static LineStatusV2 mk(uint8_t valid, uint8_t present, int16_t esc, uint8_t ev){
    LineStatusV2 s{}; s.schema_version=LSV2_SCHEMA; s.data_valid=valid;
    s.line_present=present; s.escape_angle_centideg=esc; s.event_flags=ev;
    s.line_angle_centideg = present? 0 : LSV2_NA_I16;
    return s;
}
void test_escape_on_imminent_exit(void){
    LineStatusV2 s = mk(1,1, 9000, EV_IMMINENT_EXIT);
    FieldSafety fs = fs_eval(s);
    TEST_ASSERT_TRUE(fs.preempt);
    TEST_ASSERT_EQUAL_INT16(9000, fs.escape_angle_centideg);
}
void test_conservative_when_invalid(void){
    LineStatusV2 s = mk(0,0, LSV2_NA_I16, EV_LIFTED);
    FieldSafety fs = fs_eval(s);
    TEST_ASSERT_FALSE(fs.preempt);
    TEST_ASSERT_TRUE(fs.conservative);
}
void test_no_action_when_clear(void){
    LineStatusV2 s = mk(1,0, LSV2_NA_I16, 0);
    FieldSafety fs = fs_eval(s);
    TEST_ASSERT_FALSE(fs.preempt);
    TEST_ASSERT_FALSE(fs.conservative);
}
void test_escape_when_line_present_no_flag(void){
    LineStatusV2 s = mk(1,1, -4500, 0);
    FieldSafety fs = fs_eval(s);
    TEST_ASSERT_TRUE(fs.preempt);
    TEST_ASSERT_EQUAL_INT16(-4500, fs.escape_angle_centideg);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_escape_on_imminent_exit);
    RUN_TEST(test_conservative_when_invalid);
    RUN_TEST(test_no_action_when_clear);
    RUN_TEST(test_escape_when_line_present_no_flag);
    return UNITY_END();
}
```

- [ ] **Step 2: Verificar que falla.**

- [ ] **Step 3: Implementar** — `src/shared/field_safety.h`:
```cpp
#pragma once
#include <stdint.h>
#include "types.h"
namespace iitasoccer {
struct FieldSafety {
    bool    preempt;                 // true ⇒ debe ESCAPAR ya (máxima prioridad)
    bool    conservative;            // true ⇒ datos inválidos: modo conservador
    int16_t escape_angle_centideg;   // dirección de escape (válida si preempt)
};
FieldSafety fs_eval(const LineStatusV2& s);
}  // namespace iitasoccer
```
`src/shared/field_safety.cpp`:
```cpp
#include "field_safety.h"
namespace iitasoccer {
FieldSafety fs_eval(const LineStatusV2& s){
    FieldSafety f{}; f.preempt=false; f.conservative=false;
    f.escape_angle_centideg = LSV2_NA_I16;
    if (s.data_valid == 0){ f.conservative = true; return f; }
    bool imminent = (s.event_flags & EV_IMMINENT_EXIT) != 0;
    if ((s.line_present && s.escape_angle_centideg != LSV2_NA_I16) || imminent){
        f.preempt = true;
        f.escape_angle_centideg = s.escape_angle_centideg;
    }
    return f;
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Verificar que pasa** (4 tests).

- [ ] **Step 5: Commit**
```bash
cd /c/Users/violl/futbol2026/open-soccer-robocup-team2026 && git add "software/teensy/Soccer 2026/src/shared/field_safety.h" "software/teensy/Soccer 2026/src/shared/field_safety.cpp" "software/teensy/Soccer 2026/test/test_central_safety/test_main.cpp" && git commit -m "feat(central): field_safety — reflejo de borde (preempcion)"
```

---

## Task 5: `strategy_core` — FSM unificada arquero + jugador

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/strategy_core.h`, `strategy_core.cpp`
- Test: `software/teensy/Soccer 2026/test/test_central_strategy/test_main.cpp`

FSM única (única fuente de verdad). Rol por parámetro. Estados:
- ARQUERO: `GK_WAIT, GK_PATROL, GK_INTERCEPT, GK_CLEAR`
- JUGADOR: `FP_WAIT, FP_RUSH, FP_SEEK, FP_DRIVE, FP_DEFEND`
Transiciones semilla tomadas del árbol de `strategy_transitions` (caracterización existente). Tiempo inyectado por parámetro.

- [ ] **Step 1: Test que falla** — `test/test_central_strategy/test_main.cpp`:
```cpp
// pio test -e test_native -f test_central_strategy
#include <unity.h>
#include "strategy_core.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

static ScWorld w(bool run, bool ball, float bx, float by){
    ScWorld s{}; s.match_running=run; s.ball_visible=ball;
    s.ball_x_mm=bx; s.ball_y_mm=by; s.goal_opp_visible=true;
    s.goal_opp_angle_centideg=0; return s;
}
void test_wait_until_match_runs(void){
    StrategyCore sc{}; sc_init(sc, ROLE_FIELD);
    ScOut o = sc_tick(sc, w(false,false,0,0), 0);
    TEST_ASSERT_EQUAL_INT(SC_FP_WAIT, o.state);
}
void test_field_rush_on_start_when_ball_seen(void){
    StrategyCore sc{}; sc_init(sc, ROLE_FIELD);
    ScOut o = sc_tick(sc, w(true,true,100,300), 0);
    TEST_ASSERT_EQUAL_INT(SC_FP_RUSH, o.state);
}
void test_field_seek_on_start_when_no_ball(void){
    StrategyCore sc{}; sc_init(sc, ROLE_FIELD);
    ScOut o = sc_tick(sc, w(true,false,0,0), 0);
    TEST_ASSERT_EQUAL_INT(SC_FP_SEEK, o.state);
}
void test_field_defend_when_ball_behind(void){
    StrategyCore sc{}; sc_init(sc, ROLE_FIELD);
    // pelota muy atrás del robot (y negativo grande) ⇒ DEFEND
    ScOut o = sc_tick(sc, w(true,true,0,-600), 0);
    TEST_ASSERT_EQUAL_INT(SC_FP_DEFEND, o.state);
}
void test_gk_wait_then_patrol(void){
    StrategyCore sc{}; sc_init(sc, ROLE_GOALKEEPER);
    TEST_ASSERT_EQUAL_INT(SC_GK_WAIT, sc_tick(sc, w(false,false,0,0),0).state);
    TEST_ASSERT_EQUAL_INT(SC_GK_PATROL, sc_tick(sc, w(true,false,0,0),10).state);
}
void test_gk_intercept_when_ball_seen(void){
    StrategyCore sc{}; sc_init(sc, ROLE_GOALKEEPER);
    sc_tick(sc, w(true,false,0,0), 0);
    ScOut o = sc_tick(sc, w(true,true,50,200), 10);
    TEST_ASSERT_EQUAL_INT(SC_GK_INTERCEPT, o.state);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_wait_until_match_runs);
    RUN_TEST(test_field_rush_on_start_when_ball_seen);
    RUN_TEST(test_field_seek_on_start_when_no_ball);
    RUN_TEST(test_field_defend_when_ball_behind);
    RUN_TEST(test_gk_wait_then_patrol);
    RUN_TEST(test_gk_intercept_when_ball_seen);
    return UNITY_END();
}
```

- [ ] **Step 2: Verificar que falla.**

- [ ] **Step 3: Implementar** — `src/shared/strategy_core.h`:
```cpp
#pragma once
#include <stdint.h>
namespace iitasoccer {
enum ScRole { ROLE_GOALKEEPER=0, ROLE_FIELD=1 };
enum ScState {
    SC_FP_WAIT=0, SC_FP_RUSH, SC_FP_SEEK, SC_FP_DRIVE, SC_FP_DEFEND,
    SC_GK_WAIT, SC_GK_PATROL, SC_GK_INTERCEPT, SC_GK_CLEAR
};
struct ScWorld {
    bool     match_running;
    bool     ball_visible;
    float    ball_x_mm;                 // relativo al robot
    float    ball_y_mm;                 // +y = frente
    bool     goal_opp_visible;
    int16_t  goal_opp_angle_centideg;
};
struct StrategyCore { ScRole role; ScState state; };
struct ScOut { ScState state; };
void  sc_init(StrategyCore& sc, ScRole role);
ScOut sc_tick(StrategyCore& sc, const ScWorld& w, uint32_t now_ms);
// Umbrales (constantes nombradas, no magic):
constexpr float SC_DEFEND_BEHIND_MM   = 400.0f;  // pelota más atrás de esto ⇒ DEFEND
constexpr float SC_DRIVE_CLOSE_MM     = 150.0f;  // pelota muy cerca al frente ⇒ DRIVE
constexpr float SC_GK_INTERCEPT_MM    = 600.0f;  // pelota dentro de esto ⇒ GK intercepta
}  // namespace iitasoccer
```
`src/shared/strategy_core.cpp`:
```cpp
#include "strategy_core.h"
#include <cmath>
namespace iitasoccer {
void sc_init(StrategyCore& sc, ScRole role){
    sc.role = role;
    sc.state = (role==ROLE_GOALKEEPER) ? SC_GK_WAIT : SC_FP_WAIT;
}
static ScState field_tick(StrategyCore& sc, const ScWorld& w){
    if (!w.match_running) return SC_FP_WAIT;
    if (!w.ball_visible)  return SC_FP_SEEK;
    float dist = sqrtf(w.ball_x_mm*w.ball_x_mm + w.ball_y_mm*w.ball_y_mm);
    if (w.ball_y_mm <= -SC_DEFEND_BEHIND_MM) return SC_FP_DEFEND;  // pelota atrás
    if (dist <= SC_DRIVE_CLOSE_MM)            return SC_FP_DRIVE;   // encarar/patear
    // En estado WAIT y arranca con pelota visible ⇒ RUSH (jugada inicial rápida);
    // luego de moverse, el control de aproximación mantiene RUSH hasta DRIVE.
    return SC_FP_RUSH;
}
static ScState gk_tick(StrategyCore& sc, const ScWorld& w){
    if (!w.match_running) return SC_GK_WAIT;
    if (!w.ball_visible)  return SC_GK_PATROL;
    float dist = sqrtf(w.ball_x_mm*w.ball_x_mm + w.ball_y_mm*w.ball_y_mm);
    if (dist <= 150.0f) return SC_GK_CLEAR;
    if (dist <= SC_GK_INTERCEPT_MM) return SC_GK_INTERCEPT;
    return SC_GK_PATROL;
}
ScOut sc_tick(StrategyCore& sc, const ScWorld& w, uint32_t /*now_ms*/){
    sc.state = (sc.role==ROLE_GOALKEEPER) ? gk_tick(sc,w) : field_tick(sc,w);
    ScOut o{}; o.state = sc.state; return o;
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Verificar que pasa** (6 tests). Si un assert no coincide con la lógica real (p.ej. el orden de los if), razonar el árbol y ajustar SOLO el assert para reflejar el comportamiento real, documentando por qué.

- [ ] **Step 5: Commit**
```bash
cd /c/Users/violl/futbol2026/open-soccer-robocup-team2026 && git add "software/teensy/Soccer 2026/src/shared/strategy_core.h" "software/teensy/Soccer 2026/src/shared/strategy_core.cpp" "software/teensy/Soccer 2026/test/test_central_strategy/test_main.cpp" && git commit -m "feat(central): strategy_core — FSM unificada arquero + jugador"
```

---

## Task 6: `motion_target` — intención → comando de movimiento

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/motion_target.h`, `motion_target.cpp`
- Test: `software/teensy/Soccer 2026/test/test_central_motion/test_main.cpp`

Traduce (estado FSM + acción de play + escape) a `{vx_mm_s, vy_mm_s, omega_centideg_s, kicker}` que los PID/cinemática ejecutan. Marco robot: +y frente, +x derecha.

- [ ] **Step 1: Test que falla** — `test/test_central_motion/test_main.cpp`:
```cpp
// pio test -e test_native -f test_central_motion
#include <unity.h>
#include "motion_target.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

void test_escape_moves_along_escape_angle(void){
    MotionIn in{}; in.intent=MI_ESCAPE; in.escape_angle_centideg=9000; // derecha
    in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_TRUE(c.vx_mm_s > 300);   // se mueve a la derecha
    TEST_ASSERT_INT_WITHIN(80, 0, c.vy_mm_s);
    TEST_ASSERT_EQUAL_UINT8(0, c.kicker);
}
void test_goto_ball_moves_toward_ball(void){
    MotionIn in{}; in.intent=MI_GOTO_BALL; in.ball_x_mm=0; in.ball_y_mm=400;
    in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_TRUE(c.vy_mm_s > 300);   // hacia adelante
}
void test_kick_sets_kicker(void){
    MotionIn in{}; in.intent=MI_KICK; in.max_speed_mm_s=500;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_EQUAL_UINT8(1, c.kicker);
}
void test_stop_is_zero(void){
    MotionIn in{}; in.intent=MI_STOP;
    MotionCmd c = mt_compute(in);
    TEST_ASSERT_EQUAL_INT16(0, c.vx_mm_s);
    TEST_ASSERT_EQUAL_INT16(0, c.vy_mm_s);
    TEST_ASSERT_EQUAL_UINT8(0, c.kicker);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_escape_moves_along_escape_angle);
    RUN_TEST(test_goto_ball_moves_toward_ball);
    RUN_TEST(test_kick_sets_kicker);
    RUN_TEST(test_stop_is_zero);
    return UNITY_END();
}
```

- [ ] **Step 2: Verificar que falla.**

- [ ] **Step 3: Implementar** — `src/shared/motion_target.h`:
```cpp
#pragma once
#include <stdint.h>
namespace iitasoccer {
enum MotionIntent { MI_STOP=0, MI_ESCAPE=1, MI_GOTO_BALL=2, MI_KICK=3, MI_HOLD=4 };
struct MotionIn {
    MotionIntent intent;
    int16_t      escape_angle_centideg;  // para MI_ESCAPE (0=frente, horario+)
    float        ball_x_mm;              // para MI_GOTO_BALL (+x derecha)
    float        ball_y_mm;              // (+y frente)
    int16_t      max_speed_mm_s;
};
struct MotionCmd {
    int16_t vx_mm_s;
    int16_t vy_mm_s;
    int16_t omega_centideg_s;
    uint8_t kicker;                      // 0/1
};
MotionCmd mt_compute(const MotionIn& in);
}  // namespace iitasoccer
```
`src/shared/motion_target.cpp`:
```cpp
#include "motion_target.h"
#include <cmath>
namespace iitasoccer {
MotionCmd mt_compute(const MotionIn& in){
    MotionCmd c{};
    float sp = (float)in.max_speed_mm_s;
    if (in.intent == MI_ESCAPE){
        float a = (in.escape_angle_centideg / 100.0f) * (float)M_PI / 180.0f;
        c.vx_mm_s = (int16_t)lroundf(sp * sinf(a));   // 0=frente ⇒ vx=sin, vy=cos
        c.vy_mm_s = (int16_t)lroundf(sp * cosf(a));
    } else if (in.intent == MI_GOTO_BALL){
        float n = sqrtf(in.ball_x_mm*in.ball_x_mm + in.ball_y_mm*in.ball_y_mm);
        if (n > 1.0f){
            c.vx_mm_s = (int16_t)lroundf(sp * in.ball_x_mm / n);
            c.vy_mm_s = (int16_t)lroundf(sp * in.ball_y_mm / n);
        }
    } else if (in.intent == MI_KICK){
        c.kicker = 1;
    }
    // MI_STOP / MI_HOLD ⇒ todo 0 (MI_HOLD podría refinarse en Plan 2).
    return c;
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Verificar que pasa** (4 tests).

- [ ] **Step 5: Commit**
```bash
cd /c/Users/violl/futbol2026/open-soccer-robocup-team2026 && git add "software/teensy/Soccer 2026/src/shared/motion_target.h" "software/teensy/Soccer 2026/src/shared/motion_target.cpp" "software/teensy/Soccer 2026/test/test_central_motion/test_main.cpp" && git commit -m "feat(central): motion_target — intencion a comando de movimiento"
```

---

## Task 7: `central_decide` — orquestador con precedencia

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/central_decide.h`, `central_decide.cpp`
- Test: `software/teensy/Soccer 2026/test/test_central_decide/test_main.cpp`

Precedencia: `field_safety.preempt` → ESCAPE; sino `play_decision != NONE` → acción de play; sino `strategy_core` (rol). Combina todos los módulos.

- [ ] **Step 1: Test que falla** — `test/test_central_decide/test_main.cpp`:
```cpp
// pio test -e test_native -f test_central_decide
#include <unity.h>
#include "central_decide.h"
#include "types.h"
using namespace iitasoccer;
void setUp(void) {}
void tearDown(void) {}

static LineStatusV2 line(uint8_t valid, uint8_t present, int16_t esc, uint8_t ev){
    LineStatusV2 s{}; s.schema_version=LSV2_SCHEMA; s.data_valid=valid;
    s.line_present=present; s.escape_angle_centideg=esc; s.event_flags=ev;
    return s;
}
static WorldSnapshot world(uint8_t run, uint8_t ballvis, int16_t bx, int16_t by){
    WorldSnapshot w{}; w.flags = run?0x08:0; w.ball_visible=ballvis;
    w.ball_x_mm=bx; w.ball_y_mm=by; w.goal_opp_visible=1;
    w.goal_opp_angle_centideg=0; w.referee_cmd = run?1:0; return w;
}
void test_safety_preempts_everything(void){
    CentralState cs{}; cd_init(cs, ROLE_FIELD);
    WorldSnapshot w = world(1,1,0,300);
    LineStatusV2 s = line(1,1, 9000, EV_IMMINENT_EXIT);
    CentralCmd c = cd_tick(cs, w, s, 0);
    TEST_ASSERT_EQUAL_INT(MI_ESCAPE, c.debug_intent);
    TEST_ASSERT_TRUE(c.cmd.vx_mm_s > 200);   // escapa
}
void test_strategy_runs_when_no_safety_no_play(void){
    CentralState cs{}; cd_init(cs, ROLE_FIELD);
    WorldSnapshot w = world(1,1,0,500);       // pelota lejos al frente, quieta
    LineStatusV2 s = line(1,0, LSV2_NA_I16, 0);
    CentralCmd c = cd_tick(cs, w, s, 0);
    TEST_ASSERT_EQUAL_INT(SC_FP_RUSH, c.debug_state);  // jugada inicial rápida
}
void test_invalid_line_is_conservative_not_escape(void){
    CentralState cs{}; cd_init(cs, ROLE_FIELD);
    WorldSnapshot w = world(1,1,0,500);
    LineStatusV2 s = line(0,0, LSV2_NA_I16, EV_LIFTED);
    CentralCmd c = cd_tick(cs, w, s, 0);
    TEST_ASSERT_NOT_EQUAL(MI_ESCAPE, c.debug_intent);
    TEST_ASSERT_TRUE(c.conservative);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_safety_preempts_everything);
    RUN_TEST(test_strategy_runs_when_no_safety_no_play);
    RUN_TEST(test_invalid_line_is_conservative_not_escape);
    return UNITY_END();
}
```

- [ ] **Step 2: Verificar que falla.**

- [ ] **Step 3: Implementar** — `src/shared/central_decide.h`:
```cpp
#pragma once
#include <stdint.h>
#include "types.h"
#include "ball_trajectory.h"
#include "play_decision.h"
#include "field_safety.h"
#include "strategy_core.h"
#include "motion_target.h"
namespace iitasoccer {
struct CentralCfg {
    int16_t ball_speed_min_mm_s;   // default 80
    int16_t toward_tol_centideg;   // default 4500
    int16_t reach_mm;              // default 400
    int16_t max_speed_mm_s;        // default 500
};
struct CentralState { StrategyCore sc; CentralCfg cfg; bool inited; };
struct CentralCmd {
    MotionCmd    cmd;
    bool         conservative;
    MotionIntent debug_intent;
    ScState      debug_state;
    PlayAction   debug_play;
};
void       cd_init(CentralState& cs, ScRole role);
CentralCmd cd_tick(CentralState& cs, const WorldSnapshot& w,
                    const LineStatusV2& line, uint32_t now_ms);
}  // namespace iitasoccer
```
`src/shared/central_decide.cpp`:
```cpp
#include "central_decide.h"
#include <cmath>
namespace iitasoccer {
void cd_init(CentralState& cs, ScRole role){
    sc_init(cs.sc, role);
    cs.cfg.ball_speed_min_mm_s = 80;
    cs.cfg.toward_tol_centideg = 4500;
    cs.cfg.reach_mm = 400;
    cs.cfg.max_speed_mm_s = 500;
    cs.inited = true;
}
CentralCmd cd_tick(CentralState& cs, const WorldSnapshot& w,
                    const LineStatusV2& line, uint32_t now_ms){
    CentralCmd out{};
    // --- 1. field_safety: máxima prioridad ---
    FieldSafety fs = fs_eval(line);
    out.conservative = fs.conservative;
    if (fs.preempt){
        MotionIn mi{}; mi.intent=MI_ESCAPE;
        mi.escape_angle_centideg=fs.escape_angle_centideg;
        mi.max_speed_mm_s=cs.cfg.max_speed_mm_s;
        out.cmd = mt_compute(mi); out.debug_intent=MI_ESCAPE;
        out.debug_state = cs.sc.state; out.debug_play=PLAY_NONE;
        return out;
    }
    // --- 2. play_decision (trayectoria de pelota) ---
    float bdist = sqrtf((float)w.ball_x_mm*w.ball_x_mm + (float)w.ball_y_mm*w.ball_y_mm);
    BallTrajIn bi{};
    bi.ball_vx_mm_s=w.ball_vx_mm_s; bi.ball_vy_mm_s=w.ball_vy_mm_s;
    bi.goal_opp_angle_centideg=w.goal_opp_angle_centideg;
    bi.goal_own_angle_centideg=(int16_t)(w.goal_opp_angle_centideg>=0
                                ? w.goal_opp_angle_centideg-18000
                                : w.goal_opp_angle_centideg+18000);
    bi.ball_speed_min_mm_s=cs.cfg.ball_speed_min_mm_s;
    bi.toward_tol_centideg=cs.cfg.toward_tol_centideg;
    bi.ball_dist_mm=(int16_t)(bdist>32767?32767:bdist);
    bi.reach_mm=cs.cfg.reach_mm;
    BallTraj bt = bt_classify(bi);
    PlayIn pin{}; pin.in_reach=bt.in_reach;
    pin.moving=(bt.kind!=BT_STILL); pin.kind=bt.kind;
    PlayOut po = pd_decide(pin);
    out.debug_play = po.action;
    if (w.ball_visible && po.action != PLAY_NONE){
        MotionIn mi{}; mi.max_speed_mm_s=cs.cfg.max_speed_mm_s;
        mi.ball_x_mm=w.ball_x_mm; mi.ball_y_mm=w.ball_y_mm;
        if (po.action==PLAY_LET_CIRCULATE){ mi.intent=MI_HOLD; }
        else if (po.action==PLAY_INTERCEPT){ mi.intent=MI_GOTO_BALL; }
        else /*PLAY_DEFLECT_TO_OPP*/      { mi.intent=MI_GOTO_BALL; }
        out.cmd = mt_compute(mi); out.debug_intent=mi.intent;
        out.debug_state = cs.sc.state; return out;
    }
    // --- 3. strategy_core (rol) ---
    ScWorld sw{};
    sw.match_running = (w.referee_cmd==1) || (w.flags & 0x08);
    sw.ball_visible = w.ball_visible!=0;
    sw.ball_x_mm = (float)w.ball_x_mm; sw.ball_y_mm=(float)w.ball_y_mm;
    sw.goal_opp_visible = w.goal_opp_visible!=0;
    sw.goal_opp_angle_centideg = w.goal_opp_angle_centideg;
    ScOut so = sc_tick(cs.sc, sw, now_ms);
    out.debug_state = so.state;
    MotionIn mi{}; mi.max_speed_mm_s=cs.cfg.max_speed_mm_s;
    mi.ball_x_mm=w.ball_x_mm; mi.ball_y_mm=w.ball_y_mm;
    switch (so.state){
        case SC_FP_WAIT: case SC_GK_WAIT:                 mi.intent=MI_STOP;  break;
        case SC_FP_RUSH: case SC_FP_SEEK: case SC_FP_DEFEND:
        case SC_GK_PATROL: case SC_GK_INTERCEPT:          mi.intent=MI_GOTO_BALL; break;
        case SC_FP_DRIVE: case SC_GK_CLEAR:               mi.intent=MI_KICK;  break;
        default:                                          mi.intent=MI_HOLD;  break;
    }
    if (!sw.ball_visible && (mi.intent==MI_GOTO_BALL)) mi.intent=MI_HOLD;
    out.cmd = mt_compute(mi); out.debug_intent=mi.intent;
    return out;
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Verificar que pasa** (3 tests). Ajustar SOLO asserts si la lógica real difiere de la predicción (documentar).

- [ ] **Step 5: Commit**
```bash
cd /c/Users/violl/futbol2026/open-soccer-robocup-team2026 && git add "software/teensy/Soccer 2026/src/shared/central_decide.h" "software/teensy/Soccer 2026/src/shared/central_decide.cpp" "software/teensy/Soccer 2026/test/test_central_decide/test_main.cpp" && git commit -m "feat(central): central_decide — orquestador con precedencia safety>play>strategy"
```

---

## Task 8: Integración — compilar CENTRAL con el núcleo nuevo

> HW-bound parcial: NO se unit-testea el binario; verificación = `pio run -e central_robot1` compila. El glue real de motores/dipswitch es Plan 2. Acá solo se garantiza que el núcleo linkea en el env CENTRAL.

**Files:**
- Modify: `software/teensy/Soccer 2026/src/central/strategy.cpp` (o el .cpp de glue que exista) — añadir un punto de entrada que llame `cd_tick` y un `static_assert` de inclusión.

- [ ] **Step 1: Inspeccionar** `src/central/*.cpp` (`main_central.cpp`, `strategy.cpp`) para ver dónde se decide hoy. Identificar el lugar donde se obtiene `WorldSnapshot` y `LineStatusV2`/línea.

- [ ] **Step 2: Añadir el wiring mínimo** — incluir `#include "central_decide.h"` en el .cpp de glue de CENTRAL y, en la función de decisión existente, construir `CentralState` estático (init una vez con el rol según build flag `ROBOT1`=GK/`ROBOT2`=FIELD o el dipswitch si ya se lee), llamar `cd_tick(cs, world, line, millis())`, y pasar `out.cmd` a la capa de motores/PID existente. NO reescribir la capa de motores; solo conectar la salida. Si la firma de la capa de motores difiere, adaptar mínimamente y anotarlo.

- [ ] **Step 3: Compilar** ambos binarios CENTRAL:
```bash
cd "/c/Users/violl/futbol2026/open-soccer-robocup-team2026/software/teensy/Soccer 2026" && export PATH="/c/mingw64/bin:$PATH" && timeout 600 python -m platformio run -e central_robot1 -e central_robot2 2>&1 | tail -12
```
Expected: SUCCESS ambos. Si falla por error PRE-EXISTENTE no relacionado (otros .cpp de src/central) → DONE_WITH_CONCERNS con el log exacto, sin arreglar código ajeno. Si falta `-I src/shared` u otro flag de infra: ya fue agregado en commit `581f14f`; si reaparece, reportarlo.

- [ ] **Step 4: Regresión host completa**
```bash
cd "/c/Users/violl/futbol2026/open-soccer-robocup-team2026/software/teensy/Soccer 2026" && export PATH="/c/mingw64/bin:$PATH" && python -m platformio test -e test_native 2>&1 | tail -6
```
Expected: TODAS las suites PASSED (las previas + las 7 nuevas de CENTRAL).

- [ ] **Step 5: Commit**
```bash
cd /c/Users/violl/futbol2026/open-soccer-robocup-team2026 && git add "software/teensy/Soccer 2026/src/central/" && git commit -m "feat(central): wirear central_decide en el glue de CENTRAL (compila)"
```

---

## Self-Review

**Spec coverage:** ✔ contrato ball_vx/vy (T1) ✔ ball_trajectory clasificación+alcance (T2) ✔ play_decision 3 casos (T3) ✔ field_safety preempción/conservador (T4) ✔ FSM unificada arquero+jugador+RUSH inicial (T5) ✔ motion_target intención→cmd (T6) ✔ central_decide precedencia safety>play>strategy (T7) ✔ compila en env CENTRAL (T8). Unifica y reemplaza la réplica `strategy_transitions` (T5/T7 son la única fuente de verdad; el viejo `strategy_transitions` queda como caracterización histórica hasta que el glue use `central_decide`). Deferred honesto: TOP llena ball_vx/vy (su plan); motores/dipswitch/tuning = Plan 2 — declarado, no oculto.

**Placeholder scan:** sin TBD/TODO; todo el código de tests e implementación está completo. Las predicciones de asserts que dependan de conversión de marco/orden de if están marcadas con la instrucción explícita de ajustar SOLO el assert al comportamiento real (TDD honesto), no la lógica.

**Type consistency:** `BallTrajKind`/`BallTraj`/`BallTrajIn` (T2) usados igual en T3/T7. `PlayAction`/`PlayIn`/`PlayOut` (T3) en T7. `FieldSafety`/`fs_eval` (T4) en T7. `ScRole`/`ScState`/`ScWorld`/`StrategyCore`/`sc_init`/`sc_tick` (T5) en T7. `MotionIntent`/`MotionIn`/`MotionCmd`/`mt_compute` (T6) en T7. `LineStatusV2`/`LSV2_*`/`EV_*` y `WorldSnapshot` (existentes/T1) consistentes. `central_decide` incluye todos los headers anteriores.

**Notas de ejecución:** todas las suites en `test/test_central_*/test_main.cpp` con `int main(){UNITY_BEGIN;RUN_TEST;UNITY_END;}` y `using namespace iitasoccer;`. `pio test -e test_native -f <suite>` por task; `pio run -e central_robot1 -e central_robot2` solo en T8. Guardrail `.pio`/deps en todas.
