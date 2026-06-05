# Group Team Interview — Team Cheat-Sheet (RoboCupJunior Soccer Open 2026, Incheon)

> **What the Group Team Interview is (5 pts):** a live challenge run together with 1-3 teams. It has 3 parts:
> **(1) Show & Tell**, **(2) Teamwork-Task** (the judges set a programming task on the spot) and
> **(3) Questions** (a question bank organized by category). It is scored against 3 criteria, each on a
> 0/1/3/5 scale. **We aim for Excellent (5) on all three.**
>
> **Golden rule of the interview (what the judge is watching for):** that **EVERYONE** contributes, that there
> is real **technical fluency** (not recitation) and **innovative approaches**. Nobody monopolizes, nobody goes silent.
>
> **Language note:** the interview is conducted in English (the language of the competition). This sheet is the
> English version; the Spanish working version lives at `docs/competencia/ENTREVISTA-PREP.md`. Rehearse the model
> answers out loud before Incheon.

---

## How it is scored (1:1 map to the rubric — this is what the judge looks for)

| Rubric criterion | What Excellent (5) is | Where we cover it in this sheet |
|---|---|---|
| **Teamwork & Communication** | Fluid collaboration, members support each other, **clear roles**, **EVERYONE contributes** | §2 (role assignment) + §1 (each person speaks in the Show & Tell) + §5 (hand-off protocol) |
| **Technical Understanding** | **Strong technical fluency** and problem-solving | §4 (model answers by category, with real numbers) |
| **Task Execution** | Completes **efficiently** with **innovative approaches** | §3 (Teamwork-Task tips: how to split up, where the code lives, fast flashing) |

> **Placeholders to fill in before Incheon** (logged in the gaps at the end): `IITA Low Battery Messi`, final roles for
> `María Virginia Viollaz`/`Elías Cordero`, and confirm that 2 people compete in Incheon (repo roster: Gustavo Viollaz director,
> Enzo Juárez Velázquez coach, María Virginia Viollaz and Elías Cordero competitors). In the interview **the competitors who
> are present speak**; the coach does not answer for them.

---

## §1 — Show & Tell (60-90 s) → targets Teamwork & Communication (Excellent)

> **Goal:** within 90 s the judge must see (a) a real robot, (b) ONE differentiating idea and (c) that **the team
> works as a team**. Anti-monologue strategy: **each member delivers one part**, chained together. Bring the robot
> powered on (or one robot + a laptop with the green test suite running in the background).

**Split script (timed, 2 voices — adapt if there are more competitors):**

| Time | Who | Line |
|---|---|---|
| 0-15 s | `María Virginia Viollaz` | "We are `IITA Low Battery Messi`, from Salta, Argentina, in the **Open** sub-league. We arrived in Incheon as the **national champions** of the 2025 Roboliga Argentina. We bring **2 robots**: a **goalkeeper** and a **striker**." |
| 15-40 s | `María Virginia Viollaz` | "Our robot uses a **distributed 3-board architecture**: one board **perceives** (2 OpenMV N6 cameras, 1 IMU, 4 ToF sensors), one board **decides** (tactical FSM + 3 omni motors) and one board **touches the floor** (a 32-sensor line ring + 2 optical odometry sensors). They talk to each other over UART at 230400 baud." |
| 40-65 s | `Elías Cordero` | "What we are proudest of is **how we verify the firmware without the board**: the decision logic lives in pure C++ modules that we compile and test on the PC with g++. Today we run **652 host-native tests in 47 suites, 0 failures** (measured 2026-06-05 18:39 ART with `scripts/run-host-tests.sh`). That lets us **iterate fast and safely** days before the competition." |
| 65-85 s | `Elías Cordero` | "And a tactical decision we are proud of: the **goalkeeper anticipates**. Instead of tracking the ball's current position, it projects where it **is going to be** using its velocity (`pos + v·0.2 s`, capped). We can show it to you on the field if you'd like." |
| 85-90 s | both | "Everything is **open-source under the MIT license** on GitHub, documented so another team can replicate it. Where would you like to start?" |

