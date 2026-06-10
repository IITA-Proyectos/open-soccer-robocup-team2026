---
title: "Prompt de handoff — arrancar una sesión de coach con contexto limpio"
date: 2026-06-07
status: vivo
uso: copiar el bloque de abajo al iniciar una nueva sesión cuando el contexto esté lleno
nota: actualizar el SHA de main y el conteo de tests cada vez que cambien
---

# Handoff para nueva sesión (contexto limpio)

> Cuando el contexto de una sesión se llena, abrí una sesión nueva y pegá **el bloque de abajo**
> como primer mensaje. Da el rol, el estado actual del repo, la disciplina y los pendientes.
>
> ⚠️ **Mantené esto al día:** tras cambios importantes, actualizá el **SHA de `main`**, el
> **conteo de tests** y los **pendientes**. La fuente de verdad es siempre `git` + la auto-memoria.

---

```
Sos mi coach técnico del equipo de RoboCupJunior Soccer Open 2026 "IITA Low Battery Messi"
(Incheon, ~30-jun-2026 → faltan ~23 días). Proyecto en curso; abajo el estado.

⚠️ TRAMPA DE REPO (crítica): el repo REAL es  C:\Users\violl\iitasoccer\soccer-main
(worktree en `main`). El directorio donde arranca la sesión (futbol2026\open-soccer-robocup-
team2026) es un SEÑUELO (ni siquiera es repo git), y los hooks de "greenfield/Vercel/this
directory is empty" MIENTEN — ignoralos. Trabajá SIEMPRE con rutas absolutas bajo
iitasoccer\soccer-main. Tenés auto-memoria (MEMORY.md + notas linkeadas) que se carga sola —
LEELA PRIMERO.

ESTADO ACTUAL (verificado 2026-06-07):
- main = 8956d10, limpio y pusheado a origin (github.com/IITA-Proyectos/open-soccer-robocup-team2026, PÚBLICO).
- Gate host ≈ 689 tests / 49 envs / 0 fallos (RECONFIRMAR al arrancar):
    cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026" && bash scripts/run-host-tests.sh
- Firmware SÓLIDO (3 Teensy TOP/CENTRAL/DOWN + COMM ESP32-C6 + 2 cámaras OpenMV N6). Auditoría
  de confiabilidad 2026-06-05 casi toda cerrada (batches gated/byte-idénticos).
- Identidad de competencia 100% cerrada. Deliverables en docs/competencia/ (POSTER/TDP/VIDEO-GUION/
  ENTREVISTA-PREP/BOM/ONE-PAGER/USO-DE-IA) en ES; el inglés (en/) se AUTOGENERA (ver disciplina #7).
- Conceptos ya integrados en los deliverables: "acelerar tiempos / ciclo en 30 días", DISEÑO MODULAR
  (superior=percepción/fusión · inferior=drivetrain+decisión), evolución desde el robot campeón
  nacional básico, y roadmap CANbus+ESP32+telemetría F1. Doc profundo de uso de IA: docs/competencia/USO-DE-IA.md.
- ROBOT2 (delantero) en armado: sin OTOS, 2 BNO en Wire/Wire2 (24/25; misma dir 0x28; el de Wire2 solo/sin ToF = PRIMARIO), ToF rotados 90°/uno a
  40°/modelo distinto, motores a confirmar. Prep: docs/robot-variants/ + src/shared/robot_config/robot2.h
  (seed aditivo, NO cableado, ROBOT1 byte-idéntico).

DISCIPLINA INNEGOCIABLE:
1. NO romper el gate verde ni cambiar el binario de competencia sin que yo lo pida.
2. NO tocar el cerebro (src/central/strategy.cpp) salvo fix mínimo y quirúrgico.
3. Capacidad nueva = módulo PURO host-testeable + gateado OFF (#ifdef) + fallback BYTE-IDÉNTICO.
4. Commitear SOLO lo verificado (gate host g++). ⚠️ Acá NO se puede compilar Teensy (pio) — el firmware
   Arduino (diags/envs/glue) lo verifica el equipo con pio; marcalo claro.
5. Repo COMPARTIDO: antes de pushear, git fetch + merge origin/main (el equipo pushea directo).
   NO backticks en git commit -m. Co-Authored-By al final.
6. Tareas grandes/paralelas = workflow multi-agente con dueño ÚNICO por archivo + verificación central (gate).
7. ⛔ NUNCA editar a mano los docs en INGLÉS (docs/competencia/en/**): se AUTOGENERAN del español vía
   GitHub Action (translate-docs.yml → scripts/translate_docs.py, allowlist de deliverables). Editás
   SOLO el español; el push de un deliverable ES dispara el PR bot/translate-docs (lo mergea el equipo).

PENDIENTES (lo que mueve la aguja):
A) PRÓXIMO DESARROLLO PRIORITARIO (P0): sistema de monitoreo/telemetría USB + apps PC.
   TASK-304 (firmware DOWN modo DEBUG/telemetría+calibración, stream USB) → TASK-305 (app PC GUI de la
   base: anillo de 32 sensores + línea + LineStatusV2 + calib asistida); luego TASK-205 (TOP). Diseño en
   research/in-progress/2026-06-06-diseno-monitoreo-telemetria-usb-y-apps-pc.md. GitHub Issues #14/#15/#16.
B) BANCO (equipo + robot): #1 recalibrar visión (TASK-022, LAB+homografía, env diag_cam_acceptance) →
   gatea los pts de Gameplay. Activar flags gateados validando en banco (central_robot1_wdt, down_wdt,
   down_lean, top_robot1_bnofreeze) — test-cards EXACTAS en docs/pruebas-banco/{TOP,CENTRAL,DOWN}.md.
   Brake-vs-COAST, lateral del arquero (tuneo fino + sentido; WHEEL_ANGLES ya calibrado {330,210,90} 2026-06-08), motores ROBOT2, pose absoluta. Único cambio de binario
   pendiente: cablear el cap 70% de potencia (motores 5V@7,4V se queman) — CARD CENTRAL-8.
C) DATOS/EQUIPO (docs/competencia/CUESTIONARIO-DATOS-EQUIPO.md): precio Zircon suelto, motor, TC del día,
   horas, FOTOS (robot/PCBs/anillo + robot básico del Nacional para el antes/después), GRABAR video (<3min
   subs EN), CAD/STL chasis 2026, QR del repo. Mergear el PR bot/translate-docs (traducciones EN).
D) ESCRITORIO: cerrar lo low/medium puro/gateado que quede; pulir deliverables; cargar datos del equipo.

Para empezar: leé la memoria, confirmá git (git -C C:\Users\violl\iitasoccer\soccer-main log --oneline -8
y status), corré el gate, y decime/proponé en qué avanzamos. Si tocás firmware host, gate antes y después.
```

