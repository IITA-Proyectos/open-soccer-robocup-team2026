# DOWN Line-Sensing Core — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convertir el lado de sensado de línea de la placa DOWN en un candidato real (sin stubs, testeado host-native, sin depender de OTOS) que emite el contrato `LineStatusV2` exacto a CENTRAL.

**Architecture:** Toda la lógica pura vive en `src/shared/` (única carpeta que compila el env `test_native`, `build_src_filter = +<shared/>`). Módulos chicos y aislados: geometría, calibración adaptativa, monitor de superficie, orquestador (`down_model`), encoder de contrato. El glue de hardware en `src/down/` queda como adaptador fino documentado. TDD estricto con Unity.

**Tech Stack:** C++17, PlatformIO, Unity (`pio test -e test_native -f <suite>`), structs `__attribute__((packed))`, protocolo `proto.h` (CRC-16/CCITT-FALSE).

**Scope / decomposición:** El programa DOWN completo se parte en 3 planes independientes y testeables. **Este es el Plan 1** (line-sensing core — cumple "funciona con o sin odometría"). Plan 2 = OTOS detrás de `IOdometrySource`+mock. Plan 3 = integración HW + `down_selftest` + bring-up. Cada plan produce software testeable por sí solo.

**Contrato de referencia (NO re-derivar, es la fuente de verdad):** `docs/firmware/CONTRATO-DATOS-DOWN.md` (LineStatusV2 16 B, convención de ángulos, sentinelas, ejemplos byte-a-byte con CRC real).

---

## File Structure

| Archivo | Responsabilidad | Acción |
|---|---|---|
| `software/teensy/Soccer 2026/src/shared/types.h` | Agregar `LineStatusV2` + máscaras + `static_assert` | Modify |
| `software/teensy/Soccer 2026/src/shared/line_geometry.h/.cpp` | Ángulo físico por sensor; `line_angle`/`escape_angle`; `sensors_on_line`; CORNER | Create |
| `software/teensy/Soccer 2026/src/shared/line_tracker.h/.cpp` | Evento `LINE_END` (estado entre ticks) | Create |
| `software/teensy/Soccer 2026/src/shared/line_calib.h/.cpp` | Calibración estática + adaptativa (gated por línea) + `CALIB_SUSPECT` | Create |
| `software/teensy/Soccer 2026/src/shared/surface_monitor.h/.cpp` | Detección "robot levantado" debounced + compuerta `data_valid` | Create |
| `software/teensy/Soccer 2026/src/shared/down_model.h/.cpp` | Orquesta filtros+calib+geo+superficie → `LineStatusV2` | Create |
| `software/teensy/Soccer 2026/src/shared/down_encode.h/.cpp` | `LineStatusV2` → frame `proto.h` (conforme al contrato) | Create |
| `test/test_down_geometry/test_main.cpp` | Tests geometría | Create |
| `test/test_down_tracker/test_main.cpp` | Tests LINE_END | Create |
| `test/test_down_calib/test_main.cpp` | Tests calibración | Create |
| `test/test_down_surface/test_main.cpp` | Tests lifted/data_valid | Create |
| `test/test_down_model/test_main.cpp` | Tests orquestador (escenarios del contrato) | Create |
| `test/test_down_encode/test_main.cpp` | Regresión contra los frames exactos del contrato §3.6 | Create |
| `software/teensy/Soccer 2026/src/down/comm_central.cpp` | Emitir `LineStatusV2` vía `down_model` (glue HW) | Modify |

Convención de ángulos (del contrato): 0° = frente (+Y), **positivo = horario visto desde arriba**, centidegrees, N/A = `INT16_MIN`.

---

## Task 1: `LineStatusV2` struct + constantes + static_assert

**Files:**
- Modify: `software/teensy/Soccer 2026/src/shared/types.h` (agregar al final, antes de `}  // namespace`)
- Test: `test/test_down_encode/test_main.cpp` (el static_assert se valida compilando; un test trivial fija sizeof)

- [ ] **Step 1: Escribir el test que falla**

Crear `test/test_down_encode/test_main.cpp`:
```cpp
// test_down_encode — corre con: pio test -e test_native -f test_down_encode
#include <unity.h>
#include <cstring>
#include "types.h"
using namespace iitasoccer;
void setUp(void){} void tearDown(void){}

void test_linestatusv2_is_16_bytes(void){
    TEST_ASSERT_EQUAL_UINT32(16, sizeof(LineStatusV2));
}
void test_linestatusv2_constants(void){
    TEST_ASSERT_EQUAL_UINT8(2, LSV2_SCHEMA);
    TEST_ASSERT_EQUAL_INT16(-32768, LSV2_NA_I16);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, LSV2_NA_U16);
    TEST_ASSERT_EQUAL_UINT8(0x01, EV_IMMINENT_EXIT);
    TEST_ASSERT_EQUAL_UINT8(0x04, EV_LINE_END);
    TEST_ASSERT_EQUAL_UINT8(0x08, EV_LIFTED);
}
int main(int, char**){ UNITY_BEGIN();
    RUN_TEST(test_linestatusv2_is_16_bytes);
    RUN_TEST(test_linestatusv2_constants);
    return UNITY_END(); }
```

- [ ] **Step 2: Correr el test y verificar que falla**

Run: `pio test -e test_native -f test_down_encode`
Expected: FAIL — `LineStatusV2` / `LSV2_SCHEMA` no declarados (compilación).

- [ ] **Step 3: Implementar en `types.h`**

