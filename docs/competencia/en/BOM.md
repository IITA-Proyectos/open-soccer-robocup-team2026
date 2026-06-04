# Bill of Materials (BOM) — Major Components
## [TEAM NAME] · RoboCupJunior Soccer **Open** · Incheon 2026

> 📋 **Official template:** if RoboCupJunior publishes an **official BOM template** for the Soccer 2026 TDP/poster, **transcribe this table into that template** before submitting (check `robocup-junior.github.io/soccer-rules/master/` and the online TDP form). This BOM already includes every column the rubric requires for *major components*: **Component · Part number/model · Quantity · Source/supplier · New vs reused · Kit vs custom · Unit cost · Total cost**.

---

**Why this document exists (rubric mapping):** the RCJ Soccer 2026 rubric explicitly requires, in both the **Poster → Method/Design** and the **TDP → Electrical/Mechanical**, a **major-components BOM with justified component selection, cost, and development time**. This document is the single source of that data; the poster and TDP reference this table instead of duplicating it. Each section below carries a heading that maps **1:1** to a rubric criterion so the judge can find it at a glance.

---

## 0. Identification (Poster → Title/Identification · target level: Excellent)

| Field | Value |
|---|---|
| Team | **[TEAM NAME]** (internal org.: **IITA** — Instituto de [Innovación/Informática] y Tecnología Aplicada — VERIFY legal name / Fundación Innovar) |
| Region | **[REGION]** — Salta, **Argentina** (confirm the name of the regional/super-regional event they qualified through) |
| League / Sub-league | RoboCupJunior **Soccer — Open League** |
| Event | RoboCup 2026, **Incheon, South Korea** (Jun 30 – Jul 6, 2026) |
| Qualification | National champions, Roboliga Argentina (December 2025, Buenos Aires) |
| Robots | **2** — ROBOT1 = goalkeeper, ROBOT2 = striker |
| License / open-source | **MIT** (Copyright 2026 IITA / Fundación Innovar) — public repo: https://github.com/IITA-Proyectos/open-soccer-robocup-team2026 |

---

## 1. Major-components BOM per robot (TDP → Electrical · Poster → Method/Design · target level: Excellent)

> **How to read the table.** Quantities are **per robot** unless noted. The **New/Reused** column distinguishes what the team designed/bought new for 2026 from what it **reuses** from the 2025 national-champion robot (a **sustainability** angle rewarded by the rubric). **Kit/Custom** distinguishes a purchased module (COTS) from a board/part fabricated by the team. The **real unit costs** come from the repo's EasyEDA/LCSC manufacturing BOMs (`hardware/electronics/.../BOM_*.csv`, dated 2026-04-12/20) — they are **cited verbatim**. Where the repo carries **no** price, it is marked **`[COST — pending]`** and recorded in §6 (Gaps).

### 1.1 Compute / control (processors)

| Component | Part number / model | Qty | Source / supplier | New / Reused | Kit / Custom | Unit cost | Total cost |
|---|---|---|---|---|---|---|---|
| TOP board MCU | **Teensy 4.0** (LCSC `C99001332551`) — Cortex-M7 @600 MHz | 1 | PJRC / LCSC | New | Module (kit) on custom PCB | **[COST — pending] (est. ~USD 24–30)** | [COST — pending] |
| DOWN board MCU | **Teensy 4.0** (LCSC `C99001332551`) | 1 | PJRC / LCSC | New | Module (kit) on custom PCB | **[COST — pending] (est. ~USD 24–30)** | [COST — pending] |
| CENTRAL board MCU | **Teensy 4.1** (Cortex-M7 @600 MHz) | 1 | PJRC | **Reused** (from the 2025 National robot) | Module (kit) on Zircon PCB | **[COST — pending] (est. ~USD 32–40)** | [COST — pending] |
| COMM board MCU | **ESP32-C6-MINI-1-N4** (Espressif; RISC-V, WiFi 6 / BLE 5 / 802.15.4, native USB, 4 MB flash) | 1 | Espressif (on COMM PCB) | New | SMD on custom PCB (RCJ fork) | **[COST — pending] (est. ~USD 3–5)** | [COST — pending] |