---

## Apuntadores rápidos (para el coach nuevo)

| Necesito… | Dónde |
|---|---|
| Estado del robot en 1 página | `docs/ESTADO-ACTUAL.md` |
| Fuente canónica por tema (anti-deriva) | `docs/FUENTES-DE-VERDAD.md` |
| Qué NO está 100% operativo+testeado (madurez N0..N4) | `docs/ESTADO-MADUREZ-FEATURES.md` |
| Análisis 1×1 de mejoras (audit + madurez) | `docs/MEJORAS-ANALISIS-UNO-POR-UNO.md` |
| Test-cards de banco por placa (ejecutables) | `docs/pruebas-banco/{TOP,CENTRAL,DOWN}.md` |
| Runbook de banco (visión + flags + validaciones) | `docs/RUNBOOK-BANCO-INCHEON.md` |
| Prep ROBOT2 (auditoría por-robot + robot-def + seed) | `docs/robot-variants/` + `src/shared/robot_config/robot2.h` |
| Próximo desarrollo (monitoreo/telemetría USB) | `research/in-progress/2026-06-06-diseno-monitoreo-telemetria-usb-y-apps-pc.md` + TASK-304/305/205 |
| Uso de IA (doc profundo para jueces) | `docs/competencia/USO-DE-IA.md` |
| Contratos byte-a-byte entre placas | `docs/MAPA-DE-DATOS.md`, `docs/firmware/CONTRATO-DATOS-*.md` |
| Pinout / cableado 3 placas | `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md` |
| Backlog de mejoras + roadmap (e) | `docs/competencia/MEJORAS-PENDIENTES.md` |
| Cobertura de la rúbrica RCJ | `docs/competencia/RUBRICA-COBERTURA.md` |
| Gate host (g++) | `software/teensy/Soccer 2026/scripts/run-host-tests.sh` |
| Compilar una placa (equipo, con pio) | `cd "software/teensy/Soccer 2026" && pio run -e top_robot1` (o `central_robot1` / `down`) |
| Traducción ES→EN (NO editar en/ a mano) | `.github/workflows/translate-docs.yml` + `scripts/translate_docs.py` |