Agregar antes de `}  // namespace iitasoccer` en `src/shared/types.h`:
```cpp
#include <climits>  // INT16_MIN

// Contrato DOWN→CENTRAL v2 — ver docs/firmware/CONTRATO-DATOS-DOWN.md
struct LineStatusV2 {
    uint8_t  schema_version;          // = LSV2_SCHEMA
    uint8_t  data_valid;              // 0/1 (compuerta maestra)
    int16_t  line_angle_centideg;     // N/A = LSV2_NA_I16
    int16_t  escape_angle_centideg;   // N/A = LSV2_NA_I16
    uint16_t penetration_mm;          // N/A = LSV2_NA_U16
    int16_t  cross_track_mm;          // N/A = LSV2_NA_I16
    uint8_t  line_present;            // 0/1 (con histéresis)
    uint8_t  sensors_on_line;         // 0..32
    uint8_t  event_flags;             // EV_* OR-eados
    uint8_t  quality;                 // 0..100
    uint8_t  sample_age_ms;           // 0..255
    uint8_t  reserved;                // = 0
} __attribute__((packed));
static_assert(sizeof(LineStatusV2) == 16, "LineStatusV2 debe ser 16 bytes (contrato)");

constexpr uint8_t  LSV2_SCHEMA = 2;
constexpr int16_t  LSV2_NA_I16 = INT16_MIN;   // -32768
constexpr uint16_t LSV2_NA_U16 = 0xFFFF;

constexpr uint8_t EV_IMMINENT_EXIT     = 0x01;
constexpr uint8_t EV_CORNER            = 0x02;
constexpr uint8_t EV_LINE_END          = 0x04;
constexpr uint8_t EV_LIFTED            = 0x08;
constexpr uint8_t EV_CALIB_SUSPECT     = 0x10;
constexpr uint8_t EV_MUX_DEAD          = 0x20;
constexpr uint8_t EV_DEGRADED_GEOMETRY = 0x40;
```

- [ ] **Step 4: Correr el test y verificar que pasa**

Run: `pio test -e test_native -f test_down_encode`
Expected: PASS (2 tests).

- [ ] **Step 5: Commit**

```bash
git add "software/teensy/Soccer 2026/src/shared/types.h" "test/test_down_encode/test_main.cpp"
git commit -m "feat(down): LineStatusV2 struct + constantes (contrato v2)"
```

---

## Task 2: `line_geometry` — ángulo de sensor, line_angle/escape, sensors_on_line

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/line_geometry.h`, `line_geometry.cpp`
- Test: `test/test_down_geometry/test_main.cpp`

- [ ] **Step 1: Escribir el test que falla**

`test/test_down_geometry/test_main.cpp`:
```cpp
// test_down_geometry — pio test -e test_native -f test_down_geometry
#include <unity.h>
#include <cmath>
#include "line_geometry.h"
using namespace iitasoccer;
void setUp(void){} void tearDown(void){}