### 1.2 Perception sensors (cameras + IMU + ranging)

| Component | Part number / model | Qty | Source / supplier | New / Reused | Kit / Custom | Unit cost | Total cost |
|---|---|---|---|---|---|---|---|
| Vision camera | **OpenMV Cam N6** (STM32N6 Cortex-M55 + Neural-ART NPU, PAG7936 sensor, QVGA RGB565 ~30 Hz) | 2 | OpenMV | New | Module (kit) | **[COST — pending]** (most expensive item on the robot, ~USD 80–200 each) | [COST — pending] |
| IMU (heading/yaw) | **Bosch BNO055** (designators U10/U11) | 2 | Bosch (module) | New | Module (kit) on 4P header | **~USD 15** (internal citation `ARQUITECTURA-3-PLACAS-2026.md` L326) | **~USD 30** ⚠️ see note |
| Multizone ToF sensor | **ST VL53L7CX** (8×8 zones, 60° FoV, Pololu module) | 4 | STMicro / Pololu | New | Module (kit) | **[PRICE — verify]** (see ⚠️ mismatch note) | [COST — pending] |
| Ultrasonic | **HC-SR04** (designator U6 on TOP) | 1 | generic | New | Module (kit) | **[COST — pending] (est. ~USD 1–3)** | [COST — pending] |

> ⚠️ **BNO055 note:** the repo mounts **2 BNO055** units but **1 unit (RIGHT, 0x29) is FAULTY**; the robot currently competes with **1 healthy BNO + 4 ToF**. For replicability/spares, plan for **2–4 units** (Incheon). The `~USD 15` price is the only concrete figure in the repo (qualitative).

### 1.3 Odometry and floor sensors (DOWN board)

| Component | Part number / model | Qty | Source / supplier | New / Reused | Kit / Custom | Unit cost | Total cost |
|---|---|---|---|---|---|---|---|
| Optical odometry | **SparkFun OTOS** (Optical Tracking Odometry Sensor; designators U5/U6) | 2 | SparkFun | New | Module (kit), header | **[COST — pending] (est. ~USD 50 each)** | [COST — pending] |
| Line phototransistor | **Everlight ALS-PT19-315C/L177/TR8** (LCSC `C146233`) | 32 | Everlight / LCSC | New | SMD on custom PCB | **USD 0.116** | **USD 3.71** |
| 0402 emitter LED (ring) | **YLED0402O** (LCSC `C28310436`) | 32 | Yongyutai / LCSC | New | SMD on custom PCB | **USD 0.016** | **USD 0.51** |
| 8:1 analog multiplexer | **TI CD4051BM** (LCSC `C353976`) — 4 muxes × 8 channels = 32 | 4 | TI / LCSC | New | SMD on custom PCB | **USD 0.96** | **USD 3.84** |
| LED current-limit resistor | 330 Ω 0603 | 32 | LCSC | New | SMD | **[COST — pending] (est. <USD 0.01)** | [COST — pending] |
| Phototr. bias resistor | 10 kΩ 0603 | 32 | LCSC | New | SMD | **[COST — pending] (est. <USD 0.01)** | [COST — pending] |

### 1.4 Actuators (KIWI omni drive)

