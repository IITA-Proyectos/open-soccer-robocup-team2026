---
task: 306
titulo: "App de monitoreo (monitor-base) confiable y lista para usar en Incheon"
fecha: 2026-06-12
asignado: equipo (Gustavo + María/Elías)
prioridad: P1
estado: pending
pedido-por: María Virginia Viollaz (banco 2026-06-12)
relacionada: TASK-305 (app PC monitoreo/calibración base), TASK-304 (telemetría DOWN)
---

# TASK-306 — App de monitoreo lista para Incheon

> **Por qué (pedido de María, banco 2026-06-12):** "esta app la tenemos que usar
> en Corea". Hoy en el banco NO se pudo usar la app para calibrar la línea — hubo
> que caer al calibrador de texto (`diag_down_calibracion`) — por los bugs de abajo.
> Para Incheon la app tiene que ser la herramienta confiable de monitoreo y
> calibración en la cancha.

## Bugs conocidos a resolver (de la auditoría 2026-06-11 + banco 2026-06-12)

1. **Calibrar por la app NO se aplica hasta reiniciar la placa** (el más grave —
   por esto hoy no se usó para calibrar). Los comandos CAL CARPET/WHITE/SAVE de la
   telemetría tocan solo la calib del `line_ring`, pero el `LineStatusV2` que se
   difunde sale del `DownModel` (`g_dm`), cuya calib NO se re-deriva → el alumno
   calibra, "ve" valores nuevos, pero `data_valid`/`line_present`/`cross_track`
   siguen con la calib vieja hasta el power-cycle. Fix propuesto:
   `comm_central_invalidate_calib()` (setea `g_dm_init=false` → re-deriva en el
   próximo send) llamado al final de cada caso CAL_* en `down_telemetry_serial.cpp`.
   (Ref: hallazgo DOWN de la auditoría 2026-06-11.)
2. **La app arranca en SIMULADOR por defecto sin avisar en pantalla** — un alumno
   que olvida `--port` ve sensores "perfectos" y puede creer que es el robot, o
   calibrar contra datos sintéticos. Fix: sufijar el título de la ventana con la
   fuente (' — SIMULADOR (sin robot)' / ' — COM12') o banner amarillo.
3. **CAL SAVE falla en silencio** (no chequea el retorno de `ec_save_calibration`,
   no avisa al host) y **CAL AUTO OFF aplica calib sin sanity-check** (puede
   persistir una calib inválida). Fix: ack/nak en el stream + rechazar
   min>=max / margen<umbral.

## Criterio de cierre (verificable en banco)

- [ ] Calibrar la línea **desde la app** (flashear `down_debug_telemetry`, abrir la
      app, capturar verde/blanco, guardar) y que el robot **detecte la línea sin
      reiniciar** la placa (verificar `data_valid`/`line_present`/`ev` en vivo).
- [ ] La app avisa claramente en pantalla cuándo está en **simulador** vs robot real.
- [ ] CAL SAVE confirma éxito/fallo; CAL AUTO OFF rechaza una calib inválida.
- [ ] Probado en banco con la placa DOWN real (no solo simulador).

## Contexto de hoy (banco 2026-06-12, María/R2)

Calibrando la línea para el arquero R2: la calib vieja no detectaba la línea de la
cancha (`ev=0x0` con el robot sobre la línea). Recalibramos con el calibrador de
texto + hoja blanca → 29/32 sensores buenos. La app habría sido más cómoda para ver
los 32 sensores gráficamente, pero por el bug #1 no se podía calibrar con ella.
Además se observó que **el conector USB de la placa DOWN parece flojo** (se
desconectó 2 veces durante la sesión) — revisar/resoldar antes de Incheon
(candidato a TASK de hardware aparte si persiste).

---

## Ampliación 2026-06-13 — Sintonía fina v3 (sensibilidad + habilitar/deshabilitar)

Se agregaron a la app + firmware DOWN 3 features de calibración fina (pedido de
Gustavo, banco 2026-06-13). Todo **aditivo y no-op por default** (todos los sensores
habilitados, sensibilidad 0 → umbral en el punto medio = competencia histórica
byte-idéntica). Ya están en `main` y compiladas en producción (ver más abajo).

1. **Habilitar/deshabilitar sensores** individualmente (click → inspector + botón, o
   doble-click en el anillo/barra). Un sensor OFF se excluye del cálculo de línea.
2. **Sensibilidad global al blanco** (`SENS GLOBAL`, −100…+100, extremos saturan) con
   barra de cercanía al umbral por sensor.
3. **Sensibilidad por-sensor** (`SENS SET`). `CAL SAVE` persiste todo en EEPROM v2.

### Estado de verificación (2026-06-13)

- ✅ **Host:** `pio run -e down` (R1 competencia), `-e down_robot2` (R2) y
  `-e down_debug_telemetry` → los **tres SUCCESS**. 59 tests C++ (g++) + 82 pytest app.
- ✅ **Firmware confirmado en producción de AMBOS robots** (lectura de código, no doc):
  el boot (`comm_central_load_persisted_calib`) carga la EEPROM v2 y puebla
  `g_dm.enabled/sensitivity/global_sens` **sin gate** → aplica en `down`/`down_robot2`,
  no solo en el env de telemetría. Los comandos SENS/SENSOR + frame v3 van detrás de
  `DOWN_DEBUG_TELEMETRY || DOWN_USB_MONITOR`, y `[env:down]` define `DOWN_USB_MONITOR`.
- ✅ **R1 placa REAL (Gustavo, banco 2026-06-13):** la app conectada a la DOWN física
  de robot1; los **sliders de sensibilidad aplican AL MOVER** (tuning en vivo por USB OK).

### Pendiente de banco (NO cerrado por Claude — regla: hardware lo cierra el equipo)

- [ ] **Persistencia tras power-cycle:** `CAL SAVE` con sensibilidad/sensores
      deshabilitados → apagar/encender la placa → confirmar que la sintonía SIGUE
      aplicada (no solo la carpet/white). Verificable mirando `threshold[]`/`enabled_bits`
      en la app tras el reboot.
- [ ] **Deshabilitar saca de la línea:** deshabilitar un sensor que ve blanco y
      confirmar que **cambia** `LineStatusV2` (centroide/`cross_track`/`sensors_on_line`)
      hacia CENTRAL — i.e. que el OFF surte efecto real en la detección, no solo en el dibujo.
- [ ] **Saturación sobre placa real:** en sensibilidad mínima detecta TODOS blanco, en
      máxima NINGUNO (en `--sim` no se ve; el sim no aplica sensibilidad).
- [ ] (opcional) Repetir el set en **R2** (`down_robot2`).