**Delivery tips (these push Teamwork & Communication toward Excellent):**
- **Explicit hand-off**: "...and that was mostly worked on by `Elías Cordero`, tell them about it" → shows clear roles and mutual support.
- Keep eye contact with **the judges AND the other teams** (the rubric rewards engagement with everyone present).
- Hold the **physical robot in hand** and point at the 3 boards as they are named (the boards are stacked like floors: TOP / CENTRAL / DOWN).
- Close with an **open-ended question** → it invites conversation instead of cutting it off.

---

## §2 — Technical role assignment → targets Teamwork & Communication (Excellent: "clear roles, everyone contributes")

> **Idea:** each competitor **owns an area** and is the "owner" of those questions. When a question comes in, the
> owner answers and the other one **adds a data point**, never talking over each other. Below is a proposed split
> based on the repo history (María/Virginia with experience in **vision and trajectories**; Elías in **robotics and
> electromechanical engineering**). **Confirm/adjust names before Incheon.**

| Area | Suggested owner | Why (repo evidence) | Typical question they answer without hesitation |
|---|---|---|---|
| **Vision + Strategy/Trajectories** | `María Virginia Viollaz` (vision + trajectories) | 2025 experience in computer vision and trajectories | "How do you detect the ball?" / "How does the goalkeeper decide where to go?" |
| **Electronics + Mechanics/Drivetrain** | `Elías Cordero` (robotics + electromechanical eng.) | Electromechanical Engineering student; motor bench work | "Why omni motors at 120°?" / "How did you choose the components?" |
| **Software / Architecture / Testing** | **shared** (both) | It is the team's differentiator; both must be able to explain the "pure modules + host tests" idea | "How do you test without the board?" / "How do the 3 boards communicate?" |
| **Development & Documentation** | **shared** | Engineering journal + SOURCES-OF-TRUTH + traceable tests | "How do you track progress?" / "How do you know something works?" |

**Team rule for the interview (say this if a judge asks "who did what?"):**
> "We work by area but with **one shared repository**: every change goes in with its entry in the **engineering
> journal** and, if it touches a critical data point, the **SOURCES-OF-TRUTH** table is updated in the same commit.
> That way none of us depends on the other's memory."

---

## §3 — Live Teamwork-Task (the judges set the task) → targets Task Execution (Excellent: "efficient + innovative")

> The judge gives a programming task on the spot (e.g.: "make the robot spin until it sees the ball and approach it",
> "make it stop when it crosses the line", "make it patrol side to side"). **Do not improvise the organization:
> follow this protocol.** What earns Excellent is showing a **method** (not chaos) and an **approach of our own**
> (reusing our pure modules + flashing fast).

### 3.1 — How we split up (say it out loud so the judge hears it)
1. **30 s of shared plan**: one person restates the task in their own words and proposes the approach ("this is basically the FSM in the APPROACH state / it's a lateral strafe / it's edge braking"). The other confirms or adjusts.
2. **Roles for the task**: one **writes the code**, the other **prepares the flash and watches the robot** (eyes on the hardware, not the screen). They swap if the task has 2 parts.
3. **Talk while doing**: narrate what is being touched ("I'm going to modify `strategy.cpp`, the APPROACH state, raising the approach speed"). The judge scores what they understand.

### 3.2 — Where the code lives (knowing this by heart = speed = Task Execution)

| What they want the robot to do | File to touch | Concrete hint |
|---|---|---|
| Decide what to do (chase, patrol, intercept) | `software/teensy/Soccer 2026/src/central/strategy.cpp` | Dual FSM: ATTACKER (KICKOFF/SEARCH/POSITION/APPROACH/LINE_AVOID) and GOALKEEPER (PATROL/INTERCEPT/CLEAR/LINE_AVOID) |
| Move the robot in a direction (vx, vy, ω) | `software/teensy/Soccer 2026/src/shared/kinematics.{h,cpp}` | omni-3 inverse kinematics: `v_i = -vx·sin(θ_i) + vy·cos(θ_i) + ω·R`. **+X=right, +Y=front, ω CCW+** |
| Apply PWM to the motors | `software/teensy/Soccer 2026/src/central/motors_zircon.{h,cpp}` | 8-bit PWM 0-255. `MOTOR_INVERT={+1,-1,+1}` (M2/U17 runs inverted in HW) |
| Tune a control loop | `software/teensy/Soccer 2026/src/shared/pids.{h,cpp}` | heading + lateral + distance. **WATCH OUT: HeadingPID clamp ≤327** (ω·100 is int16, 360 overflows) |
| Anticipate the ball (goalkeeper) | `software/teensy/Soccer 2026/src/shared/ball_predict.{h,cpp}` | `lookahead_s=0.2`, `max_lead_mm=400` (tunable) |
| Robot constants (speed, geometry) | `software/teensy/Soccer 2026/src/central/config_central.h` | `MAX_SPEED_MM_S=1000`, `WHEEL_ANGLES_DEG={60,-60,180}` (⚠️ tentative) |