| Component | Part number / model | Qty | Source / supplier | New / Reused | Kit / Custom | Unit cost | Total cost |
|---|---|---|---|---|---|---|---|
| Drive motor | **DC "TT" motor** (model/torque/RPM **[SPEC?]**; 3-wheel KIWI base at 120°) | 3 | generic | **[New/Reused?]** — 2025 used "TT motors" | Module (kit) | **[COST — pending]** (~USD 2–5) | [COST — pending] |
| Motor driver (H-bridge) | **3 H-bridges integrated on the Zircon PCB** (drivers U5/U7/U17; INA+INB+8-bit PWM) | 3 | Robomov (on Zircon) | **Reused** (part of the Zircon) | Custom (on Zircon shield) | included in Zircon | included |
| Omnidirectional wheel | **Omni wheel** (Ø/material/rollers **[SPEC?]**; the repo cites 48/58 mm only as a generic example) | 3 | **[3D-printed or purchased?]** | **[New/Reused?]** | **[Kit/Custom?]** | **[COST — pending]** | [COST — pending] |
| Kicker / solenoid | **NONE** — the robot **has no physical kicker** (it pushes the ball by inertia) | 0 | — | — | — | — | **USD 0** |

> ✅ **Design decision (sustainability + simplicity, award-worthy):** **no kicker** → fewer components, less energy, fewer points of failure. The striker pushes by inertia once aligned with the opponent's goal (logic in `src/shared/behind_ball.{h,cpp}`, 16 tests).

### 1.5 Power (common to all 3 boards)

| Component | Part number / model | Qty (per robot) | Source / supplier | New / Reused | Kit / Custom | Unit cost | Total cost |
|---|---|---|---|---|---|---|---|
| Battery | **LiPo 2S 7.4 V nominal** (mAh / C-rating / brand **[SPEC?]**) | **[1–2?]** | generic | New | Module (kit) | **[COST — pending] (est. ~USD 10–25)** | [COST — pending] |
| Battery connector | **Deans-T-F (XP1)** | 3 (1/board) | generic | New | Component | **[COST — pending] (est. <USD 1)** | [COST — pending] |
| Schottky diode (protection) | **B5819W SL** (LCSC `C8598`, 1 A/40 V; OR-ing/polarity) | 6 (2/board) | CJ / LCSC | New | SMD on custom PCB | **USD 0.024** | **USD 0.14** |
| Buck regulator | **MP1584-EN** (4-pin SIP module; 5 V logic + 3.3 V sensor rails) | 6 (2/board) | generic | New | Module (kit) | **[COST — pending] (est. ~USD 0.5–1 each)** | [COST — pending] |
| 3.3 V LDO (COMM only) | **TI UA78M33CDCYR** (SOT-223) | 1 | TI | New | SMD on COMM PCB | **[COST — pending] (est. ~USD 0.3)** | [COST — pending] |

### 1.6 Custom PCBs (fabricated by the team) and reused board

| Component | Part number / model | Qty | Source / supplier | New / Reused | Kit / Custom | Unit cost | Total cost |
|---|---|---|---|---|---|---|---|
| **TOP** PCB | `Roboliga2026_TOP` (224.0 × 97.5 mm, 2 layers; EasyEDA → JLCPCB) | 1 | JLCPCB (IITA design) | **New** | **Custom** | **[COST — pending] (est. ~USD 6–10 prorated from a batch of 5)** | [COST — pending] |
| **DOWN** PCB (= base plate) | `Roboliga 2026 Futbol` REV 1.0 (~175.1 × 165.7 mm, plate-shaped outline, **it IS the structural chassis**) | 1 | JLCPCB (IITA design) | **New** | **Custom** | **[COST — pending] (est. ~USD 6–10 prorated)** | [COST — pending] |
| **COMM** PCB | `PCB1` (25.40 × 31.20 mm; IITA fork of the official RCJ `soccer-communication-module`) | 1 | JLCPCB (IITA design) | **New** | **Custom** (open-source RCJ fork) | **[COST — pending] (est. ~USD 2–5 prorated)** | [COST — pending] |
| **CENTRAL** PCB/shield | **Zircon Rev v15** (commercial PCB by **Robomov**, robomov.net; public schematic `Zircon.pdf`) | 1 | Robomov | **Reused** (the board that won the 2025 Nationals) | **Purchased (commercial COTS)** | **[COST — pending] (Robomov price)** | [COST — pending] |

