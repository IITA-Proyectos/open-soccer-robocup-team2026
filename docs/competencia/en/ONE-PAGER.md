---
title: "One-pager — pit hand-out, RoboCupJunior Soccer Open 2026 (Incheon)"
date: 2026-06-05
status: print-ready (1 side)
language: English (primary — this is the copy given to international teams)
purpose: Sportsmanship / Community give-away — robot summary + open-source invitation
print: 1 side, A4 or US Letter, portrait
---

# IITA Low Battery Messi
### RoboCupJunior Soccer Open 2026 · IITA — Instituto de Innovación y Tecnología Aplicada, Salta, Argentina

**An omnidirectional 3-board soccer robot (TOP / CENTRAL / DOWN) with OpenMV vision and no kicker — it scores by pushing the ball with inertia.**

> Come say hi at our pit. Everything below is **open-source (MIT)** — please copy it, fork it, and build a better robot than ours.

---

## What makes it different — 3 ideas you can steal

- **Layered fail-safe with a direct DOWN→CENTRAL emergency bus.** At 1 m/s the robot travels 1 mm/ms; routing the out-of-bounds alarm through two UARTs in series adds ~25 mm of overshoot. A direct one-hop wire brakes the robot at the field edge in **< 15 ms**.
- **Byte-identical fallback.** Every new feature (anticipating goalkeeper, OTOS drive-straight, cross-track strafe) outputs the **exact same command** as the previous behavior when its data is N/A — so each feature **"sleeps" until its data flows in**, with zero regression. Verified by a test that compares the output with and without the new data.
- **A goalkeeper that anticipates by ball velocity.** It aims at the **predicted** ball X = `pos + clamp(v · lookahead)`, not the current X. When ball velocity is 0 / unavailable, `lead = 0` → it falls back byte-identically to the simple keeper.

---

## Verified without the board

- **624 tests / 44 suites / 0 failures**, host-native (`g++`), **100% offline.**
- Decision logic lives in **pure C++ modules** (no Arduino / Wire / Serial); they compile and run on a laptop with no robot attached. Verification cycle: seconds.
- Run it yourself: `scripts/run-host-tests.sh`.

---

## What you can copy (MIT)

- **Byte-by-byte data contracts** — every message documented (type / size / pin / frequency / who fills it / who consumes it), e.g. the 31-byte `WorldSnapshot`.
- **The host test harness** — `run-host-tests.sh` and the pattern that lets you test embedded firmware on a PC, no hardware needed.
- **The PCBs** — full **EasyEDA** projects for the TOP and DOWN boards (schematic + PCB + Gerbers + BOM + Pick&Place), refabricable as-is.
- **The engineering journal** — every bench iteration: measured → evaluated → one-point fix → re-verified.

---

## Open-source

- **License: MIT** · © 2026 IITA / Fundación Innovar
- **Public repo:** https://github.com/IITA-Proyectos/open-soccer-robocup-team2026

**[QR to repo — generate]**
Repo URL: `https://github.com/IITA-Proyectos/open-soccer-robocup-team2026`

---

*IITA — Salta, Argentina · National RoboCupJunior Soccer champions, Argentina (Dec 2025) → RoboCup 2026, Incheon. Investing in learning. Built to be copied.*
