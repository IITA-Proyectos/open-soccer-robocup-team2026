---
title: "arqueromix — port del ARQUERO 2025 sobre datos de TOP/DOWN (hermano de centralmix)"
date: 2026-06-21
author: "Claude Opus 4.8 (1M context)"
requested-by: "Virginia Viollaz (desde la compu de Gustavo)"
tipo: firmware-experimental
toca-competencia: NO (carpeta + env nuevos, build aislado)
---

# Sesión 2026-06-21 — arqueromix (arquero 2025 sobre TOP/DOWN)

## Qué se pidió
Virginia: replicar para el ARQUERO lo que el viernes (2026-06-19) se hizo con el delantero
(`centralmix`). Es decir, un programa BASE en la CENTRAL que sea una **máquina de estados
finitos que mueve los motores DIRECTO con PWM como el arquero 2025**, pero reemplazando:
- la lectura de los BNO y las cámaras → **lectura del puerto serie del TOP** (snapshot),
- los sensores de piso → **lectura del puerto serie del DOWN** (LineStatusV2),
manteniendo TOP y DOWN con todo lo que tienen. Pedido explícito: armar la línea/piso
**parecido a como se hizo para el delantero**. Trabajo con superpowers + workflow de análisis
paralelo, aprendiendo de la experiencia del viernes.

## Qué se hizo
1. **Análisis integral (workflow paralelo de 5 lectores + síntesis):** el molde `centralmix`
   (viernes), el delantero 2025, el arquero 2025, el arquero actual y los contratos de serie
   TOP/DOWN. (El workflow corrió en background; en paralelo leí yo mismo el molde completo y
   la transcripción fiel del arquero 2025 para verificar.)
2. **Carpeta nueva `src/arqueromix/`** (espejo de `centralmix`), 8 archivos:
   - `amix_io.h` — variables planas `AmixIO g_aio` (pelota, arcos, heading, línea, árbitro).
   - `amix_comm.{cpp,h}` — único que toca Serial: TOP (Serial7) + DOWN (Serial1) → `g_aio`.
   - `amix_motors.{cpp,h}` — primitivas directas del arquero 2025 (adproporcional/aiproporcional/
     impulso_inicial/avanzar/avanzar_patear/patear_atras), PWM por índice.
   - `amix_fsm.{cpp,h}` — FSM del arquero 2025 portada fiel (10 estados).
   - `amix_config.h` — pines 2026 + constantes 2025 (PWM, tolerancias, tiempos).
   - `main_arqueromix.cpp` — setup/loop mínimos.
   - `DOCUMENTACION.md` + `README.md`.
3. **Env aislado `central_robot2_arqueromix`** en `platformio.ini`:
   `build_src_filter = +<arqueromix/> +<shared/>` → NO compila `src/central/`, no toca ningún
   env existente. Extiende `central_robot2` (Virginia juega arquero con R2).
4. **Compila:** `pio run -e central_robot2_arqueromix` → **SUCCESS** (FLASH ~19 KB, 12,6 s).

## Diferencia clave vs centralmix (mejora pedida por Virginia)
`centralmix` (delantero) dejó como default un **BNO local** del CENTRAL — pero las placas
CENTRAL 2026 no traen BNO propio (el rumbo se procesa en el TOP). `arqueromix` toma el
**heading del snapshot del TOP por serie** (`my_heading_centideg` + bit4 `heading_valid`),
sin Wire ni BNO. Es exactamente lo que pidió Virginia ("la lectura de los BNO la reemplace
por lectura del puerto serie de la placa superior") y además es más simple y correcto.

## Cómo se portó el arquero 2025 (fuente: ANALISIS-FIEL-ARQUERO-2025.md, verificado contra el .cpp)
- **10 estados:** impulso_inicial → moverce_der/izq (patrulla strafe + corrección rumbo en 3
  bandas por `error`) → impulso_der/izq (rebote 350 ms en el borde) → secuencia de despeje
  (pausa 200 → patada 450 → pausa 1000 → retroceso recto hasta ver línea → reposicionar 1000).
- **Patrulla:** `ad/aiproporcional()` con los PWM exactos del 2025 (fronts 50, rear 89/100/40
  según banda de error), ×`pd` (1.0 sin pelota / 1.5 con pelota desviada). Direcciones INA/INB
  verificadas leyendo el fuente 2025 (L186-233).
- **Decisión por la pelota:** cerca+centrada → despeja; desviada → va al lado; banda muerta → para.
- **Línea desde DOWN:** reemplaza los 3 sensores de luz locales por `line_present`/`line_depth`
  (igual patrón que centralmix; el DOWN agrega los 32 sensores en una señal).

## Agregados sobre el 2025 (marcados)
- Gate del árbitro RCJ (`match_running`): no se mueve hasta el START.
- Timeout de seguridad (4 s) en `PATEANDO_atras` (el 2025 no tenía → podía colgarse si nunca
  veía blanco; es un riesgo conocido del análisis fiel §7.5).

## Verificación
- **Compila SUCCESS** (la verificación que SÍ puedo hacer). Cada `archivo:línea` del port se
  ancló contra el fuente 2025 + la transcripción fiel; el molde centralmix se leyó entero.
- **Competencia byte-idéntica por construcción:** solo se agregó una carpeta nueva + un env
  nuevo; no se modificó ningún archivo de `src/central/` ni ningún env existente.
- ⚠️ **Compila ≠ anda.** NADA validado en hardware. El sentido físico de cada motor, el signo
  lateral de la pelota y los umbrales (píxeles→mm) los valida el equipo → **TASK-114**.

## Pendiente (equipo / banco — Claude NO cierra TASKs de hardware)
- TASK-114: validar primitivas (ruedas al aire) → comm (`g_aio` en vivo) → signo lateral →
  re-tuneo umbrales → FSM completa en piso.
- Si anda: incorporar las mejoras P0/P1 del análisis fiel §7 (watchdog, localización ToF/OTOS
  del arco, heading-hold real, ball_predict).

## Referencias
- `src/arqueromix/DOCUMENTACION.md` (estado + diseño completo).
- `src/centralmix/` (el hermano delantero del viernes) + `journal/2026-06-19-centralmix-port-delantero-2025.md`.
- `docs/internal/ANALISIS-FIEL-ARQUERO-2025.md` (la fuente del port).
- `team-tasks/2026-06-21-task-114-validar-arqueromix-banco.md`.
