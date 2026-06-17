---
task: TASK-112
titulo: "Validar en banco la calibración por EEPROM sin reflashear (CENTRAL) y decidir promoción a competencia"
fecha-creada: 2026-06-17
creada-por: "Claude (sesión coach — Opus 4.8 1M)"
asignado: "equipo (banco con robot + USB)"
prioridad: P1
estado: ABIERTA (firmware + app listos host-tested; banco pendiente)
placas: CENTRAL
---

# TASK-112 — Validar la calibración por EEPROM

Se cableó end-to-end la calibración de potencias de la CENTRAL desde el monitor USB, sin
reflashear (gateado `-DCENTRAL_EEPROM_CALIB`). HOY aplican `min_pwm[3]` + `eff[3]` (el strafe
lateral, el síntoma del domingo). Falta validar en banco.

## Cómo probarlo

```bash
cd "software/teensy/Soccer 2026"
pio run -e central_robot2_arquero_calib -t upload     # R2 (o central_robot1_arquero_calib para R1)
python -m monitor_base                                  # vista "Calibrar CENTRAL"
```

## Checklist de banco

- [ ] Flashear `central_robot2_arquero_calib` (R2) / `central_robot1_arquero_calib` (R1).
- [ ] Abrir la app, vista **"Calibrar CENTRAL"**. Editar un piso (ej. piso del-izq 70→90),
      "Aplicar" → confirmar que el robot cambia el comportamiento del strafe EN VIVO (sin reflashear).
- [ ] "GUARDAR en EEPROM (SAVE)". **Power-cycle** del robot. Re-abrir la app → confirmar que el
      valor PERSISTE (sale de EEPROM al boot).
- [ ] Borrar/EEPROM en blanco (o robot nuevo) → confirmar que arranca con los `constexpr`
      (defaults) y se comporta como hoy (CRC falla → defaults).
- [ ] Calibrar el strafe lateral REAL: ajustar `min_pwm`/`eff` hasta que el robot strafe-e
      derecho y sin rotar (la "medialuna"/rotación parásita del banco anterior).
- [ ] **Decisión de promoción:** si funciona, agregar `-DCENTRAL_EEPROM_CALIB` a
      `central_robot1`/`central_robot2` (competencia) para que la calibración sirva en partido.
      Sin eso, el binario de partido ignora la EEPROM (usa constexpr).

## Notas

- `fwd_pwm` (avance recto) y `gyro_kp/ki/kd` (PID) ya se pueden editar/guardar en el panel,
  pero su EFECTO todavía NO está cableado (se guardan en EEPROM, listos). Su cableado al mixer
  de avance / `strategy.cpp` es el próximo incremento.
- El "Leer del robot (GET)" todavía no refresca con los valores reales (falta que el firmware
  emita el sub-objeto `ccfg` en el frame — próximo incremento). Por ahora el panel edita+guarda.

Journal: `journal/2026-06-17-calibracion-eeprom-sin-reflashear-cableada.md`.