> 🔁 **Sustainability / reuse (award-worthy):** the **Zircon Rev v15 + Teensy 4.1** is the brain that **won the 2025 Nationals**; the new boards (TOP/DOWN) are **mounted around it**, not replacing it. If a new board fails in Incheon, CENTRAL **degrades to monolithic mode**. The design is **upgradable post-Incheon** (replace one board without touching the others; a better camera = only the TOP firmware changes).

---

## 2. Component-selection justification (TDP → Electrical: "DATA-driven reasoning" · target level: Excellent)

The rubric rewards **data-driven design decisions and trade-offs**, not just the list. Summary of the decisions with quantitative backing (full detail in the cited docs):

| Decision | Alternatives evaluated | Data / criterion | Choice |
|---|---|---|---|
| **2D localization: 4× VL53L7CX (ToF) vs LiDAR vs EKF/MCL** | LiDAR (~USD 100), ToF array (~USD 26 ⚠️ see note), EKF (±0.5–1 cm / 3–5 days), Particle Filter (~500 µs, "overkill for a 1.83×2.43 m field with 4 orthogonal walls") | ToF: **±2–3 cm**, negligible CPU, **1 day** of dev, **~USD 26** vs **USD 100** for the LiDAR | **Geometric trilateration with 4 ToF** (`docs/lidar-tof-slam-analysis.md`, `research/.../2026-05-25-localizacion-tof-imu-analisis.md`) |
| **2× BNO055** (heading redundancy) | 1 IMU | "two chips at ~USD 15 each; far superior reliability" | 2 IMUs (1 healthy today; see gap) |
| **2× OTOS** (optical odometry) | wheel encoders | measures **lateral slip and rotation** directly off the floor; bench: 300 mm real → 280.4 mm (6.5 % error, passes the 8 % tolerance) — see error-by-surface chart in `docs/competencia/assets/fig9_otos_error.png` | 2 OTOS dual-bus |
| **MCU: Teensy 4.x (Cortex-M7 @600 MHz)** | ESP32, smaller STM32 | each MCU runs at **<30 % CPU** (headroom for Kalman/EKF/coordination) | Teensy 4.0/4.1 |
| **No kicker** | solenoid (2025 had one) | fewer components/energy/failures; push by inertia | removed from firmware 2026-06-03 |
| **OpenMV N6** (vs H7 Plus) | OpenMV H7 Plus | Neural-ART NPU for color vision; documented HW constraint (use `sensor`+`pyb.UART`) | 2× N6 |

> ⚠️ **ToF price mismatch (fix before submitting):** the **~USD 26** cost analysis for the ToF array cited above corresponds to the **VL53L1X (single-zone)**, but the robot **mounts the VL53L7CX (8×8 zones)**, which is a more expensive chip. The **~USD 26** price **does not apply** to the sensor actually mounted. **Obtain the real VL53L7CX price** (Pololu module) and mark it **[PRICE — verify]**; the design decision (ToF vs LiDAR) remains valid, but the cost figure must be corrected.

---

## 3. Development cost and time (Poster → Method/Design: "development time and cost" · target level: Excellent)

### 3.1 Estimated total cost

| Item | Cost |
|---|---|
| Subtotal of components **with a real price in the repo** (line ring + muxes + diodes, per robot): ALS-PT19 (3.71) + 0402 LED (0.51) + CD4051 (3.84) + B5819W (0.14) | **≈ USD 8.20** |
| Remaining major components (MCUs, N6 cameras ×2, OTOS ×2, BNO055 ×2, VL53L7CX ToF ×4 [PRICE — verify], motors ×3, Zircon, battery, MP1584 ×6, etc.) | **[COST — pending]** (no price in the repo; rough estimate USD 400–700/robot, dominated by the 2 N6 cameras and the 2 OTOS) |
| **TOTAL COST per robot** | **[TOTAL — pending]** |
| **TOTAL COST for 2 robots** | **[TOTAL — pending]** |
| Conversion to local currency (ARS) | **[TOTAL — pending]** |

