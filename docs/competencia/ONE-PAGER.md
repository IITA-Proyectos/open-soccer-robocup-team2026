---
title: "One-pager — material de pit, RoboCupJunior Soccer Open 2026 (Incheon)"
date: 2026-06-05
status: listo-para-imprimir (1 carilla)
idioma: español (versión para el equipo; el PRIMARIO en inglés está en en/ONE-PAGER.md)
proposito: regalo de Sportsmanship / Community — resumen del robot + invitación al open-source
impresion: 1 carilla, A4 o carta, vertical
---

# IITA Low Battery Messi
### RoboCupJunior Soccer Open 2026 · IITA — Instituto de Innovación y Tecnología Aplicada, Salta, Argentina

**Robot de fútbol omnidireccional de 3 placas (TOP / CENTRAL / DOWN) + visión OpenMV, sin pateador — convierte empujando la pelota por inercia.**

> Vení a saludarnos al pit. Todo lo de abajo es **open-source (MIT)**: copialo, forkealo y hacé un robot mejor que el nuestro.

---

## Qué lo hace distinto — 3 ideas para copiar

- **Fail-safe en capas con bus de emergencia directo DOWN→CENTRAL.** A 1 m/s el robot recorre 1 mm/ms; pasar la alarma de borde por dos UART en serie agrega ~25 mm de *overshoot*. Un cable directo de un solo salto frena al robot en el borde de la cancha en **< 15 ms**.
- **Fallback byte-idéntico.** Cada feature nueva (arquero que anticipa, drive-straight con OTOS, strafe por cross_track) produce **exactamente el mismo comando** que la conducta previa cuando su dato es N/A — así cada feature **"duerme" hasta que su dato fluye**, sin regresión. Verificado con un test que compara la salida con y sin el dato nuevo.
- **Arquero que anticipa por velocidad de pelota.** Apunta a la X **predicha** = `pos + clamp(v · lookahead)`, no a la X actual. Con velocidad de pelota 0 / no disponible, `lead = 0` → vuelve byte-idéntico al arquero simple.

---

## Verificación sin placa

- **624 tests / 44 suites / 0 fallos**, host-native (`g++`), **100% offline.**
- La lógica de decisión vive en **módulos C++ puros** (sin Arduino / Wire / Serial); compilan y corren en una notebook sin el robot. Ciclo de verificación: segundos.
- Corrélo vos: `scripts/run-host-tests.sh`.

---

## Qué podés copiar (MIT)

- **Contratos de datos byte-a-byte** — cada mensaje documentado (tipo / tamaño / pin / frecuencia / quién lo llena / quién lo consume), p. ej. el `WorldSnapshot` de 31 bytes.
- **El harness de tests host** — `run-host-tests.sh` y el patrón que permite testear firmware embebido en la PC, sin hardware.
- **Las PCBs** — proyectos **EasyEDA** completos de las placas TOP y DOWN (esquemático + PCB + Gerbers + BOM + Pick&Place), refabricables tal cual.
- **El diario de ingeniería** — cada iteración de banco: medimos → evaluamos → fix en un solo punto → re-verificamos.

---

## Open-source

- **Licencia: MIT** · © 2026 IITA / Fundación Innovar
- **Repo PÚBLICO:** https://github.com/IITA-Proyectos/open-soccer-robocup-team2026

**[QR al repo — generar]**
URL del repo: `https://github.com/IITA-Proyectos/open-soccer-robocup-team2026`

---

*IITA — Salta, Argentina · Campeones nacionales RoboCupJunior Soccer Argentina (dic 2025) → RoboCup 2026, Incheon. Invertimos en aprendizaje. Hecho para ser copiado.*