### 3.3 — How we flash firmware fast (have this ready BEFORE)
- **Embedded build/flash**: `pio run -e central_robot1 -t upload` (or `top_robot1` / `down`). The environment compiles **100% offline** (vendored libs in `lib/`), so **we don't depend on the venue's internet**.
- **Instant board-free verification**: `bash scripts/run-host-tests.sh` → runs the 652 host tests in seconds (652 tests / 47 suites / 0 failures, measured 2026-06-05 18:39 ART). **Showing this to the judge is a winner**: "before uploading to the robot, we validate it on the PC". **Honest caveat:** the host runner compiles the **pure modules** (shared + down); the central/top tests use Arduino and compile **on-target** on the board.
- **Bench diagnostics**: there are ~40 sketches in `src/diag/` (`diag_central_motors`, `diag_central_strafe`, `diag_central_rx_all`...) that reuse the production parsers. If the task is "move a motor", `diag_central_motors` already does it.
- **Bring-up trick that avoids wasting time (say it if something doesn't respond):** the I²C sensors (ToF and OTOS) **retain their address while powered at 3.3 V** → if they don't show up, do a **real power-cycle** (cut battery + USB for ~10 s), not just a reset. Knowing this saves 20 min of live debugging.

### 3.4 — Innovative approach to show off (what pushes Task Execution to Excellent)
- "We're going to solve it by **reusing a pure module that is already tested** instead of writing new logic blindly" → efficiency + engineering judgment.
- "We add a **fallback**: if the new data isn't there, it does exactly what it did before" → this is our *byte-identical fallback* technique, zero regression. Very sellable.

---

## §4 — Questions: model answers by category → targets Technical Understanding (Excellent: "strong technical fluency + problem-solving")

> Every answer: **a concrete data point + the why + (where applicable) a real iteration**. The judge rewards us for
> giving **numbers** and telling **a data-driven decision**, not generalities. Answers marked with 💡 include an
> iteration story (test→data→change) — those are the ones that score highest.

### General (design decisions, inspiration)

**Q: Why did you choose a 3-board architecture instead of a single one?**
> "On a principle: **process where the sensor is and decide in the center**. The vision board processes the cameras
> and the IMU, the central board only receives a world summary (a *WorldSnapshot* of **31 bytes at 100 Hz**) and
> decides. Each microcontroller stays **below 30% CPU**, so we keep headroom for improvements. It is also **modular**:
> if we change the camera in 2027, we only touch that board's firmware."

**Q: What inspired you / where did the architecture come from?**
> "It is the standard pattern in mobile robotics (Middle Size League teams like CAMBADA use it). And we started from
> **what already worked for us**: the central board is the **Zircon** we won the 2025 Nationals with; we added the
> perception and floor boards around it without replacing it."

### Electrical (component selection, troubleshooting)

**Q: How did you choose the components?**
> "COTS and cheap criteria, buyable on LCSC: **Teensy 4.0/4.1** (Cortex-M7 at 600 MHz) as the brains, **VL53L7CX**
> multizone ToF for distance, **BNO055** for heading, **SparkFun OTOS** for optical odometry, and **OpenMV N6**
> cameras. We power it with a **2S LiPo (7.4 V)** → Schottky diode protection → 2 **MP1584** buck regulators per
> board (5 V logic, 3.3 V sensors). We characterized the OTOS drift on the bench — see the error chart in
> `docs/competencia/assets/fig9_otos_error.png`."

**Q (troubleshooting): Tell me about an electrical problem you had and how you solved it.** 💡
> "The **4 ToF sensors** all boot at the same I²C address (0x29) and collide on a shared bus. Investigating the
> schematic, we discovered the pins to enumerate them **were not routed** on the PCB. Enzo did a **bodge**: he wired
> each ToF's control pin to a Teensy GPIO. And we learned something key: their addresses **persist as long as they
> have 3.3 V**, so you have to **power-cycle**, not reset. After that, the 4 ToFs enumerate at 0x2A-0x2D and it
> unlocked **2D localization by trilateration**."

**Q: Why does one motor run inverted in the code?**
> "Motor 2's driver (U17) has its inputs crossed in hardware on the Zircon shield. Instead of rewiring, we fix it in
> **a single place in the firmware**: `MOTOR_INVERT={+1,-1,+1}`. We **validated it on the bench** by spinning each
> motor separately with `diag_central_motors`."

### Mechanical (features, materials, manufacturing)

**Q: How does the robot move?**
> "**KIWI omnidirectional** base: 3 omni wheels at **120°** with 3 DC motors. That gives us holonomic motion (it can
> go in any direction without turning). The inverse kinematics live in a pure module tested with **11 tests**, and it
> includes **proportional saturation**: if one wheel saturates, we scale all 3 equally so we don't distort the
> trajectory."

