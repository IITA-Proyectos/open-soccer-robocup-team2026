---
title: "Mejoras pendientes priorizadas — RoboCupJunior Soccer Open 2026"
date: 2026-06-04
status: backlog-ejecutable
idioma: español-rioplatense (este doc es interno; los DELIVERABLES finales van en INGLÉS)
fuente: consolidación de faltantes + mejoras + gaps de los 5 jueces adversariales (Poster, TDP, Video, Entrevista, BOM)
---

# Mejoras pendientes — lo que falta para llegar al máximo

> Consolida **todos** los faltantes, mejoras y gaps de los 5 veredictos adversariales contra la rúbrica oficial.
> Agrupado por: **(a) datos a conseguir · (b) fotos/diagramas a producir · (c) mejoras de contenido por deliverable · (d) acciones de competencia.**
>
> Prioridad: **P0 = bloqueante** (sin esto el deliverable es inválido o pierde la mitad del puntaje) · **P1 = alto impacto** (sube de Proficient a Excellent) · **P2 = pulido**.

---

## (a) DATOS A CONSEGUIR — del equipo / banco / facturas

| # | Prio | Dato | Por qué (qué criterio desbloquea) | Dónde se usa |
|---|---|---|---|---|
| A1 | **P0** | **Nombre oficial del equipo** registrado en RoboCup Incheon 2026 — ✅ RESUELTO 2026-06-05: **IITA Low Battery Messi** | Identidad del TDP form, Title/ID del Poster, lower-third del video, tarjeta de entrevista | TDP §6 línea 7, POSTER Zona A, VIDEO Bloque 1, ENTREVISTA §1-2 |
| A2 | **P0** | **Región / regional de clasificación** exacta ✅ RESUELTO 2026-06-05: **Roboliga Argentina 2025 — final nacional (UAI)**, Salta/Argentina | Title/Identification (elemento obligatorio del poster) | POSTER Zona A, TDP §6, VIDEO lower-third, ENTREVISTA |
| A3 | **P0** | ✅ RESUELTO 2026-06-13: **Roster** — **Director del proyecto:** Gustavo Viollaz (coordinación, integración de las 3 placas, sesiones de banco; **NO viaja a Corea** por obligaciones laborales). **Coach principal (viaja):** Enzo Juárez Velázquez (guió el diseño de PCB con IA / VIBE PCB design; en Incheon además dirige al equipo IITA de RCJ Rescue Line). **Coach secundaria (viaja):** Cecilia Budeguer (respaldo de acompañamiento). **Competidores (viajan):** María Virginia Viollaz (18, visión/estrategia) y Elías Cordero (18, electrónica/mecánica) — ambos aprendieron a diseñar con IA | Presentation, Teamwork & Communication, créditos | ENTREVISTA §1-3, POSTER Zona B2, TDP roster |
| A4 | **P0** | **Costos reales** de los componentes que dominan el presupuesto: 2× OpenMV N6, 2× SparkFun OTOS, 3× Teensy (4.0×2+4.1), Zircon (precio Robomov), 4× VL53L7CX, batería LiPo, 3× motores, 3× ruedas, BNO055, MP1584, ESP32-C6, HC-SR04 | "Tiempo y costo de desarrollo" es elemento OBLIGATORIO del poster; desbloquea TDP Electrical "uso de recursos" (Proficient→Excellent) | BOM §1, §3.1; POSTER Zona E; TDP §1.8 |
| A5 | **P0** | **Costo total por robot + total 2 robots + conversión ARS** | Idem A4 — hoy no existe ninguna cifra total (solo ≈USD 8.20 de pasivos verificados) | BOM §3.1, POSTER Zona E · ⚙️ AVANCE 2026-06-05: existe la planilla `docs/competencia/BOM-COSTOS-TEMPLATE.md` con la estructura de carga (filas + totales/robot + total 2 robots + ARS); el dato real sigue abierto (equipo/facturas). ✅ AVANCE 2026-06-07: BOM §1 con **Costo total por fila + subtotales** (un solo valor = el más alto por ítem) + totales en §3.1: **≈USD 1.168/robot todo nuevo · ≈887 reusando el CENTRAL · ≈2.055–2.336 los 2 robots**. Batería **6800 mAh** cargada con características. ✅ TC RESUELTO 2026-06-13: **1480 ARS = 1 USD** — el equivalente en pesos (USD × 1480) es el **MÍNIMO** de referencia; el costo *landed* real en Argentina es **MAYOR** por impuestos y restricciones de importación. Pendiente del equipo: precio Zircon suelto, motor real, C-rating/marca batería, horas. |
| A6 | **P0** | **Specs del motor 2026**: modelo / voltaje / RPM / torque / reducción / encoder | Replicabilidad mecánica (estándar de oro RCJ); TDP Mechanical Proficient→Excellent | TDP §2, BOM §1.4 |
| A7 | **P0** | **Specs de la rueda omni**: Ø / material / n.º rodillos / impresa o comprada | Idem A6 | TDP §2, BOM §1.5 |
| A8 | **P0** | **Diámetro / peso / tamaño del robot** vs límite reglamentario RCJ | Replicabilidad mecánica + verificar legalidad | TDP §2 |
| A9 | **P1** | **Spec de batería**: mAh / C-rating / marca / peso / packs por robot / autonomía medida | Power budget de Electrical; sustentabilidad | BOM §1.5, TDP §1 |
| A10 | **P1** | **Set-points medidos de los 6 buck MP1584** (multímetro) | "Datos no medidos" hoy son objetivos; cierra power budget de Electrical | TDP §1, BOM |
| A11 | **P1** | **Nuevo vs Reusado de la TRACCIÓN** (motores + ruedas) — único subsistema mayor sin clasificar | Sustentabilidad (criterio premiado por RCJ) | BOM §1.4-1.5 |
| A12 | **P1** | **Cifra única de tests** verificada: correr `scripts/run-host-tests.sh` el día previo (hoy 834/60/0) y propagar a TODOS los deliverables | Credibilidad transversal; consistencia = puntos en Teamwork | TODOS (ver RUBRICA-COBERTURA §7) | ✅ RE-MEDIDO 2026-06-13 17:43 ART: cifra única **834 tests · 60 suites · 0 fallos** (la suite siguió creciendo: 658 → 834). Propagar a los 12 deliverables ES+EN (`docs/competencia/` + `docs/competencia/en/`) y a Fig.8. Queda solo re-correr el runner el día previo a entregar por si cambió. |
| A13 | **P1** | **Métricas CPU% y latencias medidas** con scope/timestamps serial (o reetiquetar como "objetivo de diseño") | Honestidad de datos; un juez técnico castiga datos inventados | TDP §1, POSTER Zona H |
| A14 | ✅ RESUELTO 2026-06-05 | VL53L7CX = **USD 19.95/u** (Pololu #3418); el ~USD26 era del VL53L1X. Corregido en BOM §2/§3 | Rigor de "razonamiento basado en datos" (TDP Electrical) | BOM §1.2/§2, TDP §1 |
| A15 | ✅ DOCUMENTADO 2026-06-05 | comunicación robot-a-robot (ESP-NOW vía ESP32-C6) como **roadmap declarado** en POSTER + TDP §4.6; hardware listo, falta integrar+validar en banco | Pregunta de entrevista (Software/Strategy) | ENTREVISTA §6, TDP |
| A16 | **P2** | **Plantilla oficial de BOM de RCJ** (verificar si existe y transcribir) | Cumplir formato exacto del comité | BOM §nota plantilla |

---

## (b) FOTOS / DIAGRAMAS A PRODUCIR — ✅ 4 figuras generadas; FALTAN las FOTOS

> **Actualización 2026-06-05/10:** ya existen en `docs/competencia/assets/` 4 figuras
> generadas: `fig2_dataflow.png`, `fig4_fsm.png`, `fig8_test_growth.png`,
> `fig9_otos_error.png` (= ítems B7/B8/B9/B10 de la tabla). **Lo que sigue faltando y
> nadie puede generar por software: las FOTOS reales (B1-B6: robot, equipo, banco,
> bodge) y los QR (B13).** Sigue siendo el cap más duro del poster. Activo gráfico
> extra: los 5 PDFs de esquemático (`Schematic_*.pdf`, `Zircon.pdf`).
>
> Guardar todo en `docs/competencia/assets/`.

| # | Prio | Figura | Tipo | Cómo producirla |
|---|---|---|---|---|
| B1 | **P0** | **Fig.5** Robot 3/4 sobre fondo neutro | Foto | Cámara, fondo blanco/gris, luz pareja |
| B2 | **P0** | **Fig.6** Robot lateral mostrando la pila de placas | Foto | Idem, vista de perfil |
| B3 | **P0** | **Fig.7** Anillo de línea (32 sensores) | Foto | Macro del DOWN board |
| B4 | **P0** | **Fig.1** Equipo (con trofeo del Nacional 2025 si se tiene) | Foto | Para Title/ID + Bloque 1 del video |
| B5 | **P1** | **Fig.10** Banco serial decodificando WorldSnapshot | Foto/captura | Pantalla con el diag corriendo |
| B6 | **P1** | **Fig.11** Bodge de los 4 ToF / power-cycle | Foto | Macro del bodge |
| B7 | **P1** | **Fig.2** Diagrama de flujo de datos (TOP/CENTRAL/DOWN con flechas etiquetadas: WorldSnapshot 31B, broadcast línea+OTOS) | Diagrama | Excalidraw/draw.io/TikZ |
| B8 | **P1** | **Fig.4** Flowchart de la FSM táctica | Diagrama | Idem |
| B9 | **P0** | **Fig.8** Crecimiento de tests (barras: 246→262→324→354→403→470→545→658→834→858) — ✓ regenerada 2026-06-14 con el punto 858 | Gráfico | matplotlib desde los snapshots del repo (`gen_figuras.py`) |
| B10 | **P0** | **Fig.9** Error de odometría OTOS por superficie (barras: A4-lámina 9.5% / sin lámina 0% / cartón 6.5%) | Gráfico | matplotlib desde datos de banco |
| B11 | **P1** | **Fig.3** Esquemático recortado (TOP o Zircon) | Recorte PDF | Usar los PDFs ya existentes |
| B12 | **P1** | Captura de los tests en verde (834/60/0) | Captura | Para TDP, poster y video |
| B13 | **P0** | **2 QR reales**: repo público + video TDP <3min | Generar QR | Cuando A1/repo/video estén listos (POSTER Zonas B3 e I) |

---

## (c) MEJORAS DE CONTENIDO POR DELIVERABLE

### POSTER (`POSTER.md`)
- **P0** — Traducir TODO a inglés y correr corrector ortográfico EN antes de imprimir (mantener banner ES solo en el .md de trabajo). ✅ RESUELTO 2026-06-05 (traducción): el deliverable EN ya existe en `docs/competencia/en/` y está verificado sin español filtrado. Queda solo correr el corrector ortográfico EN antes de imprimir.
- **P0** — Producir el **artefacto A1 real** (Figma/Inkscape/LaTeX-tikzposter) siguiendo la grilla ya definida → PDF 300dpi → revisar legibilidad a 1.5 m. Hoy es un .md de texto; Layout no pasa de Satisfactory sin el PDF.
- **P0** — Reemplazar la tabla BOM débil de Zona E por la versión condensada de `BOM.md` (part numbers + LCSC + nuevo/reusado) y **cerrar costos** (A4/A5).
- **P1** — Matizar el **403/33 → 834/60** en el Abstract y aclarar que es cobertura de lógica host-native; ciertas features (visión LAB+homografía, trilateración, strafe) están code-complete pero pendientes de banco. Protege contra preguntas filosas y mantiene credibilidad.
- **P1** — Reetiquetar CPU%/latencias como "objetivo de diseño" o medirlas (A13).
- **P2** — Completar Title/ID (A1/A2) y la tabla de roles "quién hizo qué".

### TDP (`TDP.md`)
- **P0** — Traducir a inglés y resolver `[NOMBRE DEL EQUIPO]` (línea 7 y §6) — los dos únicos faltantes que pueden invalidar/penalizar el TDP de entrada. ✅ RESUELTO 2026-06-05 (traducción): el TDP EN ya existe en `docs/competencia/en/` verificado sin español filtrado; queda solo el corrector EN. ✅ Nombre del equipo = **IITA Low Battery Messi** (resuelto 2026-06-05).
- **P0** — Completar BOM §1.8 con costos reales + total + tiempo de desarrollo → convierte Electrical de Proficient a Excellent.
- **P1** — Embeber 4-5 imágenes reales con caption+cita (robot sup/lateral, cada PCB poblada, bodge ToF, tests en verde, equipo Nacional 2025); convertir `[FOTO]/[DIAGRAMA]` de §6 en Fig. numeradas referenciadas desde el cuerpo. Sube Presentation y arrastra Documentation.
- **P1** — Mini-ficha de motor + rueda 2026 (A6/A7) y diámetro/peso/tamaño medidos (A8) → Mechanical a Excellent.
- **P1** — Subir CAD/STL del chasis 2026 (aunque sea WIP) o declarar fecha objetivo → asegura el bonus CAD entero.
- **P1** — Añadir 1-2 gráficos del ciclo testeo→evaluación→modificación (curva OTOS lámina/A4/cartón como barras de %error) — la rúbrica premia el vínculo MOSTRADO, no solo narrado.
- **P2** — En §5, link directo verificable a cada artefacto open-source (ruta exacta en el repo público).

### VIDEO (`VIDEO-GUION.md`)
- **P0** — **Grabar el video** (<3:00) con subtítulos quemados en INGLÉS. Hoy el entregable real vale 0 (es guion).
- **P0** — Corregir el comando sobreimpreso del Paso 3 (Bloque 4): mostrar `cd "software/teensy/Soccer 2026"` y el include real `lib/Unity/src` (no `lib/Unity`). Hoy un par que copie lo que ve falla → baja replicabilidad de Excellent a Proficient. ✅ RESUELTO 2026-06-05: el guion del Paso 3 ya muestra `cd "software/teensy/Soccer 2026"` + el include `lib/Unity/src`. (El comando quedará en pantalla al GRABAR — la grabación sigue pendiente, ver ítem P0 de arriba.)
- **P1** — Reemplazar `[REPO URL]` por la URL real (verificada PUBLIC: `github.com/IITA-Proyectos/open-soccer-robocup-team2026`) en Bloque 5 y tarjeta final.
- **P1** — Alinear la cifra a 834/60/0 (re-correr el runner el día de grabar) y mostrar el LICENSE MIT + org pública durante el Bloque 5. ✅ RE-MEDIDO 2026-06-13 (cifra): el guion usa **834/60/0** alineado a los demás deliverables. Queda re-correr el runner el día de grabar y mostrar LICENSE+org en el Bloque 5 (al GRABAR).
- **P2** — Congelar y hacer zoom ≥2s en la línea `Tests: 834 | Failures: 0  (Envs: 60 | OK: 60)` — es el frame que gana el punto.

### ENTREVISTA (`ENTREVISTA-PREP.md`)
- **P0** — Crear `ENTREVISTA-PREP.en.md` (o sección 🇬🇧 inline) con §1 (Show&Tell), las 4 respuestas 💡 de §4 y los cierres; agendar 3 ensayos en voz alta. ✅ RESUELTO 2026-06-05 (traducción): el deliverable EN de entrevista ya existe en `docs/competencia/en/` verificado sin español filtrado; queda solo el corrector EN. ⚠️ Los 3 ensayos en voz alta siguen pendientes (depende del equipo).
- **P0** — Corregir **rutas §3.2**: anteponer `software/teensy/Soccer 2026/` a cada `src/...` (ahí vive `platformio.ini` y `src/`). Load-bearing para la velocidad del Teamwork-Task en vivo. ✅ RESUELTO 2026-06-05: las rutas de §3.2 ya llevan el prefijo `software/teensy/Soccer 2026/`.
- **P0** — Reemplazar TODAS las apariciones de "403 tests / 33 entornos" por la cifra verificada del día (834/60/0); agregar al checklist §7 "correr el runner el día previo". ✅ RE-MEDIDO 2026-06-13 (cifra): ya propagada a 834/60/0 en el deliverable ES+EN. Queda solo agregar/confirmar en el checklist §7 "correr el runner el día previo".
- **P1** — Añadir frase honesta en §3.3: el runner compila los módulos PUROS (shared+down) host; los tests de central/top usan Arduino y se compilan on-target → convierte una sobre-venta en respuesta de ingeniería madura.
- **P1** — Bindear placeholders (A3); verificar `pio run -e central_robot1 -t upload` desde el cwd correcto (anotarlo en §7).
- **P2** — Mini-guion de Teamwork-Task que NO dependa de visión recalibrada (resolver con ToF/odometría o pedir 5 min de recalibración) — muestra resolución de problemas en vez de quedar trabados.

### BOM (`BOM.md`)
- **P0** — Cerrar §3.1 con los ~10 precios faltantes (A4) + total/robot + total 2 robots + ARS (**TC 2026-06-13 = 1480 ARS/USD**; el equivalente en pesos = USD × 1480 es el MÍNIMO de referencia — el *landed* real es MAYOR por impuestos/restricciones de importación).
- **P1** — Corregir la ruta de la BOM COMM en §5/Fuentes: `gerber_file/Placas/Comm/BOM_Board1_PCB1_2026-04-20.xlsx` (la citada `comm-board/...` está rota). ✅ RESUELTO 2026-06-05: la ruta de la BOM COMM ya apunta a `gerber_file/Placas/Comm/BOM_Board1_PCB1_2026-04-20.xlsx` y el `comm-board/...md` referenciado existe.
- **P1** — Aclarar el mismatch VL53L1X vs VL53L7CX (A14).
- **P2** — Suavizar los rótulos "nivel apuntado: Excellent" de cada encabezado (hoy aspiracionales con tantos `[COSTO?]`) → cambiar por "alimenta: <criterio>".
- **P2** — Matizar "re-fabricable tal cual": los CSV tienen part vacío en pasivos R/C → "activos/críticos especificados; pasivos genéricos por valor".

### TRANSVERSAL
- **P0** — **Unificar el nombre de la organización**: `LICENSE` dice "Instituto de **Innovación** y Tecnología Aplicada"; `README`/`POSTER`/`BOM` dicen "Instituto de **Informática** y Tecnología Aplicada". Elegir uno (verificar el registro legal de la Fundación) y propagarlo. El criterio Layout penaliza errores de consistencia que un juez cruza. ✅ RESUELTO 2026-06-05: unificado a **"Instituto de Innovación y Tecnología Aplicada"** en todos los docs.
- **P0** — Fijar **un solo conteo de tests** (A12) y propagarlo a README/ESTADO-ACTUAL/MEMORY + los 5 deliverables. ✅ RE-MEDIDO 2026-06-13 17:43 ART: cifra única **834/60/0** (658 → 834) a propagar a los 12 deliverables ES+EN + Fig.8 (ver A12). Queda re-correr el runner el día previo por si cambió.

---

## (d) ACCIONES DE COMPETENCIA — conducta, presencia, open-source

| # | Prio | Acción | Criterio que suma |
|---|---|---|---|
| D1 | **P0** | **Confirmar que el repo es PÚBLICO** y la org `IITA-Proyectos` accesible (verificado PUBLIC vía gh API el 2026-06-04 — re-confirmar antes de viajar) | Bonus software +1, Open Source de Doc & Community, QRs del poster/video |
| D2 | **P0** | **Subir CAD/STL del chasis 2026** (aunque sea WIP) | Bonus CAD/PCB +1 entero (hoy en riesgo), replicabilidad mecánica |
| D3 | **P1** | **Presencia en la sesión de poster**: 4 integrantes rotando toda la sesión, banco vivo (tests en verde + diag decodificando), banco de preguntas por categoría | Presentation (Poster) Excellent |
| D4 | **P1** | **Sportsmanship (3 pts)**: ayudar activamente a otros equipos en pits, compartir el one-pager open-source, actitud positiva en cancha | Sportsmanship 0→3 (de lo más barato de sumar) |
| D5 | **P1** | **One-pager imprimible para regalar** (resumen del robot + QR al repo) | Sportsmanship + Community + "intención de compartir conocimiento accionable" | ✅ AVANCE 2026-06-05: el one-pager ya existe en `docs/competencia/ONE-PAGER.md` (+ versión EN en `docs/competencia/en/ONE-PAGER.md`). Queda: insertar el QR real (depende de A1/repo/video) e imprimirlo. |
| D6 | **P1** | **Ensayar la Group Team Interview** en inglés: rotación de 4 integrantes con el banco de preguntas (General/Eléctrica/Mecánica/Estrategia/Software/Doc) | Teamwork & Communication + Technical Understanding |
| D7 | **P2** | **Generar y pegar los 2 QR reales** (repo + video) y confirmar que el video existe y dura <3 min | Poster Zonas B3/I; cierra promesas del poster |
| D8 | **P2** | **Imprimir/tener offline** el mapa de archivos corregido (§3.2) + la cifra de tests del día (el wifi del venue no es confiable — por eso se vendorearon libs) | Task Execution en vivo |

---

## (e) MEJORAS DE INGENIERÍA / ROADMAP (control + motores)

> Ideas de modificación a futuro (registradas 2026-06-05). Separan una **restricción de seguridad ACTUAL** de **mejoras post-Incheon / mejores motores**. Detalle técnico en `docs/ESTADO-MADUREZ-FEATURES.md` y en la memoria del coach.

| # | Prio | Idea | Estado / fundamento |
|---|---|---|---|
| E1 | **P0 SEGURIDAD (actual)** | **Cap de potencia de motores al ~70%.** Los motores 2026 son **DC con escobillas COMUNES de 5V** alimentados a **7,4V (LiPo 2S)** → si superan ~70% de duty **se queman**. | Restricción dura HOY. **Pendiente: verificar si el firmware ya limita al 70% efectivo** (hay `MAX_SPEED_MM_S`/saturación de ruedas, pero capea velocidad, no necesariamente % de duty). Si no, fix de seguridad gateado (con OK del equipo + `pio`). |
| E2 | Futuro | **Upgrade de motores** a unidades que toleren 7,4V (idea ya en el roadmap mecánico: motores más cortos con encoders). | Habilita usar >70% y desbloquea E3. En la **definición de motores** del robot-def queda anotado `tipo = brushed 5V DC común → modificar a futuro`. |
| E3 | Futuro (post-mejores-motores) | **Mapa de velocidad por POSICIÓN ABSOLUTA + DIRECCIÓN de movimiento**: 100% en zona central; bajar al acercarse a los bordes para no salirse — pero **solo si se va HACIA el borde** (si está en el borde pero se aleja, puede ir al 100%). | **Estudiar en profundidad**; **otros equipos ya lo implementaron**. Con los motores 5V@7,4V el limitante real es el 70% (E1), no la salida de cancha. **Depende de pose absoluta válida** (eje X de ToF + `TOF_OFFSET_MM`), hoy NO cableada. Encaja como módulo PURO gateado con fallback byte-idéntico cuando se retome. |
| E4 | Futuro (año próximo) | **Bus CANbus troncal** entre las 3 Teensy (reemplaza/complementa los UART punto-a-punto) + **4ª placa ESP32 gateway** que puentea (parte del) bus al exterior por inalámbrico, para **comunicación con el robot compañero** y **telemetría**. | Habilita **monitoreo estilo Fórmula 1** (sensores EN VIVO en entrenamiento → registrar → analizar para mejorar el software; ciclo data-driven → acelera tiempos). Es la evolución de la INTERFAZ del diseño modular (hoy el WorldSnapshot viaja por UART → con CAN se vuelve bus troncal robusto/tolerante a ruido/escalable a N nodos). Va junto al roadmap robot-a-robot (ESP-NOW). Reflejado en TDP §4.6 + POSTER. |

---

## Camino crítico sugerido (orden de ejecución)

1. **Conseguir los datos duros** (A1-A8): nombre, región, roster, costos, specs mecánicas. Son P0 y desbloquean varios criterios a la vez.
2. **Tomar las fotos y generar los 2 gráficos** (B1-B13). Saca Poster→Photos de Developing.
3. **Cerrar el BOM** (A4/A5 + ruta COMM) → eleva Method/Design y Electrical.
4. **Traducir Poster + TDP + material de entrevista a inglés** y correr corrector. — ✅ traducción HECHA 2026-06-05 (6 deliverables EN en `docs/competencia/en/`, verificados sin español filtrado); queda correr el corrector EN antes de imprimir.
5. **Maquetar el A1 real** (PDF 300dpi) y **grabar el video** con subtítulos EN. — sigue pendiente (banco/equipo).
6. **Unificar nombre IITA y el conteo de tests** en todo el repo. — ✅ nombre unificado a "Instituto de Innovación y Tecnología Aplicada"; conteo único re-medido 2026-06-13 17:43 ART = **834/60/0** (658 → 834) a propagar a los 12 deliverables ES+EN + Fig.8.
7. **Ensayar la entrevista** y preparar el one-pager + sportsmanship. — one-pager ✅ creado (`ONE-PAGER.md` ES+EN); ensayo y sportsmanship siguen pendientes (equipo).
8. Re-correr `scripts/run-host-tests.sh` el día previo y propagar la cifra final.