> The repo has **no** consolidated cost BOM: only **LCSC unit prices in USD** for the passives/ICs of the custom boards (cited above). **Obtain from the team** the prices for Teensy ×3, OpenMV N6 ×2 (the most expensive), OTOS ×2, BNO055 ×2–4, VL53L7CX ×4, motors ×3, battery, Zircon (Robomov price), plus the **per-robot total + 2-robot total + ARS conversion**.

### 3.2 Development time (traceable in journals)

| Milestone | Date | Evidence |
|---|---|---|
| Project kickoff | 2026-02-21 | `journal/2026-02-21-project-kickoff.md` |
| 3-board firmware start (Plan A) | 2026-05-10/11 → 2026-06-28 (8 weekly milestones) | `journal/2026-05-10-fase-0-y-arranque-fase-1.md` |
| 3 physical boards exist ("robot mechanically/electronically near-complete") | 2026-05-29 | `ESTADO-ACTUAL.md` L478-481 |
| DOWN bring-up (32 sensors + OTOS) | 2026-05-24 | `journal/2026-05-24-*` |
| TOP bring-up (4 ToF on a single bus, BNO) | 2026-05-25 → 05-31 | `journal/2026-05-30-top-tof-4-en-bus-unico-*` |
| Referee homologated (moves the robot end-to-end) | 2026-06-02/03 | `journal/2026-06-03-banco-*` |
| **Total engineering effort** | **≈ 4 months** (Feb–Jun 2026), agent-assisted development across branches | journals |

> 💡 **Sellable process metric:** **a host-native test suite that grows traceably session by session** — 180 → 246 → 262 → 324 → 354 → 470 → **545 tests / 40 suites / 0 failures (verified 2026-06-04 via `scripts/run-host-tests.sh`)**. See the growth chart in `docs/competencia/assets/fig8_test_growth.png` (generated by `gen_figuras.py`).

---

## 4. Per-board summary (TDP → Mechanical/Electrical: subsystem traceability · target level: Excellent)

| Board | MCU | Role | Key sensors/actuators | New/Reused | PCB |
|---|---|---|---|---|---|
| **TOP** | Teensy 4.0 | Perception / "sensory brain" | 2× OpenMV N6, 2× BNO055, 4× VL53L7CX, 1× HC-SR04, reads referee via GPIO | New | Custom `Roboliga2026_TOP` |
| **CENTRAL** | Teensy 4.1 | Decision / tactical FSM + motors | 3× H-bridge → 3 KIWI omni motors | **Reused** (2025 champion) | **Zircon Rev v15 (Robomov COTS)** |
| **DOWN** | Teensy 4.0 | Floor / odometry (= **base plate**) | 32-sensor line ring (4× CD4051), 2× OTOS | New | Custom `Roboliga 2026 Futbol` |
| **COMM** | ESP32-C6 | RCJ referee + partner | TXS0102 level shifter, LIS3DH accelerometer | New | Custom `PCB1` (RCJ fork) |

---

## 5. Open-source and BOM replicability (Documentation & Community Contribution + TDP Bonus +1/+1 · target level: Excellent)

The rubric grants **+1 bonus for open-sourcing CAD/PCB/schematics** and **+1 for open-sourcing the software**, plus the **Documentation & Community Contribution (5 pts)** criterion. This BOM enables **replicability** (the RCJ gold standard):