// Ángulo físico: contrato 0° = frente (+Y), positivo = horario visto desde arriba.
void test_sensor_angle_front_is_zero(void){
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, lg_sensor_angle_deg(0, 8));
}
void test_sensor_angle_quarter_is_45_for_8(void){
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, lg_sensor_angle_deg(1, 8));
}
void test_no_line_when_none_white(void){
    bool w[8]={false,false,false,false,false,false,false,false};
    float a[8]; for(int i=0;i<8;++i)a[i]=lg_sensor_angle_deg(i,8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_FALSE(g.line_present);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, g.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(0, g.sensors_on_line);
}
void test_line_centroid_front(void){
    bool w[8]={true,false,false,false,false,false,false,false};
    float a[8]; for(int i=0;i<8;++i)a[i]=lg_sensor_angle_deg(i,8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_TRUE(g.line_present);
    TEST_ASSERT_INT16_WITHIN(50, 0, g.line_angle_centideg);     // ~0.00°
    TEST_ASSERT_EQUAL_UINT8(1, g.sensors_on_line);
    // escape ≈ opuesto (~180.00° → 18000 o -18000)
    TEST_ASSERT_TRUE(abs((int)g.escape_angle_centideg) > 17000);
}
int main(int,char**){ UNITY_BEGIN();
    RUN_TEST(test_sensor_angle_front_is_zero);
    RUN_TEST(test_sensor_angle_quarter_is_45_for_8);
    RUN_TEST(test_no_line_when_none_white);
    RUN_TEST(test_line_centroid_front);
    return UNITY_END(); }
```

- [ ] **Step 2: Correr y verificar que falla**

Run: `pio test -e test_native -f test_down_geometry`
Expected: FAIL — `line_geometry.h` no existe.

- [ ] **Step 3: Implementar**

`src/shared/line_geometry.h`:
```cpp
#pragma once
#include <stdint.h>
#include "types.h"
namespace iitasoccer {
struct GeomResult {
    bool    line_present;
    int16_t line_angle_centideg;     // LSV2_NA_I16 si no hay línea
    int16_t escape_angle_centideg;   // LSV2_NA_I16 si no hay línea
    uint8_t sensors_on_line;
    bool    corner;                  // (se completa en Task 3; aquí false)
};
// Ángulo físico del sensor i en un anillo de n: 0=frente, horario+, grados.
float lg_sensor_angle_deg(int i, int n);
// Centroide angular de los sensores en blanco. escape = opuesto al centroide.
GeomResult lg_compute(const bool* white, const float* sensor_angle_deg, int n);
}  // namespace iitasoccer
```
`src/shared/line_geometry.cpp`:
```cpp
#include "line_geometry.h"
#include <cmath>
namespace iitasoccer {
static int16_t to_cd(float deg){
    while(deg>180.0f)deg-=360.0f; while(deg<=-180.0f)deg+=360.0f;
    return (int16_t)lroundf(deg*100.0f);
}
float lg_sensor_angle_deg(int i, int n){
    if(n<=0)return 0.0f;
    float d = (360.0f*(float)i)/(float)n;     // 0=frente, sentido horario
    while(d>180.0f)d-=360.0f;
    return d;
}
GeomResult lg_compute(const bool* white, const float* ang, int n){
    GeomResult g{}; g.line_present=false; g.corner=false;
    g.line_angle_centideg=LSV2_NA_I16; g.escape_angle_centideg=LSV2_NA_I16;
    double sx=0.0, sy=0.0; int cnt=0;
    for(int i=0;i<n;++i){ if(white[i]){ ++cnt;
        double r=ang[i]*M_PI/180.0; sx+=cos(r); sy+=sin(r); } }
    g.sensors_on_line=(uint8_t)(cnt>255?255:cnt);
    if(cnt==0) return g;
    g.line_present=true;
    double a = atan2(sy,sx)*180.0/M_PI;
    g.line_angle_centideg = to_cd((float)a);
    g.escape_angle_centideg = to_cd((float)a + 180.0f);
    return g;
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Correr y verificar que pasa**

Run: `pio test -e test_native -f test_down_geometry`
Expected: PASS (4 tests).

- [ ] **Step 5: Commit**

```bash
git add "software/teensy/Soccer 2026/src/shared/line_geometry.h" "software/teensy/Soccer 2026/src/shared/line_geometry.cpp" "test/test_down_geometry/test_main.cpp"
git commit -m "feat(down): line_geometry — angulo de sensor + centroide + escape"
```

---

## Task 3: `line_geometry` — detección de CORNER

**Files:**
- Modify: `software/teensy/Soccer 2026/src/shared/line_geometry.cpp` (rellenar `g.corner`)
- Test: `test/test_down_geometry/test_main.cpp` (agregar casos)

- [ ] **Step 1: Agregar test que falla**

Añadir a `test_down_geometry/test_main.cpp` (y al `main()` los `RUN_TEST`):
```cpp
void test_corner_two_perpendicular_clusters(void){
    // Anillo 8 sensores. Blanco en frente (idx0, 0°) y derecha (idx2, 90°).
    bool w[8]={true,false,true,false,false,false,false,false};
    float a[8]; for(int i=0;i<8;++i)a[i]=lg_sensor_angle_deg(i,8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_TRUE(g.corner);
}
void test_not_corner_single_cluster(void){
    bool w[8]={true,true,false,false,false,false,false,false}; // 0° y 45° contiguos
    float a[8]; for(int i=0;i<8;++i)a[i]=lg_sensor_angle_deg(i,8);
    GeomResult g = lg_compute(w, a, 8);
    TEST_ASSERT_FALSE(g.corner);
}
```
Agregar en `main()`: `RUN_TEST(test_corner_two_perpendicular_clusters); RUN_TEST(test_not_corner_single_cluster);`

- [ ] **Step 2: Correr y verificar que falla**

Run: `pio test -e test_native -f test_down_geometry`
Expected: FAIL en `test_corner_two_perpendicular_clusters` (g.corner siempre false).

- [ ] **Step 3: Implementar CORNER**

En `line_geometry.cpp`, antes de `return g;` final de `lg_compute`, insertar:
```cpp
    // CORNER: ≥2 clusters de blancos separados, con separación angular ~90°±35°.
    // Cluster = corrida de sensores blancos contiguos (anillo cerrado).
    {
        int first=-1; for(int i=0;i<n;++i){ if(!white[i]){first=i;break;} }
        int clusters=0; double cmean[8]; int cn=0; // hasta 8 clusters
        if(first<0){ /* todos blancos: una sola superficie, no corner */ }
        else {
            bool inrun=false; double sxx=0,syy=0;
            for(int k=0;k<n;++k){ int i=(first+k)%n;
                if(white[i]){ if(!inrun){inrun=true;sxx=0;syy=0;}
                    double r=ang[i]*M_PI/180.0; sxx+=cos(r); syy+=sin(r); }
                else if(inrun){ inrun=false; if(cn<8){
                    cmean[cn++]=atan2(syy,sxx)*180.0/M_PI; clusters++; } }
            }
            if(inrun && cn<8){ cmean[cn++]=atan2(syy,sxx)*180.0/M_PI; clusters++; }
        }
        if(clusters>=2){
            for(int i=0;i<cn && !g.corner;++i) for(int j=i+1;j<cn;++j){
                double d=fabs(cmean[i]-cmean[j]); if(d>180.0)d=360.0-d;
                if(d>=55.0 && d<=125.0){ g.corner=true; break; }
            }
        }
    }
```

- [ ] **Step 4: Correr y verificar que pasa**

Run: `pio test -e test_native -f test_down_geometry`
Expected: PASS (6 tests).

- [ ] **Step 5: Commit**

```bash
git add "software/teensy/Soccer 2026/src/shared/line_geometry.cpp" "test/test_down_geometry/test_main.cpp"
git commit -m "feat(down): deteccion de CORNER (2 clusters ~perpendiculares)"
```

---

## Task 4: `line_tracker` — evento LINE_END

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/line_tracker.h`, `line_tracker.cpp`
- Test: `test/test_down_tracker/test_main.cpp`

- [ ] **Step 1: Test que falla**

`test/test_down_tracker/test_main.cpp`:
```cpp
// pio test -e test_native -f test_down_tracker
#include <unity.h>
#include "line_tracker.h"
using namespace iitasoccer;
void setUp(void){} void tearDown(void){}

void test_line_end_fires_after_sustained_then_lost(void){
    LineTracker t{};
    // presente sostenido ≥ 200 ms
    TEST_ASSERT_FALSE(lt_update(t,true,0,200));
    TEST_ASSERT_FALSE(lt_update(t,true,150,200));
    TEST_ASSERT_FALSE(lt_update(t,true,250,200));     // ya >200 ms presente
    // se pierde → LINE_END
    TEST_ASSERT_TRUE (lt_update(t,false,260,200));
    // no se repite mientras siga ausente
    TEST_ASSERT_FALSE(lt_update(t,false,300,200));
}
void test_no_line_end_if_blip_too_short(void){
    LineTracker t{};
    TEST_ASSERT_FALSE(lt_update(t,true,0,200));
    TEST_ASSERT_FALSE(lt_update(t,false,50,200));     // presente solo 50 ms
}
int main(int,char**){ UNITY_BEGIN();
    RUN_TEST(test_line_end_fires_after_sustained_then_lost);
    RUN_TEST(test_no_line_end_if_blip_too_short);
    return UNITY_END(); }
```

- [ ] **Step 2: Correr y verificar que falla**

Run: `pio test -e test_native -f test_down_tracker`
Expected: FAIL — `line_tracker.h` no existe.

- [ ] **Step 3: Implementar**

`src/shared/line_tracker.h`:
```cpp
#pragma once
#include <stdint.h>
namespace iitasoccer {
struct LineTracker {
    bool     present_prev;
    bool     had_sustained;
    uint32_t present_since_ms;
    bool     since_valid;
};
// Devuelve true UNA vez en la transición presente→ausente,
// si la línea estuvo presente de forma continua ≥ min_track_ms.
bool lt_update(LineTracker& t, bool present_now, uint32_t now_ms,
                uint32_t min_track_ms);
}  // namespace iitasoccer
```
`src/shared/line_tracker.cpp`:
```cpp
#include "line_tracker.h"
namespace iitasoccer {
bool lt_update(LineTracker& t, bool present, uint32_t now, uint32_t min_ms){
    bool fired=false;
    if(present){
        if(!t.since_valid){ t.present_since_ms=now; t.since_valid=true; }
        if(now - t.present_since_ms >= min_ms) t.had_sustained=true;
    } else {
        if(t.present_prev && t.had_sustained) fired=true;
        t.had_sustained=false; t.since_valid=false;
    }
    t.present_prev=present;
    return fired;
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Correr y verificar que pasa**

Run: `pio test -e test_native -f test_down_tracker`
Expected: PASS (2 tests).

- [ ] **Step 5: Commit**

```bash
git add "software/teensy/Soccer 2026/src/shared/line_tracker.h" "software/teensy/Soccer 2026/src/shared/line_tracker.cpp" "test/test_down_tracker/test_main.cpp"
git commit -m "feat(down): line_tracker — evento LINE_END (fin de linea de area)"
```

---

## Task 5: `line_calib` — calibración estática + adaptativa + CALIB_SUSPECT

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/line_calib.h`, `line_calib.cpp`
- Test: `test/test_down_calib/test_main.cpp`

- [ ] **Step 1: Test que falla**

`test/test_down_calib/test_main.cpp`:
```cpp
// pio test -e test_native -f test_down_calib
#include <unity.h>
#include "line_calib.h"
using namespace iitasoccer;
void setUp(void){} void tearDown(void){}

void test_static_threshold_is_midpoint(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    TEST_ASSERT_EQUAL_UINT16(500, c.threshold);
}
void test_adapt_carpet_moves_toward_reading_when_off_line(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    // sensor NO sobre línea, luz subió: lectura carpet 260 sostenida
    for(int i=0;i<200;++i) lc_adapt_carpet(c, 260, /*on_line=*/false, 0.05f);
    TEST_ASSERT_UINT16_WITHIN(15, 260, c.carpet);
    TEST_ASSERT_UINT16_WITHIN(15, (260+800)/2, c.threshold);
}
void test_adapt_does_not_drift_when_on_line(void){
    SensorCalib c{}; lc_set_static(c, 200, 800);
    for(int i=0;i<200;++i) lc_adapt_carpet(c, 790, /*on_line=*/true, 0.05f);
    TEST_ASSERT_EQUAL_UINT16(200, c.carpet);   // congelado sobre blanco
}
void test_suspect_when_margin_too_small(void){
    SensorCalib cs[2]; lc_set_static(cs[0],400,900); lc_set_static(cs[1],500,560);
    TEST_ASSERT_TRUE(lc_is_suspect(cs,2,/*min_margin=*/100)); // cs[1]=60<100
    SensorCalib ok[1]; lc_set_static(ok[0],200,800);
    TEST_ASSERT_FALSE(lc_is_suspect(ok,1,100));
}
int main(int,char**){ UNITY_BEGIN();
    RUN_TEST(test_static_threshold_is_midpoint);
    RUN_TEST(test_adapt_carpet_moves_toward_reading_when_off_line);
    RUN_TEST(test_adapt_does_not_drift_when_on_line);
    RUN_TEST(test_suspect_when_margin_too_small);
    return UNITY_END(); }
```

- [ ] **Step 2: Correr y verificar que falla**

Run: `pio test -e test_native -f test_down_calib`
Expected: FAIL — `line_calib.h` no existe.

- [ ] **Step 3: Implementar**

`src/shared/line_calib.h`:
```cpp
#pragma once
#include <stdint.h>
namespace iitasoccer {
struct SensorCalib { uint16_t carpet; uint16_t white; uint16_t threshold; };
// Calibración estática: umbral = punto medio carpet/white.
void lc_set_static(SensorCalib& c, uint16_t carpet, uint16_t white);
// Adaptación on-the-fly del baseline de carpet. Solo adapta si el sensor NO
// está sobre línea (gate anti-deriva). alpha ∈ (0,1] = velocidad de adaptación.
void lc_adapt_carpet(SensorCalib& c, uint16_t filtered, bool sensor_on_line,
                      float alpha);
// true si algún sensor no separa piso/blanco (|white-carpet| < min_margin).
bool lc_is_suspect(const SensorCalib* cs, int n, uint16_t min_margin);
}  // namespace iitasoccer
```
`src/shared/line_calib.cpp`:
```cpp
#include "line_calib.h"
#include <cmath>
namespace iitasoccer {
static uint16_t mid(uint16_t a, uint16_t b){ return (uint16_t)(((uint32_t)a+b)/2); }
void lc_set_static(SensorCalib& c, uint16_t carpet, uint16_t white){
    c.carpet=carpet; c.white=white; c.threshold=mid(carpet,white);
}
void lc_adapt_carpet(SensorCalib& c, uint16_t filtered, bool on_line, float alpha){
    if(on_line) return;                         // no adaptar sobre blanco
    if(alpha<=0.0f) return; if(alpha>1.0f) alpha=1.0f;
    float nc = (1.0f-alpha)*(float)c.carpet + alpha*(float)filtered;
    c.carpet = (uint16_t)lroundf(nc);
    c.threshold = mid(c.carpet, c.white);
}
bool lc_is_suspect(const SensorCalib* cs, int n, uint16_t min_margin){
    for(int i=0;i<n;++i){
        int m = (int)cs[i].white - (int)cs[i].carpet; if(m<0)m=-m;
        if(m < (int)min_margin) return true;
    }
    return false;
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Correr y verificar que pasa**

Run: `pio test -e test_native -f test_down_calib`
Expected: PASS (4 tests).

- [ ] **Step 5: Commit**

```bash
git add "software/teensy/Soccer 2026/src/shared/line_calib.h" "software/teensy/Soccer 2026/src/shared/line_calib.cpp" "test/test_down_calib/test_main.cpp"
git commit -m "feat(down): line_calib — calib estatica + adaptativa + CALIB_SUSPECT"
```

---

## Task 6: `surface_monitor` — robot levantado + compuerta data_valid

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/surface_monitor.h`, `surface_monitor.cpp`
- Test: `test/test_down_surface/test_main.cpp`

- [ ] **Step 1: Test que falla**

`test/test_down_surface/test_main.cpp`:
```cpp
// pio test -e test_native -f test_down_surface
#include <unity.h>
#include "surface_monitor.h"
using namespace iitasoccer;
void setUp(void){} void tearDown(void){}

void test_lifted_requires_debounce(void){
    SurfaceMonitor s{};
    // mayoría de sensores muy por debajo de carpet → candidato
    uint16_t filt[8]={10,10,10,10,10,10,10,10};
    uint16_t carp[8]={200,200,200,200,200,200,200,200};
    TEST_ASSERT_FALSE(sm_update(s,filt,carp,8,0,  /*debounce*/100,7,50));
    TEST_ASSERT_FALSE(sm_update(s,filt,carp,8,50, 100,7,50));
    TEST_ASSERT_TRUE (sm_update(s,filt,carp,8,120,100,7,50)); // >100 ms sostenido
}
void test_not_lifted_when_on_floor(void){
    SurfaceMonitor s{};
    uint16_t filt[8]={210,205,200,198,202,207,201,203};
    uint16_t carp[8]={200,200,200,200,200,200,200,200};
    TEST_ASSERT_FALSE(sm_update(s,filt,carp,8,0,100,7,50));
    TEST_ASSERT_FALSE(sm_update(s,filt,carp,8,500,100,7,50));
}
void test_data_valid_gate(void){
    TEST_ASSERT_EQUAL_UINT8(0, sm_data_valid(true,false));   // lifted
    TEST_ASSERT_EQUAL_UINT8(0, sm_data_valid(false,true));   // calib suspect
    TEST_ASSERT_EQUAL_UINT8(1, sm_data_valid(false,false));
}
int main(int,char**){ UNITY_BEGIN();
    RUN_TEST(test_lifted_requires_debounce);
    RUN_TEST(test_not_lifted_when_on_floor);
    RUN_TEST(test_data_valid_gate);
    return UNITY_END(); }
```

- [ ] **Step 2: Correr y verificar que falla**

Run: `pio test -e test_native -f test_down_surface`
Expected: FAIL — `surface_monitor.h` no existe.

- [ ] **Step 3: Implementar**

`src/shared/surface_monitor.h`:
```cpp
#pragma once
#include <stdint.h>
namespace iitasoccer {
struct SurfaceMonitor { bool cand; uint32_t cand_since_ms; };
// Detecta robot levantado: ≥ min_sensors leen filtered < carpet - delta_below
// de forma sostenida ≥ debounce_ms. Devuelve true mientras "levantado".
bool sm_update(SurfaceMonitor& s, const uint16_t* filtered,
                const uint16_t* carpet, int n, uint32_t now_ms,
                uint32_t debounce_ms, int min_sensors, uint16_t delta_below);
// Compuerta maestra del contrato: 1 solo si NO lifted y NO calib suspect.
uint8_t sm_data_valid(bool lifted, bool calib_suspect);
}  // namespace iitasoccer
```
`src/shared/surface_monitor.cpp`:
```cpp
#include "surface_monitor.h"
namespace iitasoccer {
bool sm_update(SurfaceMonitor& s, const uint16_t* f, const uint16_t* c, int n,
                uint32_t now, uint32_t debounce, int min_s, uint16_t db){
    int below=0;
    for(int i=0;i<n;++i){ if((int)f[i] < (int)c[i] - (int)db) ++below; }
    bool cand = (below >= min_s);
    if(cand){
        if(!s.cand){ s.cand=true; s.cand_since_ms=now; }
        return (now - s.cand_since_ms) >= debounce;
    }
    s.cand=false;
    return false;
}
uint8_t sm_data_valid(bool lifted, bool suspect){
    return (lifted || suspect) ? 0 : 1;
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Correr y verificar que pasa**

Run: `pio test -e test_native -f test_down_surface`
Expected: PASS (3 tests).

- [ ] **Step 5: Commit**

```bash
git add "software/teensy/Soccer 2026/src/shared/surface_monitor.h" "software/teensy/Soccer 2026/src/shared/surface_monitor.cpp" "test/test_down_surface/test_main.cpp"
git commit -m "feat(down): surface_monitor — lifted debounced + compuerta data_valid"
```

---

## Task 7: `down_model` — orquestador → LineStatusV2

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/down_model.h`, `down_model.cpp`
- Test: `test/test_down_model/test_main.cpp`

- [ ] **Step 1: Test que falla**

`test/test_down_model/test_main.cpp`:
```cpp
// pio test -e test_native -f test_down_model
#include <unity.h>
#include "down_model.h"
using namespace iitasoccer;
void setUp(void){} void tearDown(void){}

static void mkcfg(DownModelCfg& c){
    c.imminent_depth=6; c.adapt_alpha=0.02f; c.calib_min_margin=120;
    c.lifted_debounce_ms=100; c.lifted_min_sensors=7; c.lifted_delta_below=80;
    c.line_end_min_track_ms=200;
}
void test_no_line_carpet_valid(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[8]={205,198,202,201,199,203,200,204}; // todo carpet
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(2, s.schema_version);
    TEST_ASSERT_EQUAL_UINT8(1, s.data_valid);
    TEST_ASSERT_EQUAL_UINT8(0, s.line_present);
    TEST_ASSERT_EQUAL_INT16(LSV2_NA_I16, s.line_angle_centideg);
    TEST_ASSERT_EQUAL_UINT8(0, s.sensors_on_line);
}
void test_line_front_sets_fields(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[8]={850,830,210,200,205,202,198,206}; // blanco en 0 y 1
    LineStatusV2 s = dm_update(m,cfg,raw,8,1000);
    TEST_ASSERT_EQUAL_UINT8(1, s.line_present);
    TEST_ASSERT_TRUE(s.sensors_on_line >= 1);
    TEST_ASSERT_NOT_EQUAL(LSV2_NA_I16, s.line_angle_centideg);
}
void test_lifted_sets_invalid_and_flag(void){
    DownModel m{}; DownModelCfg cfg; mkcfg(cfg);
    for(int i=0;i<8;++i) lc_set_static(m.calib[i],200,800);
    uint16_t raw[8]={5,5,5,5,5,5,5,5}; // todos << carpet
    dm_update(m,cfg,raw,8,0);
    LineStatusV2 s = dm_update(m,cfg,raw,8,200); // >debounce
    TEST_ASSERT_EQUAL_UINT8(0, s.data_valid);
    TEST_ASSERT_TRUE(s.event_flags & EV_LIFTED);
}
int main(int,char**){ UNITY_BEGIN();
    RUN_TEST(test_no_line_carpet_valid);
    RUN_TEST(test_line_front_sets_fields);
    RUN_TEST(test_lifted_sets_invalid_and_flag);
    return UNITY_END(); }
```

- [ ] **Step 2: Correr y verificar que falla**

Run: `pio test -e test_native -f test_down_model`
Expected: FAIL — `down_model.h` no existe.

- [ ] **Step 3: Implementar**

`src/shared/down_model.h`:
```cpp
#pragma once
#include <stdint.h>
#include "types.h"
#include "line_filters.h"
#include "line_geometry.h"
#include "line_tracker.h"
#include "line_calib.h"
#include "surface_monitor.h"
namespace iitasoccer {
constexpr int DM_MAX_SENSORS = 32;
struct DownModelCfg {
    int      imminent_depth;        // sensors_on_line ≥ esto ⇒ IMMINENT_EXIT
    float    adapt_alpha;           // adaptación carpet
    uint16_t calib_min_margin;      // < ⇒ CALIB_SUSPECT
    uint32_t lifted_debounce_ms;
    int      lifted_min_sensors;
    uint16_t lifted_delta_below;
    uint32_t line_end_min_track_ms;
};
struct DownModel {
    SensorCalib    calib[DM_MAX_SENSORS];
    FilterBuffer   filt[DM_MAX_SENSORS];
    bool           was_white[DM_MAX_SENSORS];
    SurfaceMonitor surface;
    LineTracker    tracker;
    uint8_t        seq_sample;      // contador interno de muestra
};
// Procesa un set de lecturas crudas y produce el contrato LineStatusV2.
LineStatusV2 dm_update(DownModel& m, const DownModelCfg& cfg,
                        const uint16_t* raw, int n, uint32_t now_ms);
}  // namespace iitasoccer
```
`src/shared/down_model.cpp`:
```cpp
#include "down_model.h"
namespace iitasoccer {
LineStatusV2 dm_update(DownModel& m, const DownModelCfg& cfg,
                        const uint16_t* raw, int n, uint32_t now){
    if(n>DM_MAX_SENSORS) n=DM_MAX_SENSORS;
    uint16_t filt[DM_MAX_SENSORS]; bool white[DM_MAX_SENSORS];
    float ang[DM_MAX_SENSORS];
    for(int i=0;i<n;++i){
        filt[i]=lf_temporal_update(m.filt[i], raw[i]);
        white[i]=lf_hysteresis_on_white(filt[i], m.calib[i].threshold,
                                         m.was_white[i]);
        m.was_white[i]=white[i];
        ang[i]=lg_sensor_angle_deg(i,n);
    }
    bool validated[DM_MAX_SENSORS];
    lf_spatial_filter(white, validated, n);

    GeomResult g = lg_compute(validated, ang, n);

    // calib adaptativa (gated por validated)
    for(int i=0;i<n;++i)
        lc_adapt_carpet(m.calib[i], filt[i], validated[i], cfg.adapt_alpha);
    bool suspect = lc_is_suspect(m.calib, n, cfg.calib_min_margin);
    bool lifted  = sm_update(m.surface, filt, /*carpet*/nullptr_safe(m),
                             n, now, cfg.lifted_debounce_ms,
                             cfg.lifted_min_sensors, cfg.lifted_delta_below);
    bool line_end = lt_update(m.tracker, g.line_present, now,
                              cfg.line_end_min_track_ms);

    LineStatusV2 s{};
    s.schema_version = LSV2_SCHEMA;
    s.data_valid = sm_data_valid(lifted, suspect);
    s.line_present = g.line_present ? 1 : 0;
    s.sensors_on_line = g.sensors_on_line;
    if(g.line_present){
        s.line_angle_centideg   = g.line_angle_centideg;
        s.escape_angle_centideg = g.escape_angle_centideg;
        s.penetration_mm = (uint16_t)(g.sensors_on_line); // proxy hasta calib geom (Plan 3)
        s.cross_track_mm = LSV2_NA_I16;                    // requiere ref-edge (Plan 3)
    } else {
        s.line_angle_centideg=LSV2_NA_I16; s.escape_angle_centideg=LSV2_NA_I16;
        s.penetration_mm=LSV2_NA_U16; s.cross_track_mm=LSV2_NA_I16;
    }
    uint8_t ev=0;
    if(g.line_present && g.sensors_on_line>=cfg.imminent_depth) ev|=EV_IMMINENT_EXIT;
    if(g.corner)   ev|=EV_CORNER;
    if(line_end)   ev|=EV_LINE_END;
    if(lifted)     ev|=EV_LIFTED;
    if(suspect)    ev|=EV_CALIB_SUSPECT;
    if(n<32)       ev|=EV_DEGRADED_GEOMETRY;
    s.event_flags=ev;
    s.quality = s.data_valid ? (uint8_t)(g.line_present? 85 : 95) : 0;
    s.sample_age_ms = 0;          // lo setea el glue HW con el delta real
    s.reserved = 0;
    m.seq_sample++;
    return s;
}
}  // namespace iitasoccer
```
> Nota: reemplazar `nullptr_safe(m)` por un buffer `carpet[]` derivado de
> `m.calib[i].carpet`. Implementarlo inline:
```cpp
// dentro de dm_update, antes de sm_update:
uint16_t carpet[DM_MAX_SENSORS];
for(int i=0;i<n;++i) carpet[i]=m.calib[i].carpet;
// y llamar: sm_update(m.surface, filt, carpet, n, now, ...);
```
(Quitar `nullptr_safe`; usar `carpet`.)

- [ ] **Step 4: Correr y verificar que pasa**

Run: `pio test -e test_native -f test_down_model`
Expected: PASS (3 tests).

- [ ] **Step 5: Commit**

```bash
git add "software/teensy/Soccer 2026/src/shared/down_model.h" "software/teensy/Soccer 2026/src/shared/down_model.cpp" "test/test_down_model/test_main.cpp"
git commit -m "feat(down): down_model — orquestador a LineStatusV2"
```

---

## Task 8: `down_encode` — frame proto conforme al contrato (regresión byte-a-byte)

**Files:**
- Create: `software/teensy/Soccer 2026/src/shared/down_encode.h`, `down_encode.cpp`
- Modify: `test/test_down_encode/test_main.cpp` (agregar regresión contra el contrato)

- [ ] **Step 1: Agregar test que falla**

Añadir a `test/test_down_encode/test_main.cpp` (incluir `#include "down_encode.h"` y los structs):
```cpp
#include "down_encode.h"
// Ejemplo B del contrato §3.6 (SEQ=0x01): frame exacto esperado.
void test_encode_matches_contract_example_B(void){
    LineStatusV2 s{};
    s.schema_version=2; s.data_valid=1;
    s.line_angle_centideg=4500; s.escape_angle_centideg=-13500;
    s.penetration_mm=15; s.cross_track_mm=-8;
    s.line_present=1; s.sensors_on_line=4; s.event_flags=0;
    s.quality=88; s.sample_age_ms=1; s.reserved=0;
    uint8_t out[64];
    size_t n = down_encode_line(s, 0x01, out, sizeof(out));
    const uint8_t exp[] = {
      0xAA,0x10,0x10,0x01, 0x02,0x01,0x94,0x11,0x44,0xCB,0x0F,0x00,
      0xF8,0xFF,0x01,0x04,0x00,0x58,0x01,0x00, 0xDF,0xBF,0x55 };
    TEST_ASSERT_EQUAL_UINT32(sizeof(exp), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(exp, out, sizeof(exp));
}
```
Agregar en `main()`: `RUN_TEST(test_encode_matches_contract_example_B);`

- [ ] **Step 2: Correr y verificar que falla**

Run: `pio test -e test_native -f test_down_encode`
Expected: FAIL — `down_encode.h` no existe.

- [ ] **Step 3: Implementar**

`src/shared/down_encode.h`:
```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "types.h"
namespace iitasoccer {
// Serializa LineStatusV2 como frame proto.h (TYPE=LINE_URGENT=0x10).
// Devuelve bytes escritos, 0 si error. Conforme a docs/firmware/CONTRATO-DATOS-DOWN.md.
size_t down_encode_line(const LineStatusV2& s, uint8_t seq,
                         uint8_t* out, size_t out_size);
}  // namespace iitasoccer
```
`src/shared/down_encode.cpp`:
```cpp
#include "down_encode.h"
#include "proto.h"
#include <string.h>
namespace iitasoccer {
size_t down_encode_line(const LineStatusV2& s, uint8_t seq,
                         uint8_t* out, size_t out_size){
    Frame f{};
    f.type = MsgType::LINE_URGENT;
    f.seq = seq;
    f.payload_len = sizeof(LineStatusV2);   // 16
    memcpy(f.payload, &s, sizeof(LineStatusV2));
    return proto_encode(f, out, out_size);
}
}  // namespace iitasoccer
```

- [ ] **Step 4: Correr y verificar que pasa**

Run: `pio test -e test_native -f test_down_encode`
Expected: PASS — el frame generado coincide **byte a byte** con el ejemplo B del contrato (CRC 0xDFBF incluido).

- [ ] **Step 5: Commit**

```bash
git add "software/teensy/Soccer 2026/src/shared/down_encode.h" "software/teensy/Soccer 2026/src/shared/down_encode.cpp" "test/test_down_encode/test_main.cpp"
git commit -m "feat(down): down_encode — frame proto conforme al contrato (regresion byte-a-byte)"
```

---

## Task 9: Integración HW — emitir LineStatusV2 desde la placa DOWN

> **HW-bound: no se unit-testea en host.** Cambio de glue acotado + checklist de
> bring-up. La lógica ya está testeada (Tasks 2-8).

**Files:**
- Modify: `software/teensy/Soccer 2026/src/down/comm_central.cpp`

- [ ] **Step 1: Cablear `down_model` en el envío a CENTRAL**

Reemplazar el cuerpo de `comm_central_send_line_urgent()` (hoy arma el
`LineStatus` v1 de 5 B) por:
```cpp
#include "down_model.h"
#include "down_encode.h"
// ... en el namespace anónimo del archivo:
DownModel    g_dm;
DownModelCfg g_dmcfg = { /*imminent_depth*/6, /*adapt_alpha*/0.02f,
    /*calib_min_margin*/120, /*lifted_debounce_ms*/100,
    /*lifted_min_sensors*/ (NUM_LINE_SENSORS*7)/8, /*lifted_delta_below*/80,
    /*line_end_min_track_ms*/200 };
bool g_dm_init=false;

void comm_central_send_line_urgent() {
    if(!g_dm_init){
        for(int i=0;i<NUM_LINE_SENSORS;++i)
            lc_set_static(g_dm.calib[i],
                line_ring_get_carpet(i), line_ring_get_white(i));
        g_dm_init=true;
    }
    uint16_t raw[DM_MAX_SENSORS];
    for(int i=0;i<NUM_LINE_SENSORS;++i) raw[i]=line_ring_get_raw(i);
    LineStatusV2 s = dm_update(g_dm, g_dmcfg, raw, NUM_LINE_SENSORS, millis());
    uint8_t buf[PROTO_MAX_FRAME];
    size_t nbytes = down_encode_line(s, g_send_seq++, buf, sizeof(buf));
    if(nbytes>0){ Serial1.write(buf, nbytes); g_frames_sent++; }
}
```
> Si `line_ring` no expone `line_ring_get_carpet(i)`/`get_white(i)`, agregarlos
> como getters triviales del array de calibración interno de `line_ring.cpp`
> (1 línea cada uno). NO cambiar el muestreo HW (queda para Plan 3).

- [ ] **Step 2: Compilar el binario DOWN**

Run: `pio run -e down`
Expected: compila sin errores (los módulos nuevos están en `shared/`, ya en
`build_src_filter` de `env:down`).

- [ ] **Step 3: Commit**

```bash
git add "software/teensy/Soccer 2026/src/down/comm_central.cpp"
git commit -m "feat(down): emitir LineStatusV2 (contrato v2) desde la placa DOWN"
```

- [ ] **Step 4: Documentar checklist de bring-up (no ejecutable sin robot)**

Crear nota en `team-tasks/` (o anexar a TASK-012) con: medir pines de mux,
validar lecturas crudas por `down_selftest` (Plan 3), correr calibración real,
verificar con osciloscopio que el frame en el cable coincide con el contrato
(usar los ejemplos §3.6), confirmar que CENTRAL decodifica (schema=2).

---

## Self-Review

**Spec coverage:** ✔ LineStatusV2 (Task 1) ✔ ángulo/escape/centroide (Task 2)
✔ CORNER (Task 3) ✔ LINE_END (Task 4) ✔ auto-calibración estática+adaptativa+
SUSPECT (Task 5) ✔ lifted + data_valid (Task 6) ✔ orquestación a contrato
(Task 7) ✔ conformidad byte-a-byte al contrato (Task 8) ✔ integración HW
(Task 9). `penetration_mm` real en mm y `cross_track_mm` quedan como proxy/N-A
explícito → se completan en **Plan 3** (requieren geometría calibrada y
ref-edge físico); declarado, no oculto. OTOS = **Plan 2** (fuera de scope, el
usuario pidió "funciona sin odometría").

**Placeholder scan:** sin TBD/TODO ejecutables; el único proxy (`penetration_mm`)
está marcado y diferido a Plan 3 con justificación (necesita HW).

**Type consistency:** `GeomResult`, `SensorCalib`, `LineStatusV2`,
`DownModel`/`DownModelCfg`, `down_encode_line` usados con las mismas firmas en
todas las tasks. `lf_*` reusa la API existente de `line_filters.h`.

**Notas de ejecución:** correr `pio test -e test_native -f <suite>` por task;
`pio run -e down` solo en Task 9. Tests viven en `test/<suite>/test_main.cpp`
con `int main()` (patrón del repo, ver `test_line_filters`).
