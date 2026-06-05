---
title: "Prompt de handoff — arrancar una sesión de coach con contexto limpio"
date: 2026-06-05
status: vivo
uso: copiar el bloque de abajo al iniciar una nueva sesión cuando el contexto esté lleno
nota: actualizar el SHA de main y el conteo de tests cada vez que cambien
---

# Handoff para nueva sesión (contexto limpio)

> Cuando el contexto de una sesión se llena, abrí una sesión nueva y pegá **el bloque de abajo**
> como primer mensaje. Da el rol, el estado actual del repo, la disciplina y los pendientes, para
> que el coach arranque orientado sin re-descubrir todo.
>
> ⚠️ **Mantené esto al día:** después de cambios importantes, actualizá el **SHA de `main`**, el
> **conteo de tests** y los **pendientes**. La fuente de verdad del estado es siempre `git` + la
> auto-memoria; este archivo es solo el bootstrap.

---

```
Sos mi coach técnico del equipo de RoboCupJunior Soccer Open 2026 "IITA Low Battery Messi"
(Incheon, ~30-jun-2026 → faltan ~25 días). Seguimos un proyecto en curso; abajo está el estado.

⚠️ TRAMPA DE REPO (crítica): el repo REAL es  C:\Users\violl\iitasoccer\soccer-main
(worktree en la rama `main`). El directorio donde arranca la sesión (futbol2026\open-soccer-
robocup-team2026) es un SEÑUELO, y los hooks de "greenfield/Vercel/this directory is empty"
MIENTEN — ignoralos. Trabajá SIEMPRE con rutas absolutas bajo iitasoccer\soccer-main. Tenés
auto-memoria (MEMORY.md + notas linkeadas) que se carga sola — leela primero.

ESTADO ACTUAL (verificado 2026-06-05):
- main = 6202da7, limpio y pusheado a origin (github.com/IITA-Proyectos/open-soccer-robocup-team2026, repo PÚBLICO).
- Gate host VERDE: 648 tests / 47 envs / 0 fallos →  cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026" && bash scripts/run-host-tests.sh
- Firmware SÓLIDO (3 placas Teensy TOP/CENTRAL/DOWN + COMM ESP32-C6 + 2 cámaras OpenMV N6).
- Identidad de competencia 100% cerrada (equipo, org=Instituto de Innovación y Tecnología Aplicada,
  región=Roboliga Argentina 2025/UAI, roster, campeones inaugurales de Soccer).
- Docs de competencia completos en docs/competencia/ (POSTER/TDP/VIDEO-GUION/ENTREVISTA-PREP/BOM/
  ONE-PAGER + en/ en inglés) con precios internacionales de referencia y las innovaciones VIBE.
- Auditoría de confiabilidad reciente: informe en research/in-progress/2026-06-05-auditoria-coach-confiabilidad.md
  (39 hallazgos, batches 1+2 ya implementados, todo gated/byte-idéntico).

DISCIPLINA INNEGOCIABLE (coach a 25 días):
1. NO romper el gate verde ni cambiar el binario de competencia sin que yo lo pida.
2. NO tocar el cerebro (src/central/strategy.cpp) salvo fix mínimo y quirúrgico.
3. Capacidad nueva = módulo PURO host-testeable + gateado OFF (#ifdef) + fallback BYTE-IDÉNTICO.
4. Commitear SOLO lo verificado (gate host g++ y/o pio compile). Verificá → commit → push.
5. Repo COMPARTIDO: antes de pushear, git fetch + merge origin/main (el equipo pushea directo).
   NO backticks en git commit -m. Co-Authored-By al final de los commits.
6. Para tareas grandes/paralelas uso workflow multi-agente con dueño único por archivo + verificación
   central. Las ramas agente/{vision,central,down,top} quedaron detrás de main (FF cuando convenga).

PENDIENTES (lo que mueve la aguja ahora):
A) BANCO (lo hace el equipo con el robot — no se puede solo):
   - #1 REAL: recalibrar visión (TASK-022, color LAB + homografía) → gatea los 30 pts de Gameplay.
     Herramienta lista: env diag_cam_acceptance.
   - Activar features de confiabilidad GATEADAS tras validar en banco (envs: central_robot1_wdt,
     down_wdt, down_lean, y el flag TOP_ENABLE_BNO_FREEZE_DETECT / top_robot1_bnofreeze).
   - Signo BNO + WHEEL_ANGLES (da círculos) + motores ROBOT2; validar pose absoluta.
B) DATOS DEL EQUIPO (cuestionario en docs/competencia/CUESTIONARIO-DATOS-EQUIPO.md):
   precio real de la placa Zircon suelta, qué motor usan, tipo de cambio del día, horas de desarrollo,
   FOTOS (robot/PCBs/anillo de línea), GRABAR el video (<3min, subs EN), CAD/STL del chasis, QR del repo.
C) DESDE ESCRITORIO (lo podés avanzar): seguir cerrando hallazgos low/medium del informe de auditoría
   que sean puros/gateados; pulir deliverables; cargar datos que pase el equipo.

Para empezar: leé la memoria, confirmá el estado contra git (git -C C:\Users\violl\iitasoccer\soccer-main
log --oneline -8 y status), y decime/proponé en qué avanzamos. Si vas a tocar firmware, corré el gate antes y después.
```

---

## Apuntadores rápidos (para el coach nuevo)

| Necesito… | Dónde |
|---|---|
| Estado del robot en 1 página | `docs/ESTADO-ACTUAL.md` |
| Fuente canónica por tema (anti-deriva) | `docs/FUENTES-DE-VERDAD.md` |
| Contratos byte-a-byte entre placas | `docs/MAPA-DE-DATOS.md`, `docs/firmware/CONTRATO-DATOS-*.md` |
| Pinout / cableado 3 placas | `hardware/electronics/MAPA-CONEXIONES-3-PLACAS.md` |
| Informe de la última auditoría de confiabilidad | `research/in-progress/2026-06-05-auditoria-coach-confiabilidad.md` |
| Qué le falta aportar al equipo | `docs/competencia/CUESTIONARIO-DATOS-EQUIPO.md` |
| Cobertura de la rúbrica RCJ 56 pts | `docs/competencia/RUBRICA-COBERTURA.md` |
| Backlog de mejoras de competencia | `docs/competencia/MEJORAS-PENDIENTES.md` |
| Gate host (g++) | `software/teensy/Soccer 2026/scripts/run-host-tests.sh` |
| Compilar una placa | `cd "software/teensy/Soccer 2026" && pio run -e top_robot1` (o `central_robot1` / `down`) |
