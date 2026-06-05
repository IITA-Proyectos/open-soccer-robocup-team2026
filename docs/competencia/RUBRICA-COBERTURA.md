---
title: "Matriz de cobertura de rúbrica — RoboCupJunior Soccer Open 2026"
date: 2026-06-04
status: borrador-juzgado
idioma: español-rioplatense (los DELIVERABLES finales — Poster + TDP — van en INGLÉS; este doc interno queda en ES)
rubrica: RoboCupJunior Soccer 2026 (total 56 pts) — fuente robocup-junior.github.io/soccer-rules/master/scoring.html
escala: Developing=0 · Satisfactory=1 · Proficient=3 · Excellent=5 (Sportsmanship 0-3 · Video 0-1)
---

# Matriz de cobertura de la rúbrica — RoboCupJunior Soccer Open 2026

> **Objetivo de este documento:** que un juez (o el propio equipo) encuentre **cada punto de la rúbrica en 5 segundos** — qué archivo + sección lo cubre, a qué nivel apuntamos, cuánto estimamos hoy y qué falta para el máximo.
>
> **Cómo leer el estado:** el puntaje estimado refleja **lo que un juez ve HOY** (versiones de trabajo en español, sin imágenes, con placeholders). El "Nivel apuntado" es el techo realista una vez ejecutada la checklist de [MEJORAS-PENDIENTES.md](MEJORAS-PENDIENTES.md).
>
> ⚠️ **Bloqueante transversal:** Poster y TDP deben entregarse en **INGLÉS** (requisito duro). Hoy todo está en español de trabajo. Sin traducir, varios criterios no pueden puntuar Excellent.

---

## Resumen del reparto de puntos (56 pts)

| Bloque | Componente | Pts | ¿Bajo control del equipo? |
|---|---|---:|---|
| Documentación | TDP (form online) | 7 (+2 bonus) | Sí — 100% |
| Documentación | Poster Design & Presentation | 5 | Sí — 100% |
| Documentación | Group Team Interview | 5 | Sí (preparación) + en vivo |
| Documentación | Documentation & Community Contribution | 5 | Sí — 100% |
| Documentación | Short Form Video TDP | 1 | Sí — 100% |
| Conducta | Sportsmanship | 3 | Sí — en vivo |
| **Subtotal documentación + conducta** | | **26 (+2 bonus = 28)** | |
| Juego | Gameplay (partidos) | 30 | Depende del robot en cancha (fuera de alcance de esta carpeta) |
| **TOTAL** | | **56 (+2 bonus)** | |

---

## 1. TDP — Technical Documentation Paper (7 pts + 2 bonus)

Archivo principal: `docs/competencia/TDP.md` · BOM de apoyo: `docs/competencia/BOM.md`

| Componente | Criterio | Pts máx | Dónde lo cubrimos (archivo + sección) | Nivel apuntado | Estimado hoy | Qué falta para el máximo |
|---|---|---:|---|---|---:|---|
| TDP | **Electrical** (replicable + razonamiento con datos + uso de recursos) | 5 | `TDP.md` §1 (pinout doble-punta, power, buses I²C, bring-up con fallos reales) + `BOM.md` §1-§2 | Excellent | **Proficient (3)** | Cerrar BOM con **costos reales** (Teensy, N6, OTOS, BNO, MP1584, batería, Zircon) + costo total/robot; medir set-points de los 6 buck y power budget; reetiquetar CPU/latencias como "objetivo" o medirlas con scope. El "uso de recursos" del criterio hoy está vacío. |
| TDP | **Mechanical** (estrategia + iteraciones + trade-offs/restricciones) | 5 | `TDP.md` §2 (KIWI 120°, sin kicker, iteración OTOS lámina/A4/cartón 9.5%→0%→6.5%, deadzone PWM) | Excellent | **Proficient (3)** | Specs del **motor 2026** (modelo/V/RPM/torque/reducción) y **rueda omni** (Ø/material/rodillos); **diámetro/peso/tamaño** del robot vs límite reglamentario; **CAD/STL del chasis 2026**; validar WHEEL_ANGLES/WHEEL_RADIUS (hoy TENTATIVO). Sin esto un lector no puede reconstruirlo. |
| TDP | **Software** (insight estructura/función + control de versiones + flowcharts/pseudocódigo) | 5 | `TDP.md` §3 (módulos puros/glue, tabla de 8 módulos, bug int16 omega*100, 4 ramas `agente/*`, commits [WIRE BREAKING], flowchart ASCII + pseudocódigo gk_intercept, FSM dual) | Excellent | **Excellent (5)** | Criterio ya maximizado. Mantener: fijar UN conteo de tests trazable (ver fila inferior) y declarar honestamente features code-complete-no-validadas-en-HW. |
| TDP | **Presentation / Narrativa del documento** (organizado + narrativa clara del recorrido) | 5 | `TDP.md` §4 (mapa de navegación §4.3, "cómo leer", narrativa campeones 2025, lecciones §4.2) | Excellent | **Proficient (3)** | **Traducir a inglés**; resolver `[NOMBRE DEL EQUIPO]` (es la identidad del form); embeber 4-5 imágenes reales con caption+cita. |
| TDP | **Bonus +1 — Open-source CAD/PCB/esquemáticos** | +1 | `TDP.md` §5.1 + repo: `hardware/electronics/pcb_design/{top,down}_board` (EasyEDA SCH+PCB+gerbers), `Zircon.pdf` | Reclamado (parcial) | **+0.5 (riesgo)** | El PCB y esquemáticos SÍ están open-source; **falta el CAD/STL del chasis 2026** (solo hay 2025 con dribbler/solenoide descartados). Un juez estricto descuenta por "CAD ausente". Subir el STL 2026 aunque sea WIP. |
| TDP | **Bonus +1 — Open-source software** | +1 | `TDP.md` §5.2 + repo: `LICENSE` (MIT), firmware 3 placas, 40+ carpetas de test, `scripts/run-host-tests.sh`, `calib-lab-n6.py` | Reclamado | **+1 (seguro)** | Bien ganado. Mejora: link directo verificable a cada artefacto para que el juez confirme en 10 s. |
| **Subtotal TDP** | | **7 (+2)** | | | **≈4-5 / 7 + ≈1.5 bonus** | |

