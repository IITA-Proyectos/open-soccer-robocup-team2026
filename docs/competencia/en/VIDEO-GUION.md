---
title: "Short Form Video TDP SCRIPT — RoboCupJunior Soccer Open 2026"
date: 2026-06-04
status: judged-draft
language: English narration + burned-in English subtitles (this is the script that gets recorded)
target-format: video < 3 minutes (Short Form Video TDP)
rubric: RoboCupJunior Soccer 2026 — Short Form Video TDP (1 pt, 0/1 scale; top level = Satisfactory = 1)
---

# HOW TO READ THIS SCRIPT (note for the team, NOT spoken in the video)

This file is the **shot-by-shot script** for the **Short Form Video TDP** (RCJ Soccer 2026 rubric: **1 point**, **0/1** scale; the ceiling is **Satisfactory = 1**). The rubric asks for ONE thing to earn the point: that the video **shows THE feature the team is MOST proud of, explained clearly enough that ANOTHER peer competitor understands it and learns from it**.

Each table row has 4 columns:
- **Time** — start mark of the shot (cumulative). The total lands **< 3:00**.
- **Narration (EN)** — the **EXACT text** that is spoken/recorded. Computed at **~145 words/min** (requested range 140–160).
- **On-screen image** — `[IMG: ...]` = what is seen (filmed or captured).
- **On-screen text/graphic** — lower-thirds, titles and numbers that appear as overlays (in English).

The **block titles map 1:1 to the rubric criterion** so the JUDGE can see at a glance why this video earns the point. Below the table: **shot list**, **production notes** and a **gap log** (missing real data).

---

# RUBRIC CRITERION → WHERE IT IS MET (map for the judge)

| Rubric criterion (Short Form Video TDP, 0/1) | Level targeted | Where it is met in this script |
|---|---|---|
| **"Easy to follow / a peer competitor understands it"** (Satisfactory = 1) | **Satisfactory (= maximum, 1 pt)** | Problem → solution → how-to-replicate structure (blocks 1–5); ~145 wpm pace; EN subtitles; jargon explained; on-screen demo of 44 suites passing green (624 tests / 44 suites / 0 failures, verified 2026-06-04 via scripts/run-host-tests.sh) |
| **"Shows THE feature the team is MOST proud of"** | Met | Block 1 names the feature within the first 12 s: **host-native testing of embedded firmware** |
| **"Another competitor learns from it"** (replicability — RCJ gold standard) | Met | Block 4 gives the **exact recipe** (pure modules + `g++` + vendored Unity) so any team can copy it |

---

# WHY THIS FEATURE (rationale for the choice — note for the team)

We chose **host-native testing of embedded firmware** as the "feature we're most proud of" among the project's 5 candidates. Reasons, in order of weight for THIS deliverable:

1. **It is the only one that can be demonstrated LITERALLY on screen in seconds.** A video can show **40 test suites passing green in a terminal** — the judge and any competitor *see* the proof, we don't just tell them about it. The other features (3-board fail-safe, ToF trilateration, an anticipating goalkeeper) are powerful but are currently **"host-verified only" and NOT field-validated** (vision not yet recalibrated = blocker #1; kinematics uncalibrated; pose never comes back `valid`); presenting them as finished would be dishonest and risky.
2. **It solves a pain that EVERY RCJ team has:** verifying embedded firmware without burning bench hours or depending on the physical board. A peer competitor walks away with something they **can apply to their own robot on Monday**. That is exactly what the criterion rewards ("learn from it").
3. **It has a real, fresh, verifiable number TODAY.** We ran the suite on **2026-06-04** and it returned **624 tests / 44 suites / 0 failures (verified 2026-06-04 via scripts/run-host-tests.sh)** in seconds, 100% offline. It is not a promise: it is reproducible with `bash scripts/run-host-tests.sh`.
4. **It has a concrete, relatable origin story:** the antivirus **Avast** was blocking PlatformIO's registry (`pio test` couldn't download Unity), so the team **routed around it** by vendoring Unity and compiling with `g++`. A real obstacle → a clever fix → now anyone can replicate it.
5. **It is the most "replicable"**, and replicability is RCJ's gold standard. The recipe fits in 20 seconds of screen time.

