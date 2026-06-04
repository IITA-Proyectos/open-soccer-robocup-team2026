# Technical Documentation Paper (TDP) — RoboCupJunior Soccer Open League 2026

**Team:** IITA — [TEAM NAME] (confirm the name registered for RoboCup Incheon; the repository uses "IITA — Open Soccer RoboCup Team 2026")
**Organization:** IITA (Instituto de [Innovación/Informática] y Tecnología Aplicada — VERIFY legal name) · Fundación Innovar — Salta, Argentina
**League / sub-league:** RoboCupJunior Soccer — **Open League**
**Event:** RoboCup 2026 — Incheon, South Korea (30 Jun – 6 Jul 2026)
**Qualification:** National champions, RoboCupJunior Soccer Argentina (Roboliga, December 2025, Buenos Aires)
**Repository (open-source, MIT):** https://github.com/IITA-Proyectos/open-soccer-robocup-team2026

| Role | Name | Technical responsibility |
|---|---|---|
| Project director | Gustavo Viollaz (@gviollaz) | Coordination, test bench, integration of the 3 boards |
| Coach | Enzo Juárez (@enzzo195) | PCB design (EasyEDA), hardware bodges, technical review |
| Competitor — Soccer Open | María Virginia Viollaz (@mariaviollaz), 18 | Computer vision, trajectories, bench |
| Competitor — Soccer Open | Elías Cordero (Electromechanical Eng., UNSa) | Motors, power electronics, bench |

> **How to read this TDP (for the judge):** the 4 sections map 1:1 to the 4 TDP rubric criteria — **§1 Electrical**, **§2 Mechanical**, **§3 Software**, **§4 Presentation / Narrative**. Every design decision is presented in a **Decision → Why → Data** format. The closing section (§5) explicitly claims the **2 bonus points** (open-source CAD/PCB and open-source software). Anything **not** yet validated on hardware is marked as such honestly, distinguishing *"verified on the bench"* from *"verified on the host only"*.

---

## Robot summary (context in 30 seconds)

Two omnidirectional robots (3-wheel KIWI omni base at 120°): **ROBOT1 = goalkeeper**, **ROBOT2 = striker**. No physical kicker: the striker pushes the ball by inertia. The intelligence is distributed across **3 Teensy boards + 1 COMM board + 2 cameras**:

| Board | MCU | Role | Main sensors |
|---|---|---|---|
| **TOP** | Teensy 4.0 | Sensory brain | 2 OpenMV N6 cameras + 2 BNO055 (IMU) + 4 ToF VL53L7CX + 1 HC-SR04 + referee over GPIO |
| **CENTRAL** | Teensy 4.1 (on Zircon Rev v15 PCB) | Decision brain / master | Tactical FSM + 3 PIDs + omni-3 inverse kinematics + 3 motors |
| **DOWN** | Teensy 4.0 | Floor sensor | Ring of 32 line sensors (4× CD4051 mux) + 2 OTOS optical-odometry sensors |
| **COMM** | ESP32-C6 | RCJ referee + partner | Fork of the official RCJ module; delivers START/STOP as a GPIO level |

TOP fuses everything into a **31-byte WorldSnapshot** that it sends to CENTRAL over UART at 100 Hz; DOWN broadcasts line + odometry to CENTRAL and TOP (symmetric broadcast). The decision logic lives in **pure C++ modules** verified by a **host-native test suite (545 tests / 40 suites / 0 failures (verified 2026-06-04 via scripts/run-host-tests.sh))** that runs on a PC without the board.

---

# §1. ELECTRICAL — Replicable electrical design, with data-driven reasoning

> **Rubric goal (Excellent):** provide enough detail for a technical reader to **replicate the design process**, assess resource use, and give **data-driven reasoning** for each decision. This section is organized as *Decision → Why → Data*, with reproducible pinout tables and the bring-up procedures that avoid the mistakes we already made.

## 1.1 Overall electrical topology

The robot uses **4 microcontrollers** (3 Teensy + 1 ESP32-C6) on **3 custom PCBs + 1 commercial board**:

| Board | PCB | MCU | Core |
|---|---|---|---|
| TOP | "Roboliga2026_TOP" (custom, 2-layer, ≈224.0 × 97.5 mm) | Teensy 4.0 (U14) | Cortex-M7 @ 600 MHz |
| CENTRAL | Zircon Rev v15 (commercial, Robomov) | Teensy 4.1 | Cortex-M7 @ 600 MHz |
| DOWN | "Roboliga 2026 Futbol" REV 1.0 (custom, ≈175.1 × 165.7 mm) | Teensy 4.0 (U7) | Cortex-M7 @ 600 MHz |
| COMM | "PCB1" (custom, 25.40 × 31.20 mm) | ESP32-C6-MINI-1-N4 | RISC-V, WiFi6/BLE5/802.15.4 |

**Decision:** distribute the electronics across specialist boards instead of a single monolithic board.
**Why:** *process where the sensor is, decide in the center* (a standard mobile-robotics rule, cf. CAMBADA MSL and PCBWay RCJ 2022/2024). It reduces UART traffic and keeps each peripheral from fighting over another board's bus.
**Data:** by design, each MCU runs at **< 30 % CPU** (TOP ~25 %, CENTRAL ~20 %, DOWN ~22 % — design estimates documented in `docs/ARQUITECTURA-3-PLACAS-2026.md`). *(Caveat: these are design targets, not measured with an oscilloscope/profiler; see the "live metrics" gap.)*

## 1.2 Power (replicable chain)