> Nota de escala: la rúbrica del TDP suma 4 criterios × 0/1/3/5 (=20 internos) que el form normaliza a 7 pts. Con Software=5 + tres Proficient=3 → 14/20 ≈ **4-5 de 7**. Cerrando BOM+specs+CAD+traducción sube a **6-7 de 7 + 2 bonus**.

---

## 2. Poster — Poster Design & Presentation (5 pts)

Archivo: `docs/competencia/POSTER.md` (maqueta A1 apaisado, máx 70.7×100 cm) · BOM: `docs/competencia/BOM.md`

| Componente | Criterio | Pts máx | Dónde lo cubrimos (archivo + sección) | Nivel apuntado | Estimado hoy | Qué falta para el máximo |
|---|---|---:|---|---|---:|---|
| Poster | **Abstract** (resume cada componente crítico, lenguaje científico, intención de compartir) | 5 | `POSTER.md` Zona B "ABSTRACT" | Excellent | **Proficient (3)** | Traducir a inglés; **matizar el 403/33** (no sobrevender capacidad operativa: visión sin calibrar es el bloqueante #1). |
| Poster | **Method / Robot Production / Design** (producción completa + justificación + lenguaje/sensores + BOM + costo + tiempo) | 5 | `POSTER.md` Zonas C-D-E-F + `BOM.md` §1-§3 | Excellent | **Proficient (3)** | **Cerrar el BOM con costos** y costo total (elemento OBLIGATORIO "tiempo y costo de desarrollo"); reemplazar la tabla débil de Zona E por la de `BOM.md`. |
| Poster | **Data / Results / Discussion** (datos de test + modificaciones MAYORES por el testing + gráficos/tablas + método repetible) | 5 | `POSTER.md` Zonas G-H (tabla 8 iteraciones testeo→dato→modificación + 4 métodos de test) | Excellent | **Proficient (3)** | Producir **Fig.8** (crecimiento de tests) y **Fig.9** (error odometría por superficie) como gráficos reales; reetiquetar CPU/latencias (objetivo, no medición). |
| Poster | **Photos / Images** (abundantes, excelente calidad, etiquetadas y citadas) | 5 | `POSTER.md` Fig.1-11 (placeholders) + repo: 5 PDFs de esquemático (TOP/DOWN/Zircon) | Excellent | **Developing (0)** ⚠️ | **CERO imágenes existen en el repo.** Tomar/generar las 11 figuras (robot, equipo, banco, diagramas, 2 gráficos) y guardarlas en `docs/competencia/assets/`. **Cap más duro del poster.** |
| Poster | **Layout** (lógico, fuentes consistentes, sin errores de ortografía, original/profesional) | 5 | `POSTER.md` grilla 12 columnas + paleta + tipografías (Zona Pie) | Excellent | **Satisfactory (1)** | **Maquetar el A1 real** (Figma/Inkscape/tikzposter → PDF 300dpi); corregir ortografía del inglés; **unificar nombre IITA** (LICENSE="Innovación" vs README/POSTER="Informática"); completar `[NOMBRE DEL EQUIPO]`. |
| Poster | **Presentation** (presente toda la sesión + comprometido + responde todo) | 5 | `POSTER.md` Zona I + checklist interno + `ENTREVISTA-PREP.md` banco de preguntas | Excellent | **Satisfactory (1)** | Se puntúa en VIVO. Confirmar roster, ensayar rotación de 4 integrantes, preparar one-pager para regalar. |
| **Subtotal Poster** | | **5** | | | **≈1-2 / 5 hoy · 4 realista · 5 con ejecución impecable** | El cuello de botella es **EJECUCIÓN** (imágenes + maquetar + traducir + costos), no el contenido escrito. |

---

## 3. Group Team Interview (5 pts)

Archivo de preparación: `docs/competencia/ENTREVISTA-PREP.md` (Show&Tell + Teamwork-Task + Questions)

| Componente | Criterio | Pts máx | Dónde lo cubrimos (archivo + sección) | Nivel apuntado | Estimado hoy | Qué falta para el máximo |
|---|---|---:|---|---|---:|---|
| Interview | **Teamwork & Communication** (colaboración fluida, roles claros, TODOS contribuyen) | 5 | `ENTREVISTA-PREP.md` §1 (Show&Tell 90s repartido) + §2 (roles por área) + §5 (protocolo pase de palabra) | Excellent | **Proficient (3)** | Bindear `[INTEGRANTE A/B]` → María/Elías; traducir a inglés y ensayar; confirmar quién viaja; unificar el conteo de tests para que ambos digan lo mismo. |
| Interview | **Technical Understanding** (fluidez técnica fuerte + resolución de problemas) | 5 | `ENTREVISTA-PREP.md` §4 (6 categorías con dato+por qué; 4 historias 💡 testeo→dato→cambio, verificadas contra código) | Excellent | **Proficient (3)** | Actualizar **403/33 → 624/44** (la pantalla dirá 624 si corren el runner); cerrar `[GAP]` tocables (ESP-NOW partner, chasis 2026). |
| Interview | **Task Execution** (completa eficiente + enfoques innovadores) | 5 | `ENTREVISTA-PREP.md` §3 (plan 30s, mapa de archivos, flash offline, reuso de módulos puros + fallback byte-idéntico) | Excellent | **Satisfactory (1)** | **Corregir rutas §3.2** (`src/...` → `software/teensy/Soccer 2026/src/...`); aclarar que el runner cubre módulos puros (shared+down) y el resto es on-target; verificar `pio` desde el cwd correcto. |
| **Subtotal Interview** | | **5** | | | **≈9-11 / 15 internos ≈ 3 / 5 hoy** | Techo en Proficient hasta traducir + arreglar rutas + fijar números. |

---

## 4. Documentation & Community Contribution (5 pts)

Se alimenta de TDP + Poster + BOM + repo open-source.

| Componente | Criterio | Pts máx | Dónde lo cubrimos (archivo + sección) | Nivel apuntado | Estimado hoy | Qué falta para el máximo |
|---|---|---:|---|---|---:|---|
| Doc & Community | **TDP Quality** (lleno de info, fácil de entender, con fuentes/links) | 5 | `TDP.md` completo + `BOM.md` (cifras verbatim del repo) | Excellent | **Proficient (3)** | Traducir; embeber imágenes; agregar links verificables a cada fuente/artefacto. |
| Doc & Community | **Poster/Presentation/Interview** (informativo, ayuda a otros a replicar) | 5 | `POSTER.md` + `ENTREVISTA-PREP.md` | Excellent | **Proficient (3)** | Mismas dependencias (imágenes + maquetar + traducir). |
| Doc & Community | **Open Source** (TODO lo necesario para aprender, publicado) | 5 | Repo MIT: firmware 3 placas, PCB EasyEDA, gerbers, tests, recetas, visión Python | Excellent | **Proficient (3)** | **Confirmar repo público**; subir CAD/STL 2026; corregir la ruta rota de la BOM COMM (`gerber_file/Placas/Comm/...`); completar part numbers de pasivos o matizar "re-fabricable tal cual". |
| **Subtotal Doc & Community** | | **5** | | | **Proficient (3)** | Sube a Excellent (5) con traducción + imágenes + open-source completo y explicado. |

---

## 5. Short Form Video TDP (1 pt)

Archivo: `docs/competencia/VIDEO-GUION.md` (guion <3 min, feature: testing host-native)

| Componente | Criterio | Pts máx | Dónde lo cubrimos (archivo + sección) | Nivel apuntado | Estimado hoy | Qué falta para el máximo |
|---|---|---:|---|---|---:|---|
| Video | **Easy to follow / un competidor par lo entiende** (criterio único, techo = Satisfactory=1) | 1 | `VIDEO-GUION.md` Bloques 1-5 (problema→solución→demo 44 suites verdes→receta→cierre, ~2:55, subtítulos EN) | Satisfactory (=1, máximo) | **0 hoy (guion, no video)** | **Grabar el video** con subtítulos EN; corregir el comando en pantalla (Paso 3: `cd software/teensy/Soccer 2026` + `lib/Unity/src`); alinear cifra a 624/44 en todos los deliverables. |
| **Subtotal Video** | | **1** | | | **1 cuando se produzca** | El guion soporta el punto completo; el punto solo existe con el archivo de video renderizado. |

---

## 6. Sportsmanship (3 pts)

Sin archivo dedicado — es conducta en vivo. Plan en `MEJORAS-PENDIENTES.md` §(d).

| Componente | Criterio | Pts máx | Dónde lo cubrimos | Nivel apuntado | Estimado hoy | Qué falta para el máximo |
|---|---|---:|---|---|---:|---|
| Sportsmanship | **Apoya activamente a otros equipos** (0/1/2/3) | 3 | Plan: one-pager para regalar + ayuda en pits + actitud en cancha | 3 (Excellent) | **Sin evidencia aún** | Ejecutar en vivo: ayudar a otros equipos, compartir el one-pager open-source, actitud positiva. Es de las formas más baratas de sumar 3 pts. |

---

## 7. Conteo de tests (consistencia transversal — afecta credibilidad en TODOS los deliverables)

| Fuente | Cifra que aparece | Estado |
|---|---|---|
| `TDP.md`, `POSTER.md`, `ENTREVISTA-PREP.md`, `BOM.md` | 403 tests / 33 envs (2026-06-03) | Conservadora |
| MEMORY del equipo | 470 / 37 (post-merge) | Intermedia |
| `scripts/run-host-tests.sh` corrido 2026-06-04 | **624 tests / 44 suites / 0 fallos (exit 0)** | **Verificado HOY** |
| `VIDEO-GUION.md` | 624 / 44 | Ya actualizado |

> **Acción:** correr el runner el día previo a entregar/grabar y **propagar UNA sola cifra** a los 5 deliverables + README + MEMORY. Tres cifras distintas en circulación = munición para un juez adversarial y riesgo de que dos integrantes digan números distintos (mata Teamwork & Communication).

---

## TOTAL ESTIMADO

| Bloque | Pts máx | Estimado HOY (lo que ve un juez) | Estimado tras ejecutar MEJORAS-PENDIENTES |
|---|---:|---:|---:|
| TDP (4 criterios → 7 pts) | 7 | ~4 | 6-7 |
| TDP bonus (CAD/PCB + software) | +2 | +1.5 | +2 |
| Poster | 5 | 1-2 | 4-5 |
| Group Team Interview | 5 | 3 | 4-5 |
| Documentation & Community | 5 | 3 | 5 |
| Short Form Video | 1 | 0 (guion) | 1 |
| Sportsmanship | 3 | 0 (sin evidencia) | 3 |
| **Subtotal documentación + conducta (sin Gameplay)** | **26 (+2)** | **≈12-13.5 / 28** | **≈25-28 / 28** |
| Gameplay (partidos, fuera de esta carpeta) | 30 | — | — |
| **TOTAL competencia** | **56 (+2 bonus)** | **n/a (depende de Gameplay)** | **n/a (depende de Gameplay)** |

> **Lectura honesta:** sobre los **28 pts de documentación + conducta + bonus** (los 100% bajo control del equipo), hoy estamos en **≈12-13.5** porque las versiones están en español, sin imágenes, con placeholders y sin video grabado. **La estructura y el contenido escrito ya apuntan a Excellent en casi todo; lo que falta es EJECUCIÓN.** Ejecutando la checklist de [MEJORAS-PENDIENTES.md](MEJORAS-PENDIENTES.md) el techo realista es **≈25-28 / 28**. Gameplay (30 pts) se juega en cancha y queda fuera del alcance de esta carpeta.
