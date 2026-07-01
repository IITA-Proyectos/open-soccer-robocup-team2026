# TASK-120 — Validar en banco el RETROCESO POST-PATEO "hasta la línea + escape adelante" del arquero (arqueromix)

- **Placa:** CENTRAL (R2, arquero — María/Virginia). TOP `top_robot2_pri` + DOWN `down_robot2` sin cambios.
- **Asignado:** equipo (banco) — María / Virginia / Gustavo
- **Prioridad:** P2 (mejora anti-"salirse de la cancha"; el arquero validado YA anda y NO se toca hasta cerrar esto). Mitiga un problema REAL observado: al volver del pateo cruza su propia línea / se mete al área.
- **Estado:** ✅ **FUNCIONAL VALIDADO EN BANCO por María (2026-07-01): "quedó andando perfecto"** — al volver del pateo va hasta la línea y avanza para despegarse, no cruza. **Pendiente antes de promover a competencia:** el chequeo de SEGURIDAD de DOWN-muerto (paso 4, desconectar DOWN a mitad del retroceso → debe frenar pronto). Compila SUCCESS; `_quieto` y `_kickcorto` byte-idénticos (md5 `8d0168…e59` y `72f2516…9d0`).
- **Build (banco):** `pio run -e central_robot2_arqueromix_retrofreno -t upload`
- **Escape / competencia:** `pio run -e central_robot2_arqueromix_quieto -t upload` (byte-idéntico al validado)
- **Flag:** `-DARQMIX_RETRO_BRAKE_ON_LINE` (nombre histórico; ya NO es un "freno", ver abajo).

## Qué hace (comportamiento FINAL, gateado)
Al VOLVER hacia atrás tras patear, el arquero se metía / cruzaba la línea. El env `_retrofreno` lo cambia a ser
**espejo del HOMING del arranque** (que "siempre anda bien"):

1. **`PATEANDO_atras` retrocede HASTA detectar la línea** (`retroceder_quieto`, PWM 80). Sale por:
   - **`linea()`** → estado `escapar_adelante` (normal).
   - **`!down_link_fresh`** → `acomodar_linea` (SEGURIDAD, ver hallazgo #1 abajo — solo si el sensor DOWN muere).
   - **`AMIX_T_ATRAS_SAFETY_RETRO`=50 s** → `acomodar_linea` (red anti-cuelgue final, no debería dispararse).
2. **`escapar_adelante` (estado NUEVO)** = copia de `inicio_avanzar` del homing: **AVANZA al frente**
   (`avanzar_inicio`, PWM 75) **HASTA que deja de pisar la línea** (`!linea()`), con impulso mínimo
   (`AMIX_T_INICIO_AVANCE_MIN`, cubre parpadeo) + red (`AMIX_T_INICIO_AVANCE_SAFETY`) → `acomodar_linea`.

Es "a condición" (usa la línea), no "a tiempo". **Se ELIMINÓ** el intento previo por freno (`frenar_retroceso` /
`frenar_adelante` / `AMIX_FRENO_RETRO_*`).

## Historia (por qué quedó así)
- **Fix #1:** freno activo por tiempo al pisar la línea. Banco: "se sigue metiendo, muchas veces".
- **Fix #2:** el retroceso va HASTA la línea (se sacaron el gate de frescura + el safety corto de 4 s que lo
  cortaban antes de la línea). Banco: "ya no va hacia adelante cuando cruza".
- **Fix #3 (este):** al pisar la línea, en vez de frenar, **avanza hasta despegarse** (como el homing) — decisión
  de María. + **revisión adversarial** (ver abajo) que restauró una seguridad que el fix #2 había roto.

## Revisión adversarial 2026-07-01 (4 lentes + verificación) — 2 hallazgos
- **#1 (GRAVE, CORREGIDO):** el fix #2 sacó el gate de frescura de DOWN del retroceso. Como `apply_down_line`
  (`amix_comm.cpp:139`) sólo escribe `line_present` cuando llega un frame y **nunca lo resetea al perder el
  enlace**, si DOWN se cae a mitad del retroceso `line_present` queda CONGELADO en `false` → el retroceso corría
  **ciego hasta 50 s** y se salía de la cancha (la rama sin flag cortaba en ~4 s). Falla real en este robot (UART
  sucio por EMI de motores). **FIX aplicado:** se re-agregó el corte `!g_aio.down_link_fresh → acomodar_linea` en
  el retroceso gateado (espejo de la rama sin flag). Sólo actúa si el sensor MUERE; en juego normal DOWN llega
  fresco → sigue yendo "hasta la línea".