**Identical power chain on TOP and DOWN** (CENTRAL uses the Zircon's regulation):

```
LiPo 2S 7.4 V nominal ──► Deans-T-F connector (XP1)
                          │
            2× Schottky diode B5819W (1A/40V, LCSC C8598)  ← reverse-polarity protection / OR-ing
                          │
            ┌─────────────┴─────────────┐
   Buck MP1584-EN (≈5 V logic)   Buck MP1584-EN (≈3.3 V sensors)
```

**Decision:** two independent MP1584-EN buck regulators per board (one ≈5 V logic rail, one ≈3.3 V sensor rail) + 2 Schottky protection diodes.
**Why:** separating the sensor rail from the logic rail reduces conducted noise; the Teensy 4.0 **does not tolerate 5 V on its GPIO** (3.3 V max), so 5 V electronics (e.g. the HC-SR04) must be level-shifted.
**Critical data learned on the bench:** the **OTOS are powered from the MP1584's 3.3 V that comes from the BATTERY** — USB only powers the Teensy. A weird I²C address such as `0x64` instead of `0x17` **is a brownout of a marginal 3.3 V rail, not a different chip**. Bring-up recipe: battery delivering real current + a full power cycle (battery + USB, 10 s) before opening the serial monitor.

> **[GAP — power budget]** The trimpot set-points of the 6 MP1584 bucks **were not measured with a multimeter** (5 V / 3.3 V are assumed). Also missing: LiPo battery capacity (mAh), C-rating, and weight, plus the runtime calculation. **Data to measure before Incheon.** See §6 (gaps).

## 1.3 TOP BOARD — I²C buses and sensor selection

The **6 I²C sensors hang off the same `Wire` bus (pins 18/19)**: 2 BNO055 (0x28, 0x29) + 4 ToF VL53L7CX (0x2A–0x2D).

| Sensor | Qty | Address | Bus | Why it was chosen |
|---|---|---|---|---|
| BNO055 (9-DOF IMU) | 2 | 0x28 / 0x29 | `Wire` | On-chip fused absolute heading; redundancy against impacts/magnetic interference from motors |
| VL53L7CX (8×8 multizone ToF) | 4 | 0x2A–0x2D | `Wire` | 2D localization by trilateration (4 orthogonal walls); FoV 60°, ±15 mm at <2 m |
| HC-SR04 (ultrasonic) | 1 | — (GPIO) | — | Redundant frontal distance (currently gated OFF) |

**Key decision (data-driven):** **I²C at 100 kHz** and read the BNO at **20 Hz** (not 100 Hz).
**Why:** the BNO055 and the VL53L7CX **do not coexist** on the same bus at high speed: while the ToF are ranging, the BNO's multi-byte read gets corrupted.
**Data:** at 400 kHz (or 100 kHz with the BNO read hard) **the yaw freezes**; at **100 kHz + BNO @20 Hz** the heading stays OK on the bench (boot ~40 s). It is a *band-aid*: the underlying fix (noted in the code) is to move the BNO to a separate bus (`Wire1`). **Honest real status:** the RIGHT BNO (0x29) is a **failed unit**; the robot runs with 1 healthy BNO (0x28) + 4 ToF, all stable.

### Reference electrical iteration — enumerating 4 ToF on a single bus

**Problem:** all 4 VL53L7CX boot at 0x29 → they collide. A forensic analysis of the rev 1.0 schematic/PCB revealed that the 4 XSHUT/LPn pads were **intentionally left unrouted** (8 explicit No-Connect flags, 0 nets in the netlist). The `PIN_TOF_XSHUT` line in the config was fiction inherited from the aspirational design.
**What we tried:** a forensic search for XSHUT/LPn strings in the SCH+PCB JSON (0 matches); a **physical bodge** (Enzo) wiring each ToF's LP pin to a GPIO by reusing the INT trace; 5 incremental diagnostic sketches.
**Data:** after a power cycle, the 4 LP pins work on pins **{9, 10, 11, 12}** (active-high) and enumerate to 0x2A–0x2D. **Reproducible lesson:** the VL53L7CX I²C addresses **persist as long as 3V3 is present** — you must **power-cycle, not reset**, or the bus boots dirty and enumeration returns a false negative ("no LP works").
**Change:** `PIN_TOF_XSHUT = {9,10,11,12}`, `NUM_TOF_ACTIVE = 4`; `Wire1` (24/25) freed for DOWN; 2D trilateration localization unblocked at the HW level. **Noted collateral conflict:** pin 10 (LP of ToF[1]) clashes with the role dip-switch → relocate. Routing XSHUT on the PCB remains a P0 item on the TOP rev 1.1 wishlist (post-Incheon).

## 1.4 DOWN BOARD — line ring and odometry

| Component | Qty | Part / LCSC | Function |
|---|---|---|---|
| ALS-PT19 phototransistor | 32 | Everlight / C146233 | Line sensor (sees white vs carpet) |
| 0402 emitter LED | 32 | C28310436 | Paired sensor illumination |
| 330 Ω resistor | 32 | — | LED current limiting |
| 10 kΩ resistor | 32 | — | Phototransistor bias |
| CD4051BM analog mux | 4 | TI / C353976 | 8 channels each → 32 multiplexed sensors |
| OTOS (optical odometry) | 2 | SparkFun | Optical pose/slip (mouse-style floor reading) |

**Decision:** the 2 OTOS sit on **separate I²C buses** (`Wire` 18/19 and `Wire1` 17/16).
**Why:** both OTOS share the **same fixed address 0x17** (not selectable) → they cannot share a bus. This is the *opposite* solution to the ToF (which reassign addresses and therefore can share a bus).
**Data:** quantitative bench validation (2026-05-24): **300 mm actual → 280.4 mm reported = 6.5 % error** (passes the 8 % tolerance). See **Fig. 9 — OTOS odometry error by surface** (`docs/competencia/assets/fig9_otos_error.png`, generated by `gen_figuras.py`).

### Reference electrical iteration — the 4 mux do NOT share select lines

**Problem:** prior documentation claimed the 4 CD4051 shared the A/B/C lines — an architecture that would have broken the firmware reading the 32 sensors.
**What we tried:** **automatic extraction from the EasyEDA schematic JSON** with a Python parser (union-find over wires/junctions) cross-checked against the Teensy 4.0 PJRC pinout, plus empirical bench validation (covering all 32 sensors).
**Data:** each CD4051 has **its own 3 select lines (12 SEL pins in total, not shared)** + 4 ADC outputs (A0/A1/A8/A9); each mux's INH is tied to physical GND (always enabled). Channel order has consistent "scrambling" `CH_LUT={3,0,1,2,5,7,6,4}`. **Bench verdict: 32 OK, 0 dead.**
**Change:** `config_down.h` rewritten with `PIN_MUX_A/B/C[4]` (12 independent pins) and `PIN_MUX_OUT={A0,A1,A8,A9}`; `PIN_MUX_INH[]` removed. **Reproducibility:** the `extract_pinout_from_schematic.py` script regenerates the full pin table from the SCH/PCB JSON (a reusable pattern for any EasyEDA PCB, documented step by step in `down-board-pack/01-pinout-y-posiciones.md §13`).

## 1.5 CENTRAL BOARD — motor drivers (Zircon Rev v15)

3 H-bridges (drivers U5/U7/U17, each with INA+INB+PWM, 8-bit PWM 0–255). Different logical mapping per robot (different physical wiring):

| Motor | ROBOT1 (goalkeeper) | ROBOT2 (striker) | Notes |
|---|---|---|---|
| M1 | U5 (INA2/INB5/PWM3) | U17 | — |
| M2 | U17 (INA8/INB7/PWM6) | U7 | **INVERTED by HW** (INA/INB swapped) |
| M3 | U7 (INA11/INB12/PWM4) | U5 | — |

**Decision (data-driven):** `MOTOR_INVERT = {+1, -1, +1}`, applied at a single point (`motors_zircon.cpp`).
**Why:** driver U17 has INA/INB swapped in hardware → motor 2 spins inverted; without compensation, the omni kinematics produce inverted trajectories.
**Data:** validated on the bench (María/Elías, `diag_central_line_sweep_robot1`) by activating each H-bridge separately and observing the rotation direction. **Honest caveat:** ROBOT2 inherits the same array **unvalidated** (rotated drivers); the bench will settle it.

## 1.6 RCJ referee integration — the integration error we avoided

**Decision:** read the referee as a **GPIO level (not UART)** on TOP pins 5/6, with `INPUT_PULLDOWN` and `match_running = pin5 OR pin6`.
**Why:** the official RCJ COMM module delivers START/STOP as a **voltage level** (3.3 V = GO / 0 V = STOP) on OUT1/OUT2 via a TXS0102 level shifter — it **never emits the UART frame** the firmware originally expected.
**Bench data (key):** in PLAY the COMM raises **ONLY ONE** of the two pins (they are not mirrored) → the original AND never produced GO; the **OR does**. Fail-safe: loose cable → both LOW → STOP. With this fix (TASK-039), **the referee moved CENTRAL for the first time end-to-end** (COMM→GPIO 5/6→TOP→flag in WorldSnapshot→Serial7→CENTRAL).

## 1.7 Link map (reproducible — both ends)

| Link | TX (board·port·pin) | RX (board·port·pin) | Baud | Status |
|---|---|---|---|---|
| TOP → CENTRAL (31 B snapshot) | TOP·Serial4·pin17 | CENTRAL·Serial7·pin28 | 230400 | fix 2026-06-02 |
| DOWN → CENTRAL (line + OTOS) | DOWN·Serial1·pin1 | CENTRAL·Serial1·pin0 | 230400 | ✅ bench-validated |
| DOWN → TOP (line + OTOS) | DOWN·Serial5·pin20 | TOP·Serial1·pin0 | 230400 | ⚠️ not yet wired |
| front camera → TOP | cam·UART3 | TOP·Serial3·pin15 | 19200 | ✅ format OK |
| rear camera → TOP | cam·UART3 | TOP·Serial5·pin21 | 19200 | ✅ format OK |
| TOP ↔ COMM (partner ESP-NOW) | TOP·Serial2·7/8 | COMM (ESP32-C6) | 115200 | fix 2026-06-02 |

> **Documented hardware trap to replicate:** the **Teensy 4.0 does NOT expose Serial7 (28/29) on the edge** (they are rear SMD pads). That is why the link to CENTRAL goes over Serial4 (16/17), not Serial7. The Teensy 4.1 (CENTRAL) **does** expose them.

## 1.8 BOM of major components — resource use / cost: [BOM costs — pending]

> **Resource use / cost:** [BOM costs — pending]. The per-robot quantities are confirmed; the unit prices and the total cost **are pending closure** (see the gap below). The table lists what cost data we do have and marks the rest as pending.

| Component | Qty (robot) | Unit price (LCSC USD) |
|---|---|---|
| Teensy 4.0 / 4.1 | 3 | [BOM costs — pending] |
| OpenMV N6 (camera) | 2 | [BOM costs — pending] (the most expensive components) |
| BNO055 (IMU) | 2 | ~15 USD (qualitative mention) |
| VL53L7CX (ToF) | 4 | [BOM costs — pending] |
| OTOS SparkFun | 2 | [BOM costs — pending] |
| CD4051BM (mux) | 4 | 0.96 |
| ALS-PT19 (phototransistor) | 32 | 0.116–0.118 |
| 0402 LED | 32 | 0.016 |
| B5819W diode | 4–6 | 0.024 |
| MP1584-EN (buck) | 6 | [BOM costs — pending] |
| ESP32-C6 / Zircon / LiPo | 1 each | [BOM costs — pending] |

> **[BOM costs — pending]** The BOM CSVs (LCSC) only carry unit prices for **some passives**. Missing prices for the Teensy, OpenMV N6, OTOS, BNO055, MP1584, battery, and the purchased Zircon board, and **there is no total robot cost nor ARS conversion**. Building the full economic table (resource use / cost) is a registered gap.

---

# §2. MECHANICAL — Mechanical strategy, design iterations, and trade-offs

> **Rubric goal (Excellent):** describe the mechanical strategy and **design iterations**, explain **trade-offs and constraints**. Wherever a mechanical value has not yet been measured on the assembled robot, it is explicitly marked **TENTATIVE** (engineering honesty) and the procedure to measure it is given.

## 2.1 Mechanical strategy: 3-wheel KIWI omni base

**Decision:** an omnidirectional **3-wheel KIWI omni base at 120°**, with no physical kicker.
**Why:** the 3-wheel omni base gives holonomic motion (independent translation + rotation) with fewer motors/less weight than a 4-wheel base; removing the kicker drops components, energy draw, and failure points — the striker **pushes the ball by inertia** once aligned with the opponent's goal.
**Trade-off:** the KIWI geometry has a rear wheel (at 180°) that **contributes nothing to a pure lateral strafe** (projection = 0). This is correct by geometry, but it demands care with the PWM dead zone (see iteration 2.4).

| Parameter | Firmware value | Status |
|---|---|---|
| `WHEEL_ANGLES_DEG` | {60, -60, 180} (front = +Y) | **TENTATIVE — confirm on the physical assembly** |
| `WHEEL_RADIUS_MM` (center→wheel) | 100.0 | **TENTATIVE — measure on the assembled robot** |
| `MAX_SPEED_MM_S` | 1000 | estimated |

## 2.2 The chassis IS the DOWN PCB (structural decision)

**Decision:** the DOWN board ("Roboliga 2026 Futbol" REV 1.0) **is the robot's structural base plate**.
**Why:** integrating the ring of 32 line sensors directly into the base plate eliminates a mechanical part and guarantees the ring geometry (the sensors stay fixed with respect to the center).
**Data:** rounded plate-like outline ≈**175.1 × 165.7 mm** (`Gerber_BoardOutlineLayer.GKO`), NPTH 3.0/3.5 mm mounting holes for M3 screws to the chassis, **142 of 148 components on the Bottom side** (the ring faces the floor). The 3 PCBs stack as levels (TOP / CENTRAL / DOWN).

## 2.3 Floor-by-floor mechanical architecture (3-board stack)

```
   ┌──────────────────────────┐
   │   TOP   (Teensy 4.0)      │  ← cameras + IMU + ToF (looks forward/up)
   ├──────────────────────────┤
   │   CENTRAL (Zircon 4.1)    │  ← motors + FSM
   ├──────────────────────────┤
   │   DOWN  (Teensy 4.0)      │  ← structural base plate + line ring (looks at the floor)
   └──────────────────────────┘
            │  │  │
         3 omni motors at 120° (KIWI)
```

**Milestone (2026-05-29):** the 3 physical boards exist and are mounted — the robot is nearly complete mechanically/electronically.

> **[GAP — stack]** The **inter-floor spacing** (standoff height, TOP/CENTRAL/DOWN separation) and how the stack is fixed to the chassis **are not documented** in the repository. **[DIAGRAM: layout of the 3-board stack with standoff dimensions]** and **[PHOTO: side view of the robot showing the 3 levels]** are missing deliverables.

## 2.4 Mechanical design iterations (with data)

### Iteration A — Goalkeeper strafe: "only motor 1 turns" (PWM dead zone + KIWI geometry)
- **Problem:** with the referee in START and a lateral strafe command, **only M1 turned**; M2 and M3 stayed still.
- **What we tried:** bench test `diag_central_arbitro_strafe_robot1` with a pure lateral command (vx, vy=0, ω=0), wheel-by-wheel analysis against `WHEEL_ANGLES={60,-60,180}`.
- **Data:** for pure lateral, M1(60°) and M2(-60°) receive ±0.866·vx; with vx=150 mm/s and MAX_SPEED=1000 → PWM ≈ **33/255 = ~13 %**, **below the start-up threshold**. M1 (less friction) scrapes and turns; M2 stays stuck (stall). M3(180°) yields lateral projection of **exactly 0 → still, CORRECT** (expected KIWI geometry).
- **Change:** diagnosis = **not a motor bug, it is low PWM + geometry**. Bench proposal: raise speed (`-DDIAG_ARB_SPEED_MM_S=600` ≈ 52 % PWM). Design candidate (CA-01): dead-zone compensation with a `MOTOR_MIN_PWM` floor (~25–45), calibrated per robot. **Trade-off:** a PWM floor that is too high makes the robot "jump" from rest; that is why it stays at 0 until calibrated per robot on the bench.

### Iteration B — Motor 2 inverted by hardware
- **Problem:** motor 2 (driver U17) spins the wrong way → the kinematics would produce inverted trajectories (circles).
- **Data:** INA/INB swapped in HW on the Zircon (validated on the bench).
- **Change:** `MOTOR_INVERT = {+1,-1,+1}` at a single point. **Constraint:** ROBOT2 inherits the array unvalidated (rotated drivers) — pending bench.

### Iteration C — KIWI kinematics: did it produce circles?
- **Problem:** initial hypothesis that `WHEEL_ANGLES` produced circles instead of straight lines (unmeasured geometry).
- **Data:** the strafe bench (Iteration A) showed that **M3=0 in pure lateral is correct**, reducing the "circles" suspicion to a dead-zone/PWM problem, not an angle problem.
- **Change:** the values are kept as TENTATIVE pending physical measurement. **Measurement procedure documented** (`docs/omni3-drive-system.md §4`): real `wheel_radius` = mark the wheel, roll 1 turn, distance/2π; real `robot_radius` = center to contact point.

### Iteration D — OTOS protective film and surface texture
- **Problem:** sub-optimal optical readings with the plate's protective film.
- **Data:** with film over A4: 28.6/300 mm = **9.5 %** (catastrophic). Without film over A4: 0.3 mm = **0 %** (surface too uniform, like an optical mouse over glass). Without film over **corrugated cardboard**: 280.4/300 = **6.5 %** (passes the 8 % tolerance, monotonic).
- **Change (TASK-030):** film removed + require a micro-textured surface. **10× improvement.** On the RoboCup green carpet both conditions are met by default. The three points (film / direct / cardboard) are plotted in **Fig. 9 — OTOS odometry error by surface** (`docs/competencia/assets/fig9_otos_error.png`).

### Iteration E — Edge emergency brake (brake or coast?)
- **Problem:** `EMERGENCY_LINE` calls `motors_brake()` (HIGH/HIGH), but it is not confirmed that the Zircon brakes actively (it could COAST).
- **Data:** at 1 m/s the robot travels 15 mm in 15 ms; if it brakes by coasting instead of active braking, the post-detection distance grows and it may cross the line.
- **Change (CA-03, pending bench):** **measure first** with the driver datasheet; if it COASTs, implement braking via a brief reverse pulse. Flagged "do not touch the firmware blindly".

## 2.5 Field and manufacturing constraints

- **RCJ Soccer field:** playing area 2190 × 1580 mm; wall-to-wall 2430 × 1820 mm. Goal-to-goal axis (+Y, long) = 2430; lateral (+X, short) = 1820. Open-source 42 mm IR ball (2026).
- **Manufacturing (inherited from 2025, for reference):** 3D printing (HM2300 printer, OpenSCAD/Tinkercad sources). 2025 motors = TT motors with H-bridge drivers.

> **[GAP — mechanics/manufacturing]** Real data for the 2026 robot is missing for full replicability: **diameter/weight** of each robot and the regulation size limit; **2026 motor specification** (model, V, RPM, torque, gear ratio, encoder yes/no); **2026 omni wheel** (diameter, material, rollers); **CAD/STL/GCode of the 2026 chassis** (only links to the 2025 chassis exist, which include the already-discarded dribbler+solenoid) + print parameters; **board-stack spacing**; **chassis materials** (beyond the PCB plate); **bumpers/cover/protection**. Registered as gaps in §6. **[PHOTO: assembled 2026 robot, top and side views; printed parts and assembly.]**

---

# §3. SOFTWARE — Code structure, version control, and pseudocode

> **Rubric goal (Excellent):** give real *insight* into the structure/function of the code **and include use of version control, flowcharts, or pseudocode**. This section covers all three: (a) module architecture, (b) host-native testing + multi-agent git as version control, (c) flowcharts/pseudocode in ASCII blocks.

## 3.1 Stack and discipline: pure logic + thin glue

**Languages:** C++17 (firmware, `iitasoccer` namespace, `__attribute__((packed))` structs with size `static_assert`) + Python (OpenMV N6 vision, `find_blobs` by LAB color). Build: PlatformIO (**57 `[env:]` entries** — per-robot envs + ~40 diag/test).

**Core engineering decision:** the **decision logic lives in PURE modules** (`src/shared/`, with no Arduino/Wire/Serial/`analogWrite`); the **Arduino glue is thin** and compile-only.
**Why:** pure modules compile and test with **g++ on the PC, without the board**, which gives a verification cycle of seconds and sidesteps Avast blocking the PlatformIO registry.
**Data:** **545 tests / 40 suites / 0 failures (verified 2026-06-04 via scripts/run-host-tests.sh)**. Session-to-session growth, fully traceable: 180 → 246 → 262 → 324 → 354 → 403 → 470 → 545. See **Fig. 8 — host-native test coverage growth** (`docs/competencia/assets/fig8_test_growth.png`, generated by `gen_figuras.py`).

### [FLOWCHART] Host-native verification pipeline
```
   PURE module (src/shared/*.cpp)  ──┐
   Unity test (test/test_X/...)    ──┤
   vendored Unity (lib/Unity)      ──┤
                                     ▼
        g++ -std=gnu++17 -I src/shared -I lib/Unity/src \
            lib/Unity/src/unity.c src/shared/*.cpp test_main.cpp -o test_X
                                     ▼
        ./test_X   →   PASS/FAIL on the PC (no board, no network, no Avast)
                                     ▼
        green gate (0 failures)  →  only then is it merged
```

## 3.2 Main pure modules (host-tested)

| Module (`src/shared/`) | What it does | Tests |
|---|---|---|
| `kinematics` | Omni-3 inverse kinematics `v_i = -vx·sin θ_i + vy·cos θ_i + ω·R`; `saturate_wheels()` scales the 3 wheels proportionally to preserve the trajectory | 11 |
| `pids` | Heading + lateral + distance PID. **`HeadingPID.output_clamp ≤ 327`** | 18 |
| `proto` | UART frame `[0xAA·LEN·TYPE·SEQ·PAYLOAD·CRC16·0x55]`, resynchronizing FrameDecoder | 13 |
| `behind_ball` | Kicker-less push: align with the opponent's goal and push | 16 |
| `ball_velocity` | Ball velocity by finite differences + EMA (α=0.4), reset on loss | 13 |
| `ball_predict` | Anticipating goalkeeper: aims at predicted X = pos + clamp(v·lookahead) | 9 |
| `localization` | Direct 2D trilateration with 4 ToF + heading (integer arithmetic, Q12 LUT) | 14 |
| `line_filters` | Temporal filter + hysteresis + centroid + all-white saturation | 39 |

**Critical bug closed with data (anti sign-flip):** `cmd.omega_centideg_s = omega·100` is **int16**; a clamp of 360 → 36000 centideg **> 32767** → sign wrap → the robot spun full-speed **the wrong way** on saturation. **Fix:** `output_clamp 360 → 327` (327·100 = 32700 < 32767), with a regression test. Zero regression risk: the old behavior **was** the bug.

## 3.3 The data contract: WorldSnapshot v3 (31 bytes)

TOP → CENTRAL @100 Hz, TYPE 0x60, `static_assert(sizeof == 31)`. Contract evolution: **v1 = 24 B → v2 = 27 B (+ball_vx/vy) → v3 = 31 B (+goal_own + heading_valid)**. Fields: own pose (x/y/heading/confidence), ball (x/y + vx/vy robot-frame + visible/confidence), opponent goal (angle+distance), own goal, `min_obstacle_mm`, `referee_cmd`, flags (in_penalty / partner_alive / partner_sees_ball / match_running / heading_valid).

**Decision:** mark contract changes as **WIRE-BREAKING** and deploy them in a coordinated way (re-flash all affected boards together).
**Why:** a piecemeal change leaves the data chain dead (an old parser discards the new frame).
**Data (real bug):** `comm_down.cpp` decoded the old `LineStatus` (5 B) and discarded 100 % of the real `LineStatusV2` (16 B) → CENTRAL **blind to the line**, invisible in telemetry. Fix verified with a g++ harness, 8/8 PASS + `test_central_line_ingest`.

## 3.4 Dual tactical FSM (the brain)

`src/central/strategy.cpp`, called by `main_central.cpp`:

```
ATTACKER:  WAIT_START → KICKOFF → SEARCH → POSITION → APPROACH → (push by inertia)
                                                              └─► LINE_AVOID
GOALKEEPER: WAIT_START → PATROL → INTERCEPT → CLEAR
                                          └─► LINE_AVOID
   EMERGENCY_LINE  ── bypasses the FSM (handled in main_central before the tick) ──►
```

Kicker-less push: the striker pushes when `|angle to goal| < ATK_KICK_ANGLE_DEG (12°)` and `dist < ATK_KICK_DIST_MM (80)`.

### [PSEUDOCODE] Anticipating goalkeeper (`ball_predict`, pure module)
```
function gk_intercept_target(ball_x, ball_vx, lookahead_s, max_lead_mm):
    lead = clamp(ball_vx * lookahead_s, -max_lead_mm, +max_lead_mm)
    px   = ball_x + lead
    if trajectory == BT_TO_OWN_GOAL:       # shot is heading toward our own goal
        lead = lead * extra_factor
        kp   = kp * scale
    return px
# Byte-identical FALLBACK: with vx=0 (ball at rest / velocity N/A) → lead=0 → px=ball_x
# = behavior IDENTICAL to the version without anticipation, guaranteed by test.
```

**Cross-cutting design decision — byte-identical fallback:** each new feature (anticipating goalkeeper, drive-straight with OTOS, cross_track strafe) produces **exactly the same command** as the previous behavior when the new data is N/A.
**Why:** it lets the feature "sleep" until the data flows on the bench, without introducing a regression.
**Data:** verifiable with a test that compares the output with and without the data.

## 3.5 2D localization by trilateration (decision with alternative analysis)

**Decision:** direct geometric trilateration (4 cardinal ToF + BNO heading, integer arithmetic).
**Why:** it was formally compared against 5 alternatives for accuracy / CPU / dev-time.
**Data:**

| Algorithm | Accuracy | CPU | Dev-time | Verdict |
|---|---|---|---|---|
| **Geometric trilateration** | ±2–3 cm | negligible | ~1 day | **CHOSEN** |
| EKF | ±0.5–1 cm | medium | 3–5 days + tuning | 2027 backlog |
| Particle Filter / MCL | ±1 cm | ~500 µs | high | overkill "for a 1.83×2.43 m field with 4 orthogonal walls" |

**Honest status:** the algorithm is tested (14 host tests) but **hardware validation is pending (TASK-035)** — today the pose never comes out "valid" (ToF on the Y axis only) and `main_top` uses the IMU's heading directly.

## 3.6 Symmetric DOWN broadcast with loss detection

DOWN broadcasts 3 frames to CENTRAL (Serial1) **and** TOP (Serial5): `LineStatusV2` 0x10 @200 Hz + `Pose2D` 0x11 @100 Hz + `Velocity2D` 0x12 @100 Hz, with a **monotonic SEQ per link** (the receiver detects loss by SEQ gap). Implemented in 3 layers (transport / drive-straight OTOS / real cross_track), all with exact fallback.

## 3.7 Layered fail-safes and watchdogs (with numbers)

| Mechanism | Behavior | Target number |
|---|---|---|
| Direct DOWN→CENTRAL bus | Brake on leaving the field in 1 UART hop | < 15 ms (vs ~25 ms via 2 UARTs) |
| WorldSnapshot watchdog | No frame in 500 ms → safe mode (motors stopped) | 500 ms |
| LINE_URGENT watchdog | No frame in 500 ms → blind line strategy | 500 ms |
| Referee fail-safe | Loose cable → both pins LOW → STOP | — |
| Anti sign-flip clamp | `omega·100` ≤ 32767 (int16) | clamp ≤ 327 |

## 3.8 Version control: multi-agent development and audit

**Decision:** development on **4 parallel branches** `agente/{central, down, top, vision}` with git worktrees, coordinated merges behind a green gate; repository **shared** with the human team that pushes directly to `origin/main`.
**Why:** it allows working on 4 subsystems in parallel without collision and leaves a clear audit trail; commits that break a contract are signed **[WIRE BREAKING]**.
**Data:** real branches on `origin` (`agente/central`, `agente/down`, `agente/top`, `agente/vision`, `main`). History examples: `24bd417` "WorldSnapshot v3 [WIRE BREAKING]", `d230de5` "camera contract v2 [WIRE BREAKING]", `840f2e4` "merge agente/down → main". Collaboration rule: **`git fetch` + `git merge origin/main` before pushing** (after a non-fast-forward collision).

**Adversarial audit as part of the process:** a parallel audit of **20 subsystems** (each by an independent engineer; the HIGH findings went through a 2nd skeptical reviewer). Verdict: **15/20 "solid", 4 "minor-issues", 0 critical; 2 HIGH, 9 MEDIUM, ~40 LOW, 0 false positives** (`research/in-progress/2026-06-04-analisis-paralelo-modulos.md`).

## 3.9 Documentation anti-entropy discipline

Three living indexes fight doc drift: `FUENTES-DE-VERDAD.md` (one canonical doc per topic, rule: whoever creates/supersedes a doc updates the table in the same commit), `MAPA-DE-DATOS.md` (every message: type/size/transport/pin/freq/who-fills/who-consumes), and `ESTADO-ACTUAL.md` (mandatory first read). **Explicit truth hierarchy:** if a doc contradicts the code (`types.h`/`proto.h`) or the wiring, that source wins.

> **[GAP — software · features code-complete but not HW-validated]** Honest declaration: the following features are **code-complete + host-tested (they are part of the 545 tests) but NOT YET validated on hardware** — they are finished code, not behavior proven on the robot: trilateration (TASK-035), anticipating goalkeeper (tune `lookahead_s`/`max_lead_mm`), cross_track strafe (axis/sign), drive-straight OTOS, dead-BNO failover (IMU-1 HIGH). **Real blocker #1:** **vision without recalibrating LAB+homography** for Incheon (TASK-022) — the robot does not see the ball until calibrated on the bench. CPU loads and latencies are **design targets, not measured with an oscilloscope**. **[PHOTO: the suite of 545 host tests green; bench diag decoding the WorldSnapshot.]**

---

# §4. PRESENTATION / NARRATIVE — Team journey, well organized and navigable

> **Rubric goal (Excellent):** a document that is **well organized and easy to navigate** **with a clear narrative of the team's journey**. This section closes the TDP by telling where we come from, what we decided and why, and what we learned.

## 4.1 The team journey

We are the **IITA (Salta, Argentina)** team, **national champions** of RoboCupJunior Soccer at the Roboliga Argentina (Buenos Aires, December 2025), qualified for **Incheon 2026**. The 2026 robot **does not start from scratch**: the **CENTRAL board (Zircon Rev v15 + Teensy 4.1) is exactly the one that won the 2025 Nationals**. The strategic decision was to **build new capability around what already works**, not replace it: TOP (perception) and DOWN (floor) are added as pre-processors. If a new board fails at Incheon, CENTRAL can degrade to monolithic mode.

The team's philosophy is **"invest in learning, not in the podium"**: an honest robot, matches actually played, and the systematic capture of every lesson in the engineering `journal/`. This TDP reflects that honesty: we clearly mark what is *bench-validated* vs *host-verified only*, and we publish even the false negatives that cost us time (the ToF power cycle, the GPIO referee, the silently broken line contract).

## 4.2 What we learned (the most transferable lessons)

1. **Verifying embedded firmware without the board is possible** and it changes iteration speed (pure logic + g++ host = 545 tests in seconds).
2. **Bring-up details kill**: the VL53L7CX I²C addresses persist with 3V3 (power-cycle, not reset); the OTOS are powered from the battery, not USB; the BNO + ToF do not coexist at 400 kHz.
3. **The RCJ referee arrives as a GPIO level, not over UART**, and in PLAY it raises a single pin (OR, not AND) — an integration error that can cost you inspection/homologation.
4. **Designing with a byte-identical fallback** lets you enable features with no regression risk.

## 4.3 Navigation map of this TDP

| Section | Rubric criterion | Where |
|---|---|---|
| §1 Electrical | Replicability + data-driven reasoning | Pinout, buses, power, BOM, electrical iterations |
| §2 Mechanical | Strategy + iterations + trade-offs | KIWI, PCB-plate, 5 iterations with data |
| §3 Software | Structure + version control + pseudocode | Pure modules, FSM, flowcharts, multi-agent git |
| §4 Presentation | Organization + journey narrative | This section |
| §5 Bonus | Open-source CAD/PCB + software | Closing |

## 4.4 Realistic status for Incheon (engineering honesty)

- ✅ **Solved on the bench:** the referee moves CENTRAL end-to-end; ring of 32 sensors (0 dead); 2 OTOS respond (6.5 % error); 4 ToF enumerate; motors mapped with validated `MOTOR_INVERT`.
- ⚠️ **Open blockers:** vision recalibration (TASK-022, #1); KIWI kinematics calibration; emergency brake (brake vs coast); BNO failover; trilateration validation on HW.

---

# §5. OPEN SOURCE — Claiming the 2 bonus points

> **Rubric goal (Bonus, +2):** +1 if **CAD/PCB/schematics** are open-sourced; +1 if **the software** is open-sourced. Dumping files is not enough: they are published **with an explanation of how and why**.

**License:** **MIT** (`LICENSE`, Copyright 2026 IITA / Fundación Innovar). **Public repository:** https://github.com/IITA-Proyectos/open-soccer-robocup-team2026
*(Consistency note: IITA's legal name appears as "Innovación" in `LICENSE` and "Informática" in `README`/`POSTER` — a real gap, to be resolved before submission; see §6.)*

## 5.1 Bonus +1 — Open CAD / PCB / schematics

| Deliverable | What is published | Replicable by third parties |
|---|---|---|
| TOP PCB | Complete EasyEDA project (SCH JSON + PCB JSON + PDF + BOM CSV + Pick&Place + gerbers) in `hardware/electronics/pcb_design/top_board/` | ✅ re-fabricable as-is |
| DOWN PCB | Same in `pcb_design/down_board/` (+ the `.GKO` outline that IS the plate) | ✅ re-fabricable as-is |
| COMM PCB | Fork of the official RCJ module | ✅ |
| CENTRAL (Zircon) | `Zircon.pdf` schematic (commercial Robomov board) | partial (buy/use the schematic) |
| Reproducible pinout | `extract_pinout_from_schematic.py` script regenerates the pinout from the JSON | ✅ reusable EasyEDA pattern |

> **Honesty (scope of the mechanical open-source):** the **PCBs and schematics ARE open-source** (complete EasyEDA projects for TOP and DOWN, the COMM fork, the Zircon schematic — re-fabricable as-is, see the table above). In contrast, the **CAD/STL of the 2026 chassis is PENDING / NOT in the repository** (only links to the 2025 chassis exist, with the already-discarded dribbler+solenoid). This is a gap that limits full mechanical replicability and is registered in §6.

## 5.2 Bonus +1 — Open software

- **3-board firmware** (`software/teensy/Soccer 2026/`): C++17, pure modules + glue, 57 PlatformIO envs, **545 tests / 40 suites / 0 failures (verified 2026-06-04 via scripts/run-host-tests.sh)**.
- **100 % offline-reproducible build:** libraries vendored in `lib/` (Unity, OatmealOTOS + pruned SparkFun_Toolkit) → `pio run -e down` compiles without a network.
- **Published host-native testing recipe** (`scripts/run-host-tests.sh`) so another team can verify embedded firmware without the board.
- **Self-contained per-subsystem packs** (`hardware/electronics/*-pack/`): docs + firmware snapshot + tests + ground-truth, with a "question → doc" index.
- **OpenMV N6 vision** (Python) + recalibration kit `calib-lab-n6.py`.

**Why this meets the "not just dumping" standard:** every decision is documented with its *why* and its *data* (this TDP + `docs/ARQUITECTURA-3-PLACAS-2026.md` + dated iteration journals), and the bring-up procedures that cost us hours are written as reusable lessons for 2027 and for any other team.

---

# §6. Real-data gaps (to be completed before submission)

> Honest record of missing real data. **Explicit placeholders** are marked in the body of the TDP. Priority: translate to English + close these gaps before submission.

**Identification / team**
- [GAP] **Official team name** registered for RoboCup Incheon (the repository uses "IITA — Open Soccer RoboCup Team 2026"). → form identity = `IITA` + `[TEAM NAME]`.
- [GAP] **Region/representation** exact for RCJ registration (Salta, Argentina confirmed; the name of the regional they qualified through, to be confirmed). → `[REGION]`.
- [GAP] **Legal name of the IITA organization:** the repository contradicts itself — `LICENSE` says "Innovación", `README`/`POSTER` say "Informática". Resolve to a single legal name before submission. → `IITA (Instituto de [Innovación/Informática] y Tecnología Aplicada — VERIFY legal name)`.

**Electrical / costs**
- [GAP] **BOM with costs in currency** (ARS/USD): missing prices for the Teensy, OpenMV N6, OTOS, BNO055, MP1584, battery, Zircon; no total robot cost.
- [GAP] **Real set-points of the 6 MP1584 bucks** (measure with a multimeter).
- [GAP] **Battery:** capacity (mAh), C-rating, weight, computed runtime.

**Mechanical**
- [GAP] **Diameter and weight** of each robot + regulation size limit.
- [GAP] **2026 motor:** model, V, RPM, torque, gear ratio, encoder yes/no.
- [GAP] **2026 omni wheel:** diameter, material, rollers.
- [GAP] **CAD/STL/GCode of the 2026 chassis** + print parameters + board-stack spacing + chassis materials + protection/bumpers/cover.
- [GAP] Bench validation of `WHEEL_ANGLES` / `WHEEL_RADIUS` with the assembled robot.

**Software / validation**
- [GAP] **Vision recalibration** (TASK-022) — blocker #1.
- [GAP] HW validation of trilateration (TASK-035), tuning of the anticipating goalkeeper, cross_track strafe, drive-straight OTOS, BNO failover (IMU-1).
- [GAP] **Live metrics** (CPU/latencies) with an oscilloscope/profiler — currently design targets.
- [RESOLVED] Test count at closing: **545 tests / 40 suites / 0 failures (verified 2026-06-04 via scripts/run-host-tests.sh)** — used consistently throughout the TDP.

**Images (original/CC, labeled and cited)**
- [PHOTO] Assembled 2026 robot (top view with the 3 wheels at 120°; side view with the 3-board stack).
- [PHOTO] Each populated PCB (TOP, DOWN, CENTRAL/Zircon, COMM) + the bodge of the 4 ToF LP pins.
- [PHOTO] The suite of 545 host tests green + bench diag decoding the WorldSnapshot. (Data figures already available: `docs/competencia/assets/fig8_test_growth.png` and `fig9_otos_error.png`, generated by `gen_figuras.py`.)
- [DIAGRAM] Layout of the 3-board stack with standoff dimensions; block diagram of the data flow.
- [PHOTO] Team at the 2025 Nationals.

---

*IITA — Salta, Argentina. RoboCupJunior Soccer Open League 2026.*
