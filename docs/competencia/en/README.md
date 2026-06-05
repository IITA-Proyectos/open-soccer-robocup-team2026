---
title: "Index — English judging deliverables, RoboCupJunior Soccer Open 2026"
date: 2026-06-04
status: index
language: English (these files ARE the judges' deliverables)
event: RoboCup 2026 — Incheon, South Korea (30 Jun – 6 Jul 2026)
rubric: RoboCupJunior Soccer 2026 (documentation components — 26 pts + 2 bonus)
---

# English judging deliverables — RoboCupJunior Soccer Open 2026

This folder holds the **final English versions delivered to the judges**. These are the files that get printed, uploaded, recorded, and presented in Incheon.

> ⚠️ **Source vs. deliverable.** The Spanish files in [`../`](..) are the team's **working drafts** (internal). The files **in this folder (`en/`)** are the **judging deliverables**. RoboCupJunior requires English for the Poster, the TDP, the video, and the interview material — so when ES and EN disagree, **the EN file in this folder wins**. Do not ship the ES drafts.

---

## Files in this folder

| File | What it is | Rubric it feeds |
|---|---|---|
| [`POSTER.md`](POSTER.md) | Working mock-up of the A1 landscape poster (12-column grid, 15 zones mapped 1:1 to the rubric criteria, mandatory elements, Fig. 1–11). | Poster Design & Presentation (5 pts) |
| [`TDP.md`](TDP.md) | Technical Documentation Paper — 6 sections (Electrical / Mechanical / Software / Presentation), Decision→Why→Data format, iterations A–E, §5 open-source bonus, §6 gaps. | TDP (7 pts + 2 bonus) · Documentation & Community (5 pts) |
| [`VIDEO-GUION.md`](VIDEO-GUION.md) | Shot-by-shot script (<3 min) for the Short Form TDP video. Feature: host-native testing of embedded firmware. | Short Form Video TDP (1 pt) |
| [`ENTREVISTA-PREP.md`](ENTREVISTA-PREP.md) | Group Team Interview cheat-sheet: Show & Tell + Teamwork Task + question bank by category + hand-off protocol. | Group Team Interview (5 pts) |
| [`BOM.md`](BOM.md) | Bill of Materials for the major components (part number, supplier, new/reused, kit/custom, cost). Input for the Poster and the TDP. | Poster Method/Design · TDP Electrical · Documentation |

Shared assets (figures referenced by the files above) live in [`../assets/`](../assets). Today: `fig8_test_growth.png`, `fig9_otos_error.png`, and the generator `gen_figuras.py`.

---

## Verification status per file

All five files are complete 1:1 translations of their Spanish sources, with the agreed corrections applied (test counts reconciled to **658 tests / 47 suites / 0 failures**, growth chain ending at 658, real script paths, omega clamp 360→327). Remaining notes are **non-blocking** unless flagged otherwise — verify each before final submission.

### `POSTER.md` — complete
- No blocking issues. All 15 zones, every table (rubric/board/sensors/4 roles + 12-row BOM + 8-row iteration) and all figures/captions translated 1:1 with nothing dropped.
- Corrections verified consistent: "658 tests / 47 suites / 0 failures" in all 7 occurrences; growth series 180→246→262→324→354→545→658; `scripts/run-host-tests.sh` + the g++ command line; asset paths `docs/competencia/assets/fig8_test_growth.png` and `fig9_otos_error.png`; BOM §3/§6 cross-refs; omega clamp 360→327 fix in iteration row 7. TDP video appears only as the intended `[QR to TDP video <3 min]` placeholder.
- Placeholders correctly retained and visible: `[TEAM NAME]`, `Roboliga Argentina 2025 (national final, UAI)`, all `[COST - pending]` / `[TOTAL COST - pending]`, `[New/Reused? - pending]`, the `[PHOTO:]/[DIAGRAM:]/[CHART:]/[QR …]` markers, and the three "Gap note" blocks.
- Intentional omissions (NOT losses): the ES-only "WORKING VERSION IN SPANISH / translate before printing" banner and the "suggested English: …" parenthetical on the title were dropped as working-draft scaffolding.
- Cosmetic only (mirrors ES source): "engineering journal" is italicized inconsistently across zones B3/abstract — harmonize before print if desired.

### `TDP.md` — complete
- No blocking issues. Complete 1:1 translation: all 6 sections, all subsections (1.1–1.8, 2.1–2.5, 3.1–3.9, 4.1–4.4, 5.1–5.2), iterations A–E, both reference electrical iterations, all 98 table rows, and all code/flowchart/pseudocode blocks present.
- Corrections verified: "658 tests / 47 suites / 0 failures (measured 2026-06-05 19:50 ART via scripts/run-host-tests.sh)" in all 8 expected places; growth chain ends at 545; the script and both asset figures (`fig8_test_growth.png`, `fig9_otos_error.png`, `gen_figuras.py`) exist on disk.
- The "video command" correction item is N/A here (it belongs to `VIDEO-GUION.md`); the TDP correctly contains no video command.
- Placeholders clearly marked: `[GAP]`×14, `[BOM costs - pending]`×9, `[TEAM NAME]`×2, `[REGION]`, `[RESOLVED]`, plus PHOTO/DIAGRAM/FLOWCHART/PSEUDOCODE markers and GAP sub-blocks.
- By design: the EN correctly omits the Spanish-only working-draft banner. Closing line "IITA — Salta, Argentina" is already correct; no change needed.

### `VIDEO-GUION.md` — complete (2 source-inherited reconciliations before recording)
- NON-BLOCKING (inherited from ES, not a translation error): the overlay/narration cite a terminal final line `Tests: 658 | Failures: 0  (Envs: 47 | OK: 47)`, but the real script `software/teensy/Soccer 2026/scripts/run-host-tests.sh` prints `Envs: 47 | OK: 47 | FAIL: 0` and `Tests: 658 | Failures: 0` (lines 117–120). Reconcile the on-screen text with actual output before recording the screencast.
- NON-BLOCKING (inherited from ES): the replicate-recipe command shows `g++ … -I src/shared -I src/down -I lib/Unity/src …` citing "lines 32-34", but the actual script has no `-I src/down` (flags on line 43: `-I src/shared -I lib/Unity/src`; compile on line 80). Consider trimming `-I src/down` and fixing the line reference.
- MINOR: Block 1 title overlay (0:00) reads "[TEAM NAME] logo" while the same block lower-third (0:08) and final card use "[TEAM NAME]"; confirm the logo-vs-name distinction is intended.

### `ENTREVISTA-PREP.md` — complete
- MINOR / inherited from source (not a translation defect): the last Gaps bullet (line 305) and the ES source claim "the PNGs are not yet generated in assets/", but `../assets/` now actually contains `fig8_test_growth.png` and `fig9_otos_error.png`. Optional: update to "PNGs generated; regenerate with `gen_figuras.py` if the count changes before traveling" to avoid a stale gap on a delivered doc.

### `BOM.md` — complete
- MINOR (optional): Battery connector is "Deans-T-F" in the EN BOM (line 77) but "Dean-T-F" in the ES source (line 79) and both TDP files (`en/TDP.md` L60, `TDP.md` L64). "Deans" is the correct English trade name (likely an intentional fix), but it diverges from the other deliverables — align spelling across files for consistency.

---

## Before you submit — must-close checklist

1. **Complete identity & costs.** ✅ `[TEAM NAME]` resolved 2026-06-05 = **IITA Low Battery Messi**; still fill every `Roboliga Argentina 2025 (national final, UAI)` placeholder, and resolve all `[COST - pending]` / `[TOTAL COST - pending]` / `[BOM costs - pending]` and `[New/Reused? - pending]` cells in `BOM.md`, `POSTER.md`, and `TDP.md`.
2. ✅ **Organization legal name resolved 2026-06-05:** IITA = Instituto de Innovación y Tecnología Aplicada (Fundación Innovar), unified across all files.
3. **Generate the images in `../assets/`.** `fig8_test_growth.png` and `fig9_otos_error.png` exist; produce/refresh every other `[PHOTO:]` / `[DIAGRAM:]` / `[CHART:]` referenced by the Poster and TDP (run `gen_figuras.py` to regenerate the charts if the test count changes before traveling).
4. **Record the video.** Shoot the `<3 min` screencast per `VIDEO-GUION.md`, first reconciling the on-screen test-output text and the g++ replicate command with the real `run-host-tests.sh` output.
5. **Re-run the tests the day before and lock one number.** `scripts/run-host-tests.sh` is the single source for the test count (today 658/47/0); make sure every deliverable shows the same figure, team name, and organization name.