- **Fabricable custom PCBs:** the **complete EasyEDA projects** (SCH JSON + PCB JSON + PDF + **BOM CSV** + Pick&Place + gerbers + FlyingProbeTesting) for **TOP** and **DOWN** are in `hardware/electronics/pcb_design/{top_board,down_board}/` and `gerber_file/`. Another team can **re-fabricate TOP and DOWN as-is** (JLCPCB).
- **CENTRAL (Zircon):** commercial **Robomov Rev v15** board with a **public schematic** (`hardware/electronics/Zircon.pdf`, robomov.net) → replicable by buying it or from its schematic.
- **COMM:** **open-source fork** of the official RCJ module (`IITA-Proyectos/rcj-soccer-open_communication_module`).
- **Software:** C++/Python firmware under the **MIT** license, with reproducible **host-native testing** (`scripts/run-host-tests.sh`) and a **100 % offline** build.
- **The real manufacturing BOMs** (with LCSC part numbers and unit prices) are version-controlled: `hardware/electronics/pcb_design/down_board/BOM_Roboliga_2026_Futbol_2026-04-12.csv`, `hardware/electronics/pcb_design/top_board/BOM_Roboliga2026_TOP_2026-04-12.csv`, `hardware/electronics/gerber_file/Placas/Comm/BOM_Board1_PCB1_2026-04-20.xlsx`.

---

## 6. Missing data (Gaps) — complete before submitting poster/TDP

> Mark each one as **real data to obtain from the team**; without it, the poster/TDP loses points in *Method/Design* (cost/time) and *Documentation* (replicability).

| # | Gap | Type |
|---|---|---|
| 1 | **[TEAM NAME]** officially registered for RoboCup Incheon | Identification |
| 2 | **[REGION]** — name of the qualifying regional/super-regional event | Identification |
| 3 | **[COSTS]** real prices for: Teensy 4.0 ×2 + 4.1, **OpenMV N6 ×2 (the most expensive)**, OTOS ×2, BNO055 ×2–4, VL53L7CX ×4, HC-SR04, ESP32-C6, **Zircon (Robomov price)**, LiPo battery, MP1584 ×6, UA78M33, motors ×3, omni wheels ×3 | Cost |
| 4 | **[TOTAL COST]** per robot, total for 2 robots, and **ARS conversion** | Cost |
| 5 | **[motor SPEC]** model/voltage/RPM/torque/gear ratio/encoder of the 2026 TT motor | Mechanical |
| 6 | **[wheel SPEC]** Ø/material/number of rollers/source (printed or purchased) of the 2026 omni wheel | Mechanical |
| 7 | **[battery SPEC]** capacity (mAh), C-rating, brand, weight, number of packs/robot | Electrical |
| 8 | **[New/Reused motors/wheels]** confirm whether the drivetrain is new for 2026 or reused from 2025 | Sustainability |
| 9 | ✅ Final test figure **resolved**: **545 tests / 40 suites / 0 failures (verified 2026-06-04 via `scripts/run-host-tests.sh`)** — figure in `docs/competencia/assets/fig8_test_growth.png` | Process |
| 10 | **[PHOTO]** of each populated PCB (TOP/DOWN/Zircon/COMM) and of the assembled robot to label on the poster | Images |
| 11 | **[GAP]** real MP1584 set-points (trimpot, not yet measured) and PCB cost prorated from the JLCPCB batch | Electrical |
| 12 | **[TEMPLATE]** verify whether an official RCJ BOM template exists and transcribe | Format |

---

### Sources (cited, original team material)
- `hardware/electronics/pcb_design/down_board/BOM_Roboliga_2026_Futbol_2026-04-12.csv` (LCSC prices verbatim)
- `hardware/electronics/pcb_design/top_board/BOM_Roboliga2026_TOP_2026-04-12.csv`
- `hardware/electronics/gerber_file/Placas/Comm/BOM_Board1_PCB1_2026-04-20.xlsx` (COMM manufacturing BOM); `hardware/electronics/comm-board/2026-05-17-placa-comm-componentes-y-circuito.md` (COMM BOM in text, 12 lines)
- `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md`, `docs/ARQUITECTURA-3-PLACAS-2026.md` (L326: BNO055 ~USD 15)
- `docs/lidar-tof-slam-analysis.md` (USD 100 LiDAR vs USD 26 ToF array)
- `README.md` (team, license, qualification)