> The other 4 features have their place in the **TDP** and the **poster** (where they are described with their honest "host-tested vs bench-validated" status). For the **1-point video**, the winner is the one a peer understands and copies fastest.

---

# SHOT-BY-SHOT SCRIPT (total target: **< 3:00**)

> **Narration word count:** ~410 words → at 145 wpm ≈ **2 min 50 s** of narration + breathing tails → lands **under 3:00**. (If it's tight, cut the Block 3 narration marked as *optional*.)

## BLOCK 1 — Hook + name of the feature (criterion: "shows THE feature you're most proud of") · 0:00–0:18

| Time | Narration (EN — exact text) | On-screen image | On-screen text/graphic (EN) |
|---|---|---|---|
| 0:00 | "How do you check that your robot's firmware is right… without the board, without a battery, and without losing hours at the bench?" | [IMG: tight shot of the assembled robot sitting still on the field; then a quick cut to a laptop] | **Title:** "How we test robot firmware — WITHOUT the board" · [TEAM NAME] logo |
| 0:08 | "We are [TEAM NAME], from Salta, Argentina. This is the feature we're most proud of: **host-native testing of embedded firmware**." | [IMG: team / robot photo; transition to a clean terminal] | **Lower-third:** "[TEAM NAME] · Salta, Argentina · RoboCupJunior Soccer Open" |

## BLOCK 2 — The problem (criterion: "easy to follow" — the why first) · 0:18–0:48

| Time | Narration (EN — exact text) | On-screen image | On-screen text/graphic (EN) |
|---|---|---|---|
| 0:18 | "The problem: a soccer robot's decision logic —kinematics, PIDs, the state machine, the board-to-board protocol— is hard to test ON the board. Every change means compile, flash, and watch the robot. Slow and expensive." | [IMG: timelapse of flashing a Teensy and moving the robot by hand on the field; a clock ticking in a corner] | **Text:** "Edit → flash → watch the robot → repeat = SLOW" |
| 0:34 | "And on top of that we hit a real obstacle: our antivirus, Avast, was blocking PlatformIO's registry. The `pio test` command couldn't download the test framework. We were stuck." | [IMG: screenshot of the PlatformIO error / Avast blocking the download (recreate or use a real screenshot)] | **Text:** "Avast blocked PlatformIO's registry → `pio test` couldn't run" |

## BLOCK 3 — The idea + the live demo (criterion: "easy to follow" + the judge SEES the proof) · 0:48–1:48

| Time | Narration (EN — exact text) | On-screen image | On-screen text/graphic (EN) |
|---|---|---|---|
| 0:48 | "The idea is simple: we split the firmware in two. On one side, **PURE modules** in C++ —no Arduino, no `Serial`, no `Wire`— that hold ALL of the decision logic. On the other, a thin Arduino glue layer that only wires those modules to the pins." | [IMG: animated 2-layer diagram: "PURE C++ (logic)" on top, "Arduino glue (thin)" below, with an arrow; highlight `src/shared/`] | **Diagram:** "src/shared/ = PURE modules (no Arduino)  ·  glue = thin" |
| 1:04 | "Because those modules don't depend on hardware, we compile and run them on the PC with `g++`, using the Unity framework we keep stored inside the repo. No internet, no registry, no Avast in the way." | [IMG: capture of the `bash scripts/run-host-tests.sh` command starting to run] | **Text:** "Compile with `g++` + vendored Unity → runs on the laptop, 100% offline" |
| 1:18 | "And here's what we see when we run the suite, right now:" | [IMG: **REAL screencast** of the terminal printing line by line `PASS test_proto`, `PASS test_kinematics`, `PASS test_localization`… sped up] | **Text (appears at the end):** "live run · 2026-06-04" |
| 1:26 | "Forty test suites. Five hundred and forty-five cases. Zero failures. In seconds, without touching the robot." | [IMG: freeze on the final line `PASS=44  FAIL=0  SKIP=0  (tests run: 624)`; zoom and highlight] | **Big number:** "624 tests / 44 suites / 0 failures · in seconds"<br>[FIGURE: docs/competencia/assets/fig8_test_growth.png] |
| 1:36 | *(optional, cuttable if it runs past 3:00)* "And it's not magic all at once: the suite grew traceably session by session —180, 246, 324, 403… up to 545— every bug we found became a test that never slips by again." | [IMG: animated bar chart of the growth 180→246→324→403→545 — use `docs/competencia/assets/fig8_test_growth.png` (gen_figuras.py)] | **Graphic:** "Tests over time: 180 → 246 → 324 → 403 → 545 → 624" |

## BLOCK 4 — How YOU replicate it (criterion: "another competitor learns from it" — the heart of the point) · 1:48–2:30

| Time | Narration (EN — exact text) | On-screen image | On-screen text/graphic (EN) |
|---|---|---|---|
| 1:48 | "Best of all: you can copy it for your own robot. It's three steps." | [IMG: "How to replicate — 3 steps" card appearing] | **Title:** "Replicate it in 3 steps" |
| 1:53 | "One: pull the decision logic out of the Arduino code and put it into pure modules, in separate files. Pass time in as a parameter instead of calling `millis()`, so the tests are deterministic." | [IMG: code split: on the left a `loop()` with mixed-in logic (struck through), on the right a clean pure module] | **Step 1:** "Move decision logic into PURE modules · inject time, don't call millis()" |
| 2:06 | "Two: store the Unity framework inside your own repo, so it doesn't depend on the internet or on an antivirus." | [IMG: folder tree highlighting `lib/Unity/`] | **Step 2:** "Vendor Unity into `lib/Unity` (no network needed)" |
| 2:14 | "Three: compile each test with `g++`, linking your pure modules, and run the binary on the PC. We wrapped it in a one-line script: `run-host-tests.sh`." | [IMG: capture of the command, with `cd "software/teensy/Soccer 2026"` on top and then `g++ -std=gnu++17 -I src/shared -I lib/Unity/src lib/Unity/src/unity.c src/shared/*.cpp test/test_X/test_main.cpp -o ...`; below it `bash scripts/run-host-tests.sh`] | **Step 3:** `cd "software/teensy/Soccer 2026"` → `g++ ... -I src/shared -I lib/Unity/src ...` link pure modules + run on host · one script: `run-host-tests.sh` |
| 2:24 | "The Arduino glue stays thin and is verified just by compiling it. All the intelligence, tested before you even power the robot on." | [IMG: the 2-layer diagram from Block 3 reappearing, with a green check over "PURE C++"] | **Text:** "Logic verified BEFORE powering the robot" |

## BLOCK 5 — Close + where to learn it (criterion: replicability / open-source) · 2:30–2:55

| Time | Narration (EN — exact text) | On-screen image | On-screen text/graphic (EN) |
|---|---|---|---|
| 2:30 | "This let us iterate fast and fearlessly, even days before competing. All the code and the script are open, MIT-licensed, in our GitHub repo." | [IMG: GitHub screenshot of the repo showing `scripts/run-host-tests.sh` and the `test/` folder] | **Text:** "Open-source · MIT · github.com/IITA-Proyectos/open-soccer-robocup-team2026" |
| 2:42 | "If you're a RoboCupJunior competitor and you want to test your firmware without the board, copy the recipe. See you in Incheon. Thank you!" | [IMG: robot moving on the field (short clip); ends with a team card + handle] | **Final card:** "[TEAM NAME] · RoboCupJunior Soccer Open · Incheon 2026 · github.com/IITA-Proyectos/open-soccer-robocup-team2026" |

> **Video close: ≈ 2:55** — under the 3:00 limit with margin. If the narration stretches, cut the *optional* Block 3 narration (0:48 → back to ≈ 2:43).

---

# SHOT LIST (what to film / capture, in priority order)

### A. Screencasts (the most important — they are the on-screen proof)
1. **[CAPTURE — CRITICAL]** REAL screencast of `bash scripts/run-host-tests.sh` running from start to finish, with the final line `PASS=44  FAIL=0  SKIP=0  (tests run: 624)` clearly legible. *(Verified 2026-06-04: the suite returns exactly that, exit code 0.)* Record in a terminal with a large font and a high-contrast theme.
2. **[CAPTURE]** The command, preceded by `cd "software/teensy/Soccer 2026"`, then `g++ -std=gnu++17 -I src/shared -I lib/Unity/src lib/Unity/src/unity.c src/shared/*.cpp test/test_X/test_main.cpp -o ...` (from `scripts/run-host-tests.sh`: flags on line 43, compile on line 80) on screen, highlighting that the real includes are `-I src/shared -I lib/Unity/src` (NOT `-I src/down`, NOT `lib/Unity`) and that it links `src/shared/*.cpp`.
3. **[CAPTURE]** Recreate (or use a real screenshot of) the **Avast / PlatformIO registry error** blocking `pio test` (context: TASK-025). If the exact error can't be recreated, show the note in `docs/ESTADO-ACTUAL.md` that documents it.
4. **[CAPTURE]** Folder tree showing `lib/Unity/` and `test/test_*/` (there are **40+ test folders**).
5. **[CAPTURE]** GitHub screenshot of the repo (org `IITA-Proyectos`) with `scripts/run-host-tests.sh` and the `test/` folder open, and the `LICENSE` file (MIT) visible.
6. **[CAPTURE]** "Before/after" code split: a clean pure module (e.g. `src/shared/ball_predict.cpp` or `src/shared/kinematics.cpp`) next to a `loop()` with mixed-in logic (can be illustrative).

### B. Diagrams to produce (motion graphics)
1. **[DIAGRAM]** The **2 layers**: "PURE C++ modules (`src/shared/`, no Arduino)" on top ↔ "Arduino glue (thin)" below, with an arrow. Reusable in Blocks 3 and 4.
2. **[DIAGRAM]** **Test-growth bar chart**: 180 → 246 → 324 → 403 → 545 → 624 (dates: snapshots from `docs/ESTADO-ACTUAL.md`; 545 verified 2026-06-04). Pre-rendered figure available: `docs/competencia/assets/fig8_test_growth.png` (gen_figuras.py).
3. **[GRAPHIC]** "Replicate it in 3 steps" card with the 3 steps.

### C. Robot / team footage (B-roll)
1. **[PHOTO/CLIP]** Assembled robot on the field, tight view (hook 0:00 and close 2:42).
2. **[PHOTO/CLIP]** Timelapse of flashing the Teensy + moving the robot by hand (Block 2, illustrates "slow and expensive").
3. **[PHOTO]** Photo of the team or the competitors (Block 1, identification).
4. **[CLIP]** Robot moving on the field for the close.

---

# PRODUCTION NOTES

- **Duration:** target **< 3:00** (hard rubric limit). Count: ~410 words of narration at **~145 wpm** ≈ **2:50** + tails ≈ **2:55**. The *optional* Block 3 narration (growth chart) is the first cut if time has to be reclaimed.
- **Language / subtitles:** the video is **narrated AND subtitled in English** (the rubric/judges are international); this EN script is the one that gets recorded. Keep **burned-in English subtitles** for the whole video. All **on-screen overlay text is in English**.
- **Audio:** clear voice, no music covering the narration; soft, low background music. Narrate slowly in Block 4 (the 3 steps are what the competitor needs to retain).
- **Terminal legibility:** font ≥ 18 pt, high-contrast theme, zoom/highlight on the line `PASS=44 FAIL=0 SKIP=0 (tests run: 624)`. This is the key moment: the judge has to READ the number.
- **Technical honesty (don't oversell):** the video focuses on what is **verified and demonstrable** (the suite runs and passes today). Do not claim that features not bench-validated "work on the field." This protects Sportsmanship and credibility with the judge.
- **Real data already verified (use verbatim):**
  - **624 tests / 44 suites / 0 failures (verified 2026-06-04 via scripts/run-host-tests.sh)** (exit code 0). *(NOTE: the rubric and other deliverables cited a lower previous cap; the number rose to 624 tests / 44 suites — it is verified in this session and is the figure to publish.)*
  - Traceable growth: **180 → 246 → 262 → 324 → 354 → 403 → 545 → 624** (snapshots in `docs/ESTADO-ACTUAL.md`).
  - Exact command and recipe: `scripts/run-host-tests.sh` (lines 32–34).
  - Origin: Avast was blocking PlatformIO's registry → **TASK-025**; fix = Unity vendored in `lib/Unity` + `g++`.
  - License **MIT**, public repo: https://github.com/IITA-Proyectos/open-soccer-robocup-team2026 (org `IITA-Proyectos`).
  - **Organization: IITA** (use the acronym consistently). ⚠️ The legal expansion is a **real gap, do NOT resolve it blindly**: `LICENSE` says "Instituto de **Innovación** y Tecnología Aplicada"; `README`/`POSTER` say "Instituto de **Informática** y Tecnología Aplicada". Where it must be expanded, write: "IITA (Instituto de [Innovación/Informática] y Tecnología Aplicada — VERIFY legal name)".
- **Continuity with the other deliverables:** this video highlights the SAME feature #1 that the poster and the TDP mark as their process differentiator, so the judge sees a coherent message across video, poster and TDP.

---

# GAP LOG (missing real data — complete before recording)

| # | Gap | Type | Where it impacts the video |
|---|---|---|---|
| 1 | **[TEAM NAME]** officially registered for RoboCup Incheon 2026 (the repo uses "IITA — Open Soccer RoboCup Team 2026" as an internal descriptor) | Identification | Title 0:00, lower-third 0:08, final card 2:42 |
| 2 | **[REGION]** / name of the regional or super-regional they qualified through (Salta, Argentina confirmed; the formal regional is missing) | Identification | Lower-third 0:08 (currently says only "Salta, Argentina") |
| 3 | **Repo URL RESOLVED** → https://github.com/IITA-Proyectos/open-soccer-robocup-team2026 (already overlaid at 2:30 and 2:42; no pending gap) | Open-source | Text 2:30 and final card 2:42 |
| 3b | **IITA legal name UNRESOLVED:** `LICENSE` says "Instituto de **Innovación** y Tecnología Aplicada"; `README`/`POSTER` say "Instituto de **Informática**…". Verify the real legal name before expanding the acronym on screen / in narration | Identification | Lower-third 0:08, notes |
| 4 | **[PHOTO/CLIP: assembled 2026 robot moving on the field]** — real clip for the hook (0:00) and close (2:42) | Footage | Blocks 1 and 5 |
| 5 | **[PHOTO: team / competitors]** for identification | Footage | Block 1 (0:08) |
| 6 | **[CLIP: timelapse of flashing the Teensy + moving the robot by hand]** to illustrate "slow and expensive" | Footage | Block 2 (0:18) |
| 7 | **[CAPTURE: real Avast/PlatformIO error blocking `pio test`]** — recreate or screenshot; if not, show the TASK-025 note in `ESTADO-ACTUAL.md` | Capture | Block 2 (0:34) |
| 8 | **[DECISION]** confirm English voice-over is recorded (vs. any fallback); lock before recording the narration | Production | Whole video |
| 9 | **[VERIFY at the end]** re-run `scripts/run-host-tests.sh` on the recording day to confirm the figure is still **624/44/0** (or update the overlaid number if it changed because of new tests) | Data | Block 3 (1:26, 1:36) |
| 10 | **[OPTIONAL]** confirm the exact dates of each growth-chart snapshot (180→…→624) if the bars are to be dated | Data | Block 3 (1:36, optional narration) |
