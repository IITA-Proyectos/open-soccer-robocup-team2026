---
title: "Technical Poster — RoboCupJunior Soccer Open 2026"
date: 2026-06-04
status: judging-ready
language: English (judges' deliverable)
target-format: A1 landscape (max 70.7 cm tall × 100 cm wide)
rubric: RoboCupJunior Soccer 2026 — Poster Design & Presentation (5 pts, 6 criteria × 0/1/3/5)
---

# HOW TO READ THIS MOCK-UP (team note, NOT printed)

This file describes the **physical layout** of an **A1 landscape** poster (max **70.7 cm tall × 100 cm wide**).
Each section below is a **POSTER ZONE** with: (a) its position in the grid, (b) the **EXACT TEXT** that gets printed, and (c) the **images** marked with `[PHOTO: ...]`.

The **zone titles are written so a JUDGE finds each rubric criterion at a glance**:

| Rubric criterion (Poster, 5 pts) | Where does the judge find it? | Target level |
|---|---|---|
| **Abstract** | Zone B — "ABSTRACT" | Excellent |
| **Method / Robot Production / Design** | Zones C, D, E, F — "METHOD & DESIGN" | Excellent |
| **Data / Results / Discussion** | Zones G, H — "DATA, RESULTS & DISCUSSION" | Excellent |
| **Photos / Images** | All zones (≥12 labeled figures Fig.N + footer credits) | Excellent |
| **Layout** | 12-column grid, fixed palette, fixed typefaces (Footer zone) | Excellent |
| **Presentation** (live) | Session script — Zone I "PRESENTATION PLAN" | Excellent |

---

# POSTER GRID (A1 landscape, 100 cm wide × 70.7 cm tall)

```
 100 cm WIDE  →
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ ZONE A · TITLE / IDENTIFICATION  (full-width top band, ~10 cm tall)                         │  ▲
├───────────────┬──────────────────────────────────┬───────────────────────────────────────┤  │
│ ZONE B        │ ZONE C  METHOD & DESIGN (1/3):     │ ZONE G  DATA, RESULTS & DISCUSSION    │  │
│ ABSTRACT      │  Architecture + rationale          │  (1/2): iteration table              │  │
│ (col 1-3)     │  (col 4-8)                          │  test→data→change (col 9-12)         │  │  70.7 cm
│               ├──────────────────────────────────┤                                       │  TALL
│ ZONE B2       │ ZONE D  METHOD & DESIGN (2/3):     │                                       │  │
│ TEAM JOURNEY  │  Sensors + language + software     ├───────────────────────────────────────┤  │
│ (journey)     │  (col 4-8)                          │ ZONE H  DATA (2/2): test methods      │  │
│ (col 1-3)     ├──────────────────────────────────┤  (repeatable) + charts (col 9-12)     │  │
│               │ ZONE E  METHOD & DESIGN (3/3):     │                                       │  │
│ ZONE B3       │  BOM + cost + development time     ├───────────────────────────────────────┤  │
│ OPEN SOURCE   │  (col 4-8)                          │ ZONE I  PRESENTATION PLAN + QR        │  │
│ (col 1-3)     │ ZONE F  large photo of the robot    │  (col 9-12)                           │  │
├───────────────┴──────────────────────────────────┴───────────────────────────────────────┤  │
│ ZONE FOOTER · Image credits · License · Typefaces/palette · Repo  (full-width band)         │  ▼
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

**Grid system:** 12 columns, 1.2 cm gutter, 2 cm margin. Three macro-columns: **Left (col 1-3)** = identity + abstract + journey + open source; **Center (col 4-8)** = Method & Design; **Right (col 9-12)** = Data/Results + Presentation.

---

# ZONE A — TITLE / IDENTIFICATION
*(Full-width top band — satisfies the mandatory "Title/Identification" element: team name + region + Open sub-league)*

**EXACT TEXT (printed):**

> ## IITA Low Battery Messi
> ### Push to Score: a kicker-less, 3-board soccer robot verified in software
>
> **RoboCupJunior Soccer — Open League** · **Region:** Salta, Argentina · Roboliga Argentina 2025 (national final, UAI)
> **Organization:** IITA (Instituto de Innovación y Tecnología Aplicada) / Fundación Innovar
> **National champions, Roboliga Argentina (Dec 2025 (UAI)) → RoboCup 2026, Incheon (Jun 30 – Jul 6)**

`[PHOTO: team / IITA logo on the left of the band]`
`[PHOTO: clean render or photo of the complete robot (3/4 view) on the right of the band, neutral background]`
`[PHOTO: small Argentina flag/icon + "Salta" for the region]`

> **Gap note:** confirm the **exact qualifying regional** before printing (✅ team name: IITA Low Battery Messi).

---

# ZONE B — ABSTRACT
*(Left column, top — mandatory "Abstract" element. Targets **Excellent**: summarizes EVERY critical component in scientific language with a clear intent to share actionable knowledge. Does NOT repeat the detail of the other zones — it synthesizes it.)*

**EXACT TEXT (printed):**

> ## ABSTRACT
>
> We present a RoboCupJunior Soccer Open robot built on a **distributed architecture of 3 Teensy boards + 1 communication module**, in which each microcontroller is a **specialist, not a generalist**: **TOP** (Teensy 4.0) perceives the world (2 OpenMV N6 cameras, 1 BNO055 IMU, 4 VL53L7CX ToF sensors, 1 ultrasonic, RCJ referee over GPIO) and publishes a **31-byte WorldSnapshot at 100 Hz**; **CENTRAL** (Teensy 4.1 on the Zircon PCB) decides (tactical state machine + omni-3 inverse kinematics + PIDs) and drives **3 kiwi omni wheels at 120°**; **DOWN** (Teensy 4.0) is the structural plate and the floor sensor (**a 32-sensor line ring** multiplexed + 2 OTOS optical odometers), and **broadcasts** its measurement to both boards (*symmetric broadcast* with sequence-based loss detection).
>
> The robot has **no physical kicker**: the striker **pushes the ball by inertia** when it aligns with the opponent's goal, which reduces components, energy, and points of failure. The central methodological contribution is a **discipline of verifying embedded firmware on the PC without the board**: the decision logic lives in **pure C++ modules** compiled and tested with `g++` offline (**658 tests / 47 suites / 0 failures**, measured on 2026-06-05 19:50 ART via `scripts/run-host-tests.sh`), with thin Arduino *glue*. Three distinctive innovations: **(1) layered fail-safe** with a direct DOWN→CENTRAL emergency bus to stop within **<15 ms** at the boundary; **(2) byte-identical fallback** that lets each new feature "sleep" until its data flows, with no regression; **(3) a goalkeeper that anticipates from ball velocity**. The project is **open-source (MIT)** and includes manufacturable PCBs (EasyEDA), byte-level data contracts, and an *engineering journal* with every iteration measured on the bench. **Honest status (no overselling):** the #1 remaining blocker for Incheon is the **un-recalibrated vision** (LAB color + homography); features such as **2D ToF trilateration**, the **goalkeeper strafe**, and **OTOS drive-straight** are *code-complete* and host-verified, but **pending bench validation** (their behaviors "sleep" until their data flows). This poster documents the design in enough detail for **another team to replicate it**.

---

# ZONE B2 — TEAM JOURNEY
*(Left column, middle — reinforces the **Layout/Presentation** criterion with the "journey narrative" the rubric rewards; also reinforces the Abstract with human context.)*

**EXACT TEXT (printed):**

> ## OUR JOURNEY
> - **Dec 2025:** national champions (Roboliga Argentina (UAI)) with a monolithic robot on the Zircon board.
> - **2026:** redesign to **3 boards**, reusing the champion brain (Zircon) as CENTRAL and adding perception (TOP) and floor sensing (DOWN) — *continuity, not throw-away*.
> - **May–Jun 2026:** bring-up of the 3 physical boards, ~30 documented bench sessions, test suite growing from 180 → **658 tests / 47 suites / 0 failures** (measured 2026-06-05 19:50 ART via `scripts/run-host-tests.sh`).
> - **Jun–Jul 2026:** Incheon. The team's declared strategy: **invest in learning**, play honest matches, and capture data.
>
> ### 🤖 2026 innovation — how we work ("VIBE", AI-assisted methodology)
> We adopted an AI-assisted workflow (Claude) we call **VIBE**: the AI **accelerates** design and documentation, and the **18-year-old team makes the decisions, validates on the bench, and is solely accountable** for what ships to the robot. Four fronts: **VIBE PCB Design** (EasyEDA driven by Claude via MCP), **VIBE 3D Design** (Fusion 360 via MCP — just getting started), **VIBE Coding** (C++ firmware with a 658-host-test safety net), and **Claude for documentation and project management** (TDP, byte-level data contracts, *journal*). We share the **methodology** — not just the code — as a contribution to the RoboCupJunior community.
>
> ### 🛣️ Next step (declared roadmap)
> **Robot-to-robot communication** (goalkeeper ↔ striker) over **ESP-NOW** via the COMM board (ESP32-C6, already on the robot): sharing pose, whether each one sees the ball, and its state to **coordinate strategy**. What remains is integrating it into the WorldSnapshot and validating it on the bench — true to our discipline, the cooperative behavior "sleeps" until the data flows, with no regression. We also plan to move to a **4-wheel omni** base with **shorter motors that include encoders** (more stability and control, and freed-up space for the **kicker** and **dribbler**, which are next year goals).

`[PHOTO: IITA team with the robot/trophy at the 2025 Nationals (UAI) — label Fig.1]`

> **Team roles:**
> | Role | Member | Technical contribution |
> |---|---|---|
> | Mentor (does not travel) | Gustavo Viollaz | Coordination, bench sessions, 3-board integration |
> | Coach | Enzo Juárez Velázquez | PCB design (EasyEDA), hardware *bodges*, electrical validation |
> | Competitor | María Virginia Viollaz (18) | Computer vision, trajectories, camera parser |
> | Competitor | Elías Cordero (Electromechanical Eng., UNSa) | Motor bench, kinematics, measurements |

> **Gap note:** confirm the **formal roster** (ages/category) and "who did what" for the credits.

---

# ZONE B3 — OPEN SOURCE & COMMUNITY
*(Left column, bottom — supports the TDP bonus and the "Documentation & Community" component. On the poster it contributes to **Method/Design** with "the intent to share ALL actionable knowledge".)*

**EXACT TEXT (printed):**

> ## OPEN SOURCE (EVERYTHING published, MIT)
> - **Software:** complete firmware for the 3 boards (C++17) + vision (MicroPython) + **658 tests / 47 suites / 0 failures** (measured 2026-06-05 19:50 ART via `scripts/run-host-tests.sh`) + bench scripts.
> - **Hardware:** complete **EasyEDA** projects for TOP and DOWN (schematic + PCB + Gerbers + BOM + Pick&Place); CENTRAL = Zircon Rev v15 (public schematic).
> - **How, not just what:** living **SOURCES-OF-TRUTH** documents (one canonical doc per topic), a **DATA-MAP** (every message: type/size/pin/frequency/who fills and consumes it), and an **engineering journal** with every iteration.
>
> **Repo:** `https://github.com/IITA-Proyectos/open-soccer-robocup-team2026` `[QR to repo]`

---

# ZONE C — METHOD & DESIGN (1/3): ARCHITECTURE + RATIONALE
*(Center, top — mandatory "Method/Robot Production/Design" element. Targets **Excellent**: complete, clear, and concise production, WITH a rationale for every decision.)*

**EXACT TEXT (printed):**

> ## METHOD & DESIGN — Why 3 boards?
> Each board processes **where the sensor is** and decisions are made **at the center** (standard mobile-robotics rule). This reduces UART traffic, aims to keep each MCU **<30% CPU** (*design target*, not yet measured with an oscilloscope), and allows replacing one board without touching the others.
>
> | Board | MCU | Role | Sensors / actuators |
> |---|---|---|---|
> | **TOP** | Teensy 4.0 | "I see the world" | 2 OpenMV N6 cameras · 1 BNO055 IMU · 4 VL53L7CX ToF · 1 HC-SR04 · GPIO referee |
> | **CENTRAL** | Teensy 4.1 (Zircon) | "I decide" | tactical FSM · omni-3 kinematics · 3 PIDs · **3 motors** |
> | **DOWN** | Teensy 4.0 | "I touch the ground" | 32 line sensors (4 CD4051 muxes) · 2 OTOS |
> | **COMM** | ESP32-C6 | RCJ referee | 3.3 V GPIO level to TOP |
>
> **Justified decisions (data-driven):**
> - **No kicker:** the striker pushes by inertia → less mass, less energy, fewer failures.
> - **DOWN→CENTRAL emergency bus** (1 UART hop): at 1 m/s the robot travels **1 mm/ms**; routing the boundary alarm through 2 serial UARTs adds ~25 mm of *overshoot* → a direct shortcut is wired to stop within **<15 ms**.
> - **All PIDs on CENTRAL:** a single place holding all the gains.
> - **Continuity:** CENTRAL is the Zircon that won the 2025 Nationals; if a new board fails, it degrades to monolithic mode.

`[DIAGRAM: rendered block diagram of the data flow — 3 boxes TOP/CENTRAL/DOWN with arrows labeled "WorldSnapshot 31 B @100 Hz", "LineStatusV2 16 B @200 Hz", "emergency bus" — Fig.2 · archivo docs/competencia/assets/fig2_dataflow.png (gen_diagramas.py)]`

---

# ZONE D — METHOD & DESIGN (2/3): SENSORS, LANGUAGE & SOFTWARE
*(Center, middle — covers the mandatory "programming language" and "sensors used", with software insight.)*

**EXACT TEXT (printed):**

> ## SENSING, LANGUAGE & SOFTWARE
> **Languages:** **C++17** (firmware, `namespace iitasoccer`, *packed* structs with `static_assert` on size) + **MicroPython** (OpenMV N6 vision, color detection in LAB space).
> **Build:** PlatformIO (57 environments) + an offline `g++` runner for the tests.
>
> **Sensors and what they're for:**
> | Sensor | Quantity | Function |
> |---|---|---|
> | OpenMV N6 camera (STM32N6 + NPU) | 2 | Ball + goals (QVGA, ~30 Hz) |
> | BNO055 IMU | 1 healthy | Heading (yaw) |
> | VL53L7CX ToF (8×8 zones) | 4 | Distance to walls → **2D localization** |
> | OTOS (optical odometry) | 2 | Pose/velocity from floor *slip* |
> | Line ring (phototransistor) | 32 | Field boundary (braking) |
> | HC-SR04 ultrasonic | 1 | Frontal obstacle (redundant w/ ToF) |
>
> **Software insight (code structure):**
> - **WorldSnapshot v3 = 31 B** (`static_assert(sizeof==31)`), evolution of the contract v1(24 B)→v2(27 B)→v3(31 B).
> - **Robust UART protocol:** frame `[0xAA | LEN | TYPE | SEQ | PAYLOAD | CRC16-CCITT | 0x55]`; the decoder is a byte-by-byte state machine that **resynchronizes on its own** (one garbage byte does not contaminate the next frame).
> - **Pure omni-3 kinematics:** `v_i = -vx·sin(θ_i) + vy·cos(θ_i) + ω·R`, with **proportional** saturation (scales all 3 wheels to preserve the trajectory).
> - **Anticipating goalkeeper:** aims at the **predicted X** = `pos + v·lookahead` (not the current X).

`[PHOTO: screenshot of the 658-test host suite passing green (run-host-tests.sh terminal) — Fig.3]`
`[DIAGRAM: flowchart of the dual tactical FSM — ATTACKER: WAIT_START→KICKOFF→SEARCH→POSITION→APPROACH (+LINE_AVOID); GOALKEEPER: WAIT_START→PATROL→INTERCEPT→CLEAR (+LINE_AVOID); EMERGENCY_LINE bypasses the FSM. (Pushing toward the goal is NOT a state: it happens inside APPROACH.) Ready-to-use source: the Mermaid diagram in docs/competencia/assets/diagramas.md (verified against strategy.cpp); details in docs/firmware/ESTRATEGIA-ALTO-NIVEL.md — Fig.4 · archivo docs/competencia/assets/fig4_fsm.png (gen_diagramas.py)]`

---

# ZONE E — METHOD & DESIGN (3/3): BOM, COST & DEVELOPMENT TIME
*(Center, bottom — mandatory elements "development time and cost" + "BOM of major components".)*

**EXACT TEXT (printed):**

> ## MAJOR COMPONENTS (BOM) · COST · TIME
>
> The **complete BOM of major components** (with **part numbers**, **source/supplier**, a **New vs. reused** column, **Kit/Custom**, and real LCSC unit costs) is the **single source** `docs/competencia/BOM.md` — this poster **references** it rather than duplicating it. Header excerpt:
>
> | Major component | Part number / model | Qty. | New / Reused | Unit cost (USD) |
> |---|---|---|---|---|
> | OpenMV N6 camera | OpenMV Cam N6 (STM32N6 + NPU) | 2 | New | **USD 165/ea** (int. ref.; most expensive) |
> | TOP / DOWN MCU | Teensy 4.0 (LCSC `C99001332551`) | 2 | New | **USD 23.80/ea** (int. ref.) |
> | CENTRAL MCU | Teensy 4.1 (on Zircon PCB) | 1 | **Reused** (2025 champion) | **USD 31.50** (int. ref.) |
> | BNO055 IMU | Bosch BNO055 (U10/U11) | 2 (1 healthy) | New | **~USD 35/ea** (int. ref.) |
> | VL53L7CX ToF | ST VL53L7CX (Pololu module) | 4 | New | **USD 19.95/ea** (int. ref.) |
> | SparkFun OTOS | SparkFun OTOS (U5/U6) | 2 | New | **USD 84.95/ea** (int. ref.) |
> | Zircon Rev v15 PCB (CENTRAL) | Robomov Zircon Rev v15 (COTS) | 1 | **Reused** | **~USD 200** (standalone Robomov price pending — kit USD 529) |
> | CD4051BM mux | TI CD4051BM (LCSC `C353976`) | 4 | New | 0.96 |
> | MP1584-EN buck regulator | MP1584-EN (SIP module) | 6 | New | **USD 0.90/ea** (int. ref.) |
> | LiPo 2S 7.4 V battery | LiPo 2S (mAh/C TBC) | 1–2 | New | **~USD 10–25** (specs pending) |
> | 3 DC motors + 3 omni wheels | "TT" motor + KIWI omni wheel | 3+3 | New | **motor pending** (TT ~USD 3 vs Pololu HP USD 23.95) |
> | **TOTAL per robot** | — | — | — | **≈ USD 1,000 all-new · ≈ USD 770 reusing CENTRAL** (int. ref.; 2 robots ≈ USD 1,800–2,000) |
>
> **Development time:** the 2026 redesign ≈ **8 weeks** of intensive engineering (May–June), built on the 2025 champion robot; total effort ≈ **4 months** (Feb–Jun 2026), traceable in `journal/`. Test suite growing **180 → 246 → 262 → 324 → 354 → 545 → 658** (measured 2026-06-05 19:50 ART via `scripts/run-host-tests.sh`; see Fig.8).
>
> **Real prices available in the repo (LCSC unit, cited verbatim in BOM.md):** ALS-PT19 phototransistor ≈ 0.116 · CD4051BM 0.96 · LED 0402 0.016 · B5819W diode 0.024 USD.

> **Gap note (to record):** costs are **international reference prices** (USD, verified 2026-06-05; single source `BOM.md` §3). **Pending from the team (small):** the standalone **Zircon** price (Robomov lists the kit at USD 529), which **motor** they use (TT ~USD 3 vs Pololu HP USD 23.95), the day's **ARS/USD exchange rate**, and the development **hours**. The local *landed* cost is **higher** due to Argentina's import restrictions.

---

# ZONE F — MAIN ROBOT PHOTO
*(Center, base — the poster's visual anchor. Satisfies "Photos/Images: abundant, labeled, and cited".)*

`[PHOTO: complete 2026 robot, top view showing the 3 omni wheels at 120° and the 3-board stack — Fig.5, label TOP/CENTRAL/DOWN with leader lines]`
`[PHOTO: side view showing the board stack (standoffs) and the motor mounting — Fig.6]`
`[PHOTO: detail of the 32-sensor line ring on the underside of the DOWN plate — Fig.7]`

> **Caption:** *Fig.5–7 — IITA 2026 robot. Original team photos (CC BY 4.0).*

---

# ZONE G — DATA, RESULTS & DISCUSSION (1/2): TEST → CHANGE ITERATIONS
*(Right column, upper half — mandatory "Data/Results/Discussion" element. Targets **Excellent**: meaningful test data + **MAJOR changes made as a result of testing** + a clear test→evaluation→change link.)*

**EXACT TEXT (printed):**

> ## DATA, RESULTS & DISCUSSION
> Each row is a **real** bench iteration: we measure → evaluate → **change the source in a single point** → re-verify with the host suite.

> | # | Observed problem | Measured data (bench) | Applied change |
> |---|---|---|---|
> | 1 | **The 4 ToF clash on I²C** (they all boot at 0x29); PCB rev 1.0 did not route XSHUT (8 *No-Connect*) | Schematic forensics: 0 XSHUT nets. After *bodge* + **power-cycle**: 4 LP on pins {9,10,11,12} enumerate to **0x2A–0x2D** | Migrate the 4 ToF to the single `Wire` bus; free `Wire1` for DOWN; **unblocks 2D localization** |
> | 2 | **BNO heading freezes** in production (yaw stuck at −108.3°) even though the snapshot arrives healthy | BNO + ToF cannot coexist at 400 kHz; at **100 kHz + BNO @20 Hz** the heading survives (band-aid) | I²C at 100 kHz + BNO read at 20 Hz; design decision: the goalkeeper's heading-hold uses the **OTOS** (local), not the BNO |
> | 3 | **OTOS odometry** reports garbage on an A4 sheet | A4 with film: 28.6/300 mm (9.5%); A4 without film: 0.3 mm; **corrugated cardboard: 280.4/300 mm = 6.5% error** (< 8% tol.) | Remove the film + require a textured floor → **10× improvement**; the RCJ green field already meets it |
> | 4 | **Referee did not reach the Teensy** (TOP was waiting for a UART frame the COMM never emits) | The official COMM delivers START/STOP as a **3.3 V GPIO level**; in PLAY a **single** pin goes high | Read pins 5/6 with `INPUT_PULLDOWN`, `match_running = pin5 **OR** pin6`; fail-safe to STOP. **The referee moved the robot for the first time** |
> | 5 | **Motor 2 (U17) spins inverted** due to HW (INA/INB swapped) | Wheel-by-wheel bench (María/Elías) | `MOTOR_INVERT = {+1, −1, +1}` applied in **a single place** |
> | 6 | In lateral *strafe* **only motor 1 spins** | At `vx=150`/`MAX=1000`, M1 and M2 receive ~13% PWM (33/255) → below start-up; **M3 at 180° projects 0 (correct)** | Diagnosis: PWM deadzone + kiwi geometry (not a pin bug). Proposed `MOTOR_MIN_PWM` 25–45 per robot |
> | 7 | **int16 overflow of omega** (CRITICAL): clamp 360 → 36000 centideg > 32767 → spin **fully inverted** | Audit with 13 subagents; verified against the code | `HeadingPID.output_clamp` 360 → **327** (327·100 < 32767); anti *sign-flip* test |
> | 8 | **CENTRAL blind to the line**: it decoded the old contract (5 B) and discarded all 16 B frames | `payload_len==5` rejected **100%** of the real frames | Migrate to `LineStatusV2` (16 B); g++ harness 8/8 PASS |

> **Discussion:** major changes **#1, #4, and #8 unblocked entire capabilities** (2D localization, referee homologation, boundary braking). The parallel audit (20 subsystems, adversarial review) closed at **15/20 "solid", 0 critical**.

---

# ZONE H — DATA (2/2): REPEATABLE TEST METHODS + CHARTS
*(Right column, lower half — mandatory "repeatable testing methods" element. Targets **Excellent**: the method described so **others can repeat it**, with charts/tables.)*

**EXACT TEXT (printed):**

> ## TEST METHODS (repeatable by any team)
> **M1 — Host-native verification (without the board).** The logic lives in pure C++ modules; they are compiled with
> `g++ -std=gnu++17 -I src/shared lib/Unity/src/unity.c src/shared/*.cpp test/test_X/*.cpp` and the binary is run.
> **Result: 658 tests / 47 suites / 0 failures** (measured 2026-06-05 19:50 ART via `scripts/run-host-tests.sh`). **Living figure —** we re-measure it every session and it keeps climbing; that's why it carries a date and time and never goes stale. It dodges the antivirus that was blocking PlatformIO.
>
> **M2 — Mandatory power-cycle on I²C bring-up.** The VL53L7CX and OTOS addresses **persist with 3V3**; a reset is not enough. Protocol: *flash → cut and restore power (10 s) → open monitor*. (Without this: a false negative "no sensor responds".)
>
> **M3 — Odometry on a textured surface.** Move a controlled 300 mm and compare; tolerance 8% (result: 6.5%). Script `diag_otos_move_test.py`.
>
> **M4 — Diagnostics that reuse the production parsers.** The ~40 bench sketches **do not reimplement** the decoder: they validate `payload_len` against `sizeof` and detect *staleness*/CRC/SEQ-gap.

`[CHART: bars of the test-suite growth — 180 → 246 → 262 → 324 → 354 → 545 → 658 — Fig.8 · file docs/competencia/assets/fig8_test_growth.png (gen_figuras.py)]`
`[CHART: bars of OTOS odometry error by surface — A4-film 9.5% error, A4-clean 0%, cardboard 6.5% — Fig.9 · file docs/competencia/assets/fig9_otos_error.png (gen_figuras.py)]`
`[PHOTO: bench session with a serial monitor decoding a WorldSnapshot / diag_central_motors — Fig.10]`
`[PHOTO: the bodge of the 4 ToF LP wired to GPIO 9/10/11/12 (strong visual story) — Fig.11]`

> **Gap note:** the CPU loads (~20/25/22%) and latencies (<15 ms) are **design targets**, not oscilloscope measurements (TASK-014). Measure before asserting them as data.

---

# ZONE I — PRESENTATION PLAN
*(Right column, base — targets the **Presentation** criterion: present for the WHOLE session + actively engaged with judges/participants/guests + answers every question.)*

**EXACT TEXT (printed, brief):**

> ## VISIT OUR ROBOT
> We demonstrate live: **(1)** the suite of **658 tests / 47 suites / 0 failures** running on the laptop, **(2)** the **<15 ms** boundary braking, **(3)** the anticipating goalkeeper. Ask us how to replicate any of them.
> `[QR to repo]`  ·  `[QR to TDP video <3 min]`

**Internal team checklist (NOT printed) to secure Excellent in Presentation:**
- The **4 members** present and rotating; each one masters their domain (vision / motors / PCB / integration).
- Live bench: laptop with `run-host-tests.sh` ready + a bench diag to show the decoder.
- Question bank rehearsed by category (General, Electrical, Mechanical, Strategy, Software, Development & Documentation).
- Material to give away/share (QR, one-pager) → adds to Sportsmanship and Community.

---

# ZONE FOOTER — CREDITS, LAYOUT & LICENSE
*(Full-width bottom band — closes the **Layout** criterion (consistent typefaces, no errors) and **Photos** (cited).)*

**EXACT TEXT (printed, small):**

> **License:** code and hardware under **MIT** © 2026 IITA / Fundación Innovar.
> **Images:** Fig.1, 5–7, 10–11 original team photos (CC BY 4.0). Fig.2–4, 8–9 original diagrams generated by the team. Zircon schematic © Robomov (used with attribution).
> **Repo:** https://github.com/IITA-Proyectos/open-soccer-robocup-team2026

**LAYOUT specification (design guide, NOT printed):**
- **Typefaces (2, consistent):** titles in a geometric *sans* (e.g., Montserrat/Inter Bold); body in a legible humanist *sans* (e.g., Inter/Source Sans). Sizes: zone titles ≥48 pt, subtitles ≥32 pt, body ≥24 pt (legible at 1.5 m).
- **Fixed palette (4 colors):** deep blue (TOP), orange (CENTRAL), green (DOWN) + neutral gray background. Each board zone uses its color to build a mental map.
- **Figure numbering** Fig.1…Fig.11 consistent; each figure with its **labeled and cited caption**.
- **Mandatory spell check** after translating to English (the Layout criterion requires *no spelling errors*).
- **Original design** (not a generic template): the per-board color code + the data-flow diagram are the poster's creative "signature".

---

# FINAL CHECKLIST BEFORE PRINTING (NOT printed)

- [ ] **Spell-check** the English text (hard rubric requirement: no spelling errors).
- [x] ✅ Identity COMPLETE 2026-06-05: team IITA Low Battery Messi · region Roboliga Argentina 2025 (UAI) · roster (María Virginia Viollaz / Elías Cordero + coach Enzo Juárez Velázquez / mentor Cecilia Budeguer).
- [ ] **International reference costs loaded** (≈USD 1,000 all-new / 770 reusing CENTRAL). Pending from team: **standalone Zircon price, motor, ARS/USD rate, and hours**.
- [ ] Shoot and place **all `[PHOTO:]`** (Fig.1–11), labeled and cited.
- [x] ✅ Generated 2026-06-05 (PNG @300dpi): **Fig.2** (fig2_dataflow.png), **Fig.4** (fig4_fsm.png), **Fig.8–9** (fig8/fig9). Only A1 layout remains.
- [ ] Confirm the **live test count** at closing (verified **658 tests / 47 suites / 0 failures** on 2026-06-05 18:39 ART via `scripts/run-host-tests.sh`; re-run before printing).
- [ ] Verify the poster fits **A1 landscape (≤70.7×100 cm)** and is legible at 1.5 m.
- [ ] Generate the **QR codes** (repo + TDP video).
