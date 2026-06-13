# 2026-06-13 — Sintonía fina de calibración DOWN (sensibilidad + habilitar/deshabilitar) + verificación de producción

**Sesión:** Claude (coach) + Gustavo. Repo principal `soccer-main` (branch `main`).
**Tema:** 3 features de calibración fina en la app `monitor-base` + firmware DOWN, iteración
sobre feedback de banco, y verificación de que quedan "en producción" en los dos robots.

---

## Qué se hizo

Se agregaron 3 features pedidas por Gustavo para entender/ajustar mejor los 32 sensores de
línea de la placa DOWN desde la app, sin reflashear cada vez:

1. **Habilitar/deshabilitar sensores** individualmente (click → inspector + botón, o
   doble-click en anillo/barra). Un sensor OFF se excluye del cálculo de línea (como uno
   unhealthy) en `dm_update`.
2. **Sensibilidad global al blanco** (`SENS GLOBAL`, −100…+100, **+ = menos sensible**) con
   mapeo **saturante** (−100 → todo blanco, +100 → nada) y barra de cercanía al umbral por
   sensor.
3. **Sensibilidad por-sensor** (`SENS SET`). `CAL SAVE` persiste todo en **EEPROM v2**.

Todo **aditivo y no-op por default** (todos habilitados, sensibilidad 0 → umbral en el punto
medio = competencia histórica byte-idéntica).

### Commits (todos en `main`, pusheados)

- `f69bc0b` — feat: habilitar/deshabilitar + sensibilidad global y por-sensor (firmware + app).
- `0173999` — fix: saturación de sensibilidad + contraste/visual + **6 hallazgos de revisión
  adversarial** (workflow 17 agentes). El importante: `sens=0` NO era byte-idéntico
  (`lroundf` vs `mid()` floor → +1 en sumas carpet+white impares; los 55 tests no lo cazaron
  porque todas las calibs de test usan sumas pares). Fix: `total==0 → mid()` + test de paridad.
- `a4e8ae6` — fix: sensores deshabilitados en gris + X (no amarillo) en el anillo.
- `476bf2d` — feat: doble-click en anillo/barra habilita-deshabilita directo.
- `d27c9c1` — feat: la sensibilidad se aplica **al mover** el slider (no al soltar) — más
  intuitivo. Se envía en el callback de cambio del `ttk.Scale`, deduplicado por valor entero.

---

## Verificación (host — lo que Claude SÍ puede cerrar)

- `pio run -e down` (R1 competencia), `-e down_robot2` (R2) y `-e down_debug_telemetry` →
  los **tres SUCCESS** (27.7 / 7.5 / 7.3 s).
- 59 tests C++ vía g++ (Avast bloquea `pio test` — TASK-025) + 82 pytest de la app + import GUI.

### Confirmación clave: ¿está "en producción" en los dos robots? — SÍ (por lectura de código)

Gustavo pidió "ponerlo en producción y asegurarse que está en todos los programas base, R1 y
R2". Se verificó leyendo el firmware (no la doc):

- **Boot ungated:** `comm_central_load_persisted_calib()` (en `main_down.cpp:117`, SIN gate de
  telemetría) lee la EEPROM v2 y puebla `g_dm.calib[].enabled/.sensitivity` + `g_dm.global_sens`
  → aplica en `down`, `down_robot2` Y `down_debug_telemetry`.
- **Detección ungated:** `dm_update()` (la verdad que viaja a CENTRAL) consume esas perillas
  cada tick, también sin gate.
- **Comandos + frame v3** están detrás de `#if defined(DOWN_DEBUG_TELEMETRY) || defined(DOWN_USB_MONITOR)`,
  y **`[env:down]` define `-DDOWN_USB_MONITOR`** (TASK-306, monitor dormido en el binario de
  partido), que `down_robot2` hereda → la calibración en vivo + persistencia ya funciona en los
  binarios de **partido** de ambos robots.

**Conclusión:** no había gap de firmware. "En producción y en los dos robots" ya era cierto
por diseño (defaults no-op + gate doble + boot ungated). No hubo que portar nada.

---

## Validado en HARDWARE REAL (testimonio de Gustavo — el equipo cierra hardware, no Claude)

- **R1, placa DOWN física, 2026-06-13:** la app conectada por USB; los **sliders de
  sensibilidad aplican AL MOVER** (tuning en vivo OK sobre la placa real).

## Pendiente de banco (NO cerrado — regla no negociable: hardware lo cierra el equipo)

Trasladado a **TASK-306** (ampliación 2026-06-13) como criterios de cierre:

1. **Persistencia tras power-cycle:** `CAL SAVE` con perillas → apagar/encender → confirmar que
   la sintonía sigue aplicada (no solo carpet/white).
2. **Deshabilitar saca de la línea:** que un sensor OFF cambie de verdad `LineStatusV2`
   (centroide/`cross_track`) hacia CENTRAL, no solo el dibujo.
3. **Saturación sobre placa real:** mín → todos blanco, máx → ninguno (en `--sim` no se ve).
4. (opcional) Repetir el set en R2.

---

## Docs

- **Corregido un error factual** en `docs/firmware/USO-MONITOREO-Y-TELEMETRIA.md`: decía que
  `down_debug_telemetry` "no compila / no se verificó con pio / único pendiente conocido" —
  acabo de compilarlo (SUCCESS). Actualizada la fila de troubleshooting y la sección 7.
- **Agregados los comandos v3** (`SENS GLOBAL/SET`, `SENSOR ON/OFF`) a la tabla de comandos de
  esa guía, con flag a **TASK-307** (la reconciliación del flujo canónico reflasheo-vs-en-vivo
  la decide Gustavo; no la toqué unilateralmente).
- `README` de la app y `TELEMETRIA-DOWN.md` ya estaban en v3 (sesión previa).

---

## Notas / riesgos

- En `--sim` los sliders/toggle no tienen efecto (el simulador no aplica sensibilidad ni recibe
  comandos); la saturación/efecto solo se ve sobre la placa real.
- La EEPROM subió v1→v2: una calib guardada vieja (v1) se rechaza limpio → recalibrar una vez.
- Sigue abierto el **conector USB flojo de la DOWN** (TASK-306, banco 2026-06-12) — si la app
  se desconecta sola en banco, es eso, no la app.