- **#2 (LEVE, P2 a titrar en banco):** si el ÚLTIMO frame de DOWN dejó `line_present` congelado en **true** (pisó
  línea justo antes de caerse el enlace), `escapar_adelante` avanza al campo el safety completo (~1200 ms @ PWM 75)
  sin realimentación. Es el mismo caso que el homing (`inicio_avanzar`) y va en dirección segura (al campo, no
  fuera), acotado. **NO se cambió** (para no desviarse del pedido "solo línea"). Al titrar, medir cuánto se mete;
  si molesta, agregar `|| !g_aio.down_link_fresh` a la salida de `escapar_adelante`. *(No se toca el homing porque
  vive en el código base → cambiaría `_quieto`/`_kickcorto`.)*

## Cómo validar (en orden — una perilla por vez)
1. **A/B base:** con `_quieto`, provocar un despeje y mirar el retroceso post-pateo (¿cruza / se mete? = el "antes").
2. **Flashear `_retrofreno` y repetir:** al volver del pateo, en el monitor (`python -m monitor_base --monitor`) el
   `estado` debe ir **`PATEANDO_atras` → `escapar_adelante` → `acomodar_linea`**: retrocede hasta la línea, **avanza
   para despegarse**, no la cruza.
3. **Que NO se meta demasiado al campo** al avanzar (escapar_adelante). Si se mete mucho → es el impulso mínimo del
   homing (`AMIX_T_INICIO_AVANCE_MIN`) / la velocidad (`AMIX_INICIO_AVANCE_PWM`).
4. **SEGURIDAD enlace DOWN (hallazgo #1):** en `PATEANDO_atras`, **desconectar DOWN a mitad del retroceso** →
   debe **frenar pronto** (pasar a acomodar), NO retroceder ~50 s contra la pared. **Chequeo bloqueante.**
5. **NO-REGRESIÓN:** homing de inicio, seguimiento de pelota y resto de la secuencia post-pateo, igual que el
   validado (el cambio solo entra en `PATEANDO_atras`/`escapar_adelante`, gateado).
6. **Rollback:** `_quieto` → comportamiento de hoy.

## Criterio de cierre (humano)
- Al volver del pateo: retrocede hasta la línea, **avanza para despegarse y NO cruza**, repetible (≥5 despejes),
  sin meterse demasiado al campo, sin brownout. La seguridad de DOWN-muerto (paso 4) confirmada. Sin regresión.
- **Decisión:** si anda → promover el flag al arquero de competencia (cambia el binario → **re-validar el arquero
  completo**); si no → quedarse con `_quieto`.

## Perillas de tuning
- Retroceso: red final `AMIX_T_ATRAS_SAFETY_RETRO` (50 s, no tocar salvo que corte antes de la línea).
- Escape adelante: reusa las del homing — `AMIX_T_INICIO_AVANCE_MIN` (impulso mínimo), `AMIX_T_INICIO_AVANCE_SAFETY`
  (red), `AMIX_INICIO_AVANCE_PWM` (velocidad 75). ⚠️ Son COMPARTIDAS con el homing (tocarlas cambia los dos).

## Verificación host (esta sesión, NO reemplaza el banco)
- `_retrofreno` → compila SUCCESS. `_quieto` y `_kickcorto` → **byte-idénticos** (md5 sin cambio) → 100% aislado.
- ⚠️ NO hay test host para esta FSM (Arduino: `millis`/`analogWrite`) → la validación del EFECTO es 100% de banco.

## Relación
- **TASK-119** (pateo corto + notas "el escape no alcanza"): este fix ataca el **momento #2** de esas notas ("al
  volver del pateo"). Probar juntas las features anti-salirse: `_kickcorto` (`f21a653`), `_orientesc` (`e95df40`)
  y este `_retrofreno`.