**Q: Do you have a kicker?**
> "No. The robot **pushes the ball by inertia** when the striker aligns with the opponent's goal (tolerance **12°**,
> at less than **80 mm**). It is a design decision: **fewer components, less energy, fewer failure points**. The logic
> is in the `behind_ball` module with 16 tests."

**Q: What is the chassis made of?**
> "The **structural base plate is literally the bottom PCB** (≈175 × 166 mm, rounded plate-like outline, M3
> mounting). The 3 boards stack like floors. `[GAP: confirm the materials for the rest of the chassis, the height
> between floors, and the 2026 printed parts — logged in gaps]`."

**Q (manufacturing): How did you manufacture it?**
> "We designed the **3 custom PCBs** in EasyEDA and they are manufacturable as-is (we have the gerbers + BOM). The
> central one is commercial (Robomov's Zircon). The mechanical parts are **3D printed**. `[GAP: upload the 2026
> chassis STLs — the ones in the repo are from 2025 with the now-discarded dribbler/solenoid]`."

### Strategy (positioning, tactics)

**Q: What does the goalkeeper do?**
> "It patrols the goal and, when the ball approaches, **intercepts by anticipating**: instead of going to the ball's
> current X, it goes to the **predicted** X = position + velocity × 0.2 s, capped at 400 mm. We compute the ball's
> velocity on the vision board via finite differences with an EMA filter. If the ball is still, the lead is 0 and it
> behaves like a normal goalkeeper — **automatic fallback**."

**Q: And the striker?**
> "It looks for the ball, **positions itself behind** it aligned with the opponent's goal, and **pushes**. The FSM
> has the states SEARCH → POSITION → APPROACH → push, with a `LINE_AVOID` state that **bypasses everything** if it is
> about to leave the field."

**Q: How do you avoid going out of bounds?**
> "A ring of **32 line sensors** on the floor board. When it detects an imminent exit, there is a **direct emergency
> bus** from that board to the central one (1 UART hop) to brake in **<15 ms** — at 1 m/s that's 15 mm. If we braked
> by going through the vision board (2 UARTs), we would already have crossed."

### Software (sensors, problem avoidance, robot-to-robot communication, debugging)

**Q: How do you process the sensors / build the world view?**
> "The vision board **fuses** 2 cameras + IMU + 4 ToF and builds a *WorldSnapshot* of 31 bytes: own pose, ball
> (position **and velocity**), goals, nearest obstacle and the referee command. It sends it to the central board at
> 100 Hz. The central board only consumes that summary — it never touches raw sensors."

**Q: How do the boards communicate? And what happens if a message is lost?**
> "UART with a **custom protocol**: `[START 0xAA | LEN | TYPE | SEQ | PAYLOAD | CRC-16 | END 0x55]`. The **CRC**
> detects corruption and the **SEQ** detects lost packets. The decoder is a **byte-by-byte state machine that
> re-synchronizes itself**: one garbage byte does not contaminate the next frame. And there are **watchdogs**: if a
> stream doesn't arrive within 500 ms, the central board goes into safe mode."

**Q: How do you debug? Tell me about a real software bug.** 💡
> "We have ~40 diagnostic sketches that reuse the production parsers. A real bug: the central board went **blind to
> the line** because it was decoding the old format (5 bytes) and **discarding** the new frames (16 bytes). It was
> invisible in telemetry — it looked just like a loose cable. We caught it with an **offline g++ harness** of the
> real encode→decode→interpret chain, migrated to the new format and covered it with tests. **Team rule**: on a
> failure, first grep the journal in case it has already happened to us."

**Q: Coordination between the 2 robots?**
> "Via **ESP-NOW** through the communication board (ESP32-C6). The partner shares its pose and whether it sees the
> ball, and that feeds into the WorldSnapshot. `[GAP: confirm the state of partner coordination on the bench]`."

### Development & Documentation (inspiration, tracking, testing)

**Q: How do you track progress?**
> "With three living indices: **CURRENT-STATE** (the robot's state on 1 page, mandatory reading), **SOURCES-OF-TRUTH**
> (one canonical doc per topic; whoever creates or supersedes a doc updates the table in the **same commit**) and a
> chronological **engineering journal**. Plus **self-contained packs** per subsystem for onboarding."

**Q: How do you test? (the star) 💡**
> "We separate the **decision logic into pure C++ modules** —no Arduino— and test them on the PC with g++. Today:
> **652 tests / 47 suites / 0 failures** (measured 2026-06-05 18:39 ART with `scripts/run-host-tests.sh`), which grew traceably
> (246 → 262 → 324 → 354 → 545 → 652). See the growth chart at `docs/competencia/assets/fig8_test_growth.png`. **Honest
> caveat:** the host runner compiles the **pure modules** (shared + down); the central/top tests use Arduino and
> compile **on-target**. It was born from a real problem: the **antivirus was blocking PlatformIO**, so we vendored
> the test framework and wrote a g++ runner that **dodges the antivirus and runs without internet**. That lets us
> verify embedded firmware **without having the board in hand**."

**Q: How do you make sure a change doesn't break what was working?**
> "Two things: (1) a **mandatory green gate** —all 652 tests pass (652 / 47 / 0)— before any merge; and (2) a
> **byte-identical fallback**: every new feature, if the data is unavailable, produces **exactly** the previous
> command. We verify it with a test that compares the output with and without the data. That way a feature 'sleeps'
> until the data flows and **never introduces a regression**."

**Q: Do you use version control? How do you work as a team?**
> "Git in a shared public repo on GitHub (**IITA**, https://github.com/IITA-Proyectos/open-soccer-robocup-team2026),
> **MIT, all open-source**. We develop on branches and always `git fetch` + merge before pushing. Every commit
> carries **human/AI attribution**. We audit the firmware with independent reviews (20 subsystems; critical findings
> go through a **second skeptical reviewer**)."

---

## §5 — Team protocol during the interview (what the judge observes, not what we say)

> This is what separates a 3 from a 5 in **Teamwork & Communication**. Rehearse it like choreography.

| Situation | What to do (for Excellent) | What NOT to do |
|---|---|---|
| Question about my teammate's area | "That was worked on by `[X]`, tell them" + the owner answers + **I add a data point** | Answering over the owner |
| I don't know the answer | "Honestly I'm only half-sure on that, we validate it on the bench; what I do know is..." + redirect to what I do own | Making things up / staying silent |
| My teammate gets stuck | Back them up: "and a data point that helps here is..." | Leaving them alone or correcting them harshly in public |
| The judge asks about something we haven't bench-validated | **Be honest**: "it's implemented and tested on host, field validation is pending" | Saying it works if we haven't tested it |
| There are other teams in the room | Greet them, listen to their answers, offer help if they ask | Ignoring them (the rubric rewards engagement with everyone) |

**Team closing line (memorize):** "Everything you saw is open-source and documented so another team can replicate it
— if you'd like, we'll share the repo with you." (Reinforces Documentation & Community and leaves a good impression.)

---

## §6 — Calibrated honesty (what to say if they ask about what's missing)

> Telling the truth **with an engineering framing** scores points; lying and getting caught loses them. Have these ready:

- **Vision not recalibrated for Incheon:** "The vision code is solid and tested; what's missing is **bench
  calibration** (LAB + homography) for the venue's lighting. We have the kit ready to recalibrate in <5 min." (It is
  our real blocker #1 — don't hide it.)
- **Tentative kinematics:** "The wheel angles and radius are flagged as **tentative** in the code because we hadn't
  measured the assembled robot; the module is pure and tested, we just need to load the real constants."
- **Only 1 healthy IMU:** "We run with **1 healthy BNO055 + 4 ToF**; the second IMU failed and the risk is
  documented. The pose still computes."
- **Heading that freezes:** "We found that the IMU and the ToF sensors compete on the I²C bus and the heading was
  freezing; we mitigated it by lowering the bus to 100 kHz and reading the IMU at 20 Hz. The proper fix (IMU on a
  separate bus) is noted."

---

## §7 — Preparation checklist (do this BEFORE traveling)

- [ ] Fill in `IITA Low Battery Messi` and confirm who the **competitors present** in Incheon are.
- [ ] Each competitor rehearses **their area** (§2) until they can answer fluently, with numbers.
- [ ] **Rehearse in English** §1 (Show & Tell) and the 💡 answers from §4, out loud.
- [ ] Rehearse the **timed Show & Tell** (90 s) 3 times with the robot in hand.
- [ ] Rehearse the **hand-off protocol** (§5) in a 5-question mock run.
- [ ] Laptop ready: repo cloned, `pio` working offline, `scripts/run-host-tests.sh` tested the day before, robots
      flashed with `_robot1`/`_robot2`.
- [ ] Keep the **"where the code lives" map** (§3.2) printed or on screen for the Teamwork-Task.
- [ ] Have the **power-cycle** internalized (§3.3) so you don't lose time if a sensor doesn't appear.

---

## Gaps (real data still missing — fill in before Incheon)

- `IITA Low Battery Messi` officially registered with RoboCup Junior for Incheon (confirmed 2026-06-05; "IITA - Open Soccer RoboCup Team 2026" is the internal repo descriptor).
- ✅ RESOLVED 2026-06-05: competitors **María Virginia Viollaz** (vision/strategy) and **Elías Cordero** (electro-mechanics), both 18. Also travelling: **Enzo Juárez Velázquez (coach)** and **Cecilia Budeguer (mentor)**; **Gustavo Viollaz (mentor)** does not travel.
- ✅ RESOLVED 2026-06-05: Salta, Argentina · champions of the 2025 Roboliga Argentina national final (UAI).
- **2026 chassis materials and dimensions** (height between floors/standoffs, printed parts, robot diameter and weight) — not documented; affects answers in the Mechanical category.
- **2026 chassis STL/CAD** so we can say "it's replicable" with confidence (the ones in the repo are from 2025 with the now-discarded dribbler/solenoid).
- **State of partner coordination (ESP-NOW) on the bench** — to confidently answer the "robot-to-robot" question.
- **Current test count** at travel time: verified **652 tests / 47 suites / 0 failures (2026-06-05 18:39 ART via `scripts/run-host-tests.sh`)**. Run the runner the day before and use the real figure of the day.
- **IITA legal name** (✅ resolved 2026-06-05): **IITA = Instituto de Innovación y Tecnología Aplicada** (Institute of Innovation and Applied Technology) / Fundación Innovar. Unified across all docs.
- **Data figures** (`docs/competencia/assets/fig8_test_growth.png`, `fig9_otos_error.png`): generate them with `gen_figuras.py` before traveling — the script exists but the PNGs are not yet generated in `assets/`.
- **English translation** of all interview material (a language requirement of the competition): this English version is `docs/competencia/en/ENTREVISTA-PREP.md`; it still needs to be **rehearsed out loud**.
