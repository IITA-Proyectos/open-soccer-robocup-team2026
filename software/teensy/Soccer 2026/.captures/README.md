# `.captures/` — datos crudos del `scripts/diag_capture.py`

> ⚠️ **Los datos del 2026-05-19 son INVÁLIDOS.** El firmware `diag_down` que
> los generó usaba el `config_down.h` con pinout **tentativo, no confirmado**
> contra el PCB real → el código toggleaba pines del Teensy que probablemente
> no controlan los muxes reales. Los valores 1023 saturados en S16–S31 son
> lecturas de pines al aire, no de sensores muertos.
>
> Enzo confirmó que físicamente los 32 sensores andan. El bug es del firmware
> (config), no del hardware.
>
> Mantener estos archivos como evidencia (postmortem) hasta que se complete
> **TASK-026** (confirmar pinout real). Cuando se actualice `config_down.h`
> con los pines correctos, **borrar las capturas viejas** y volver a correr
> el test masivo.

## Cómo regenerar (post TASK-026)

```powershell
cd "C:\Users\violl\iitasoccer\open-soccer-robocup-team2026\software\teensy\Soccer 2026"
& "C:\Users\violl\.platformio\penv\Scripts\python.exe" scripts\diag_capture.py --port COM10 --label carpet --duration 2
# ... poner hoja blanca ...
& "C:\Users\violl\.platformio\penv\Scripts\python.exe" scripts\diag_capture.py --port COM10 --label blanco --duration 2
# ... poner algo oscuro ...
& "C:\Users\violl\.platformio\penv\Scripts\python.exe" scripts\diag_capture.py --port COM10 --label negro  --duration 2
& "C:\Users\violl\.platformio\penv\Scripts\python.exe" scripts\diag_capture.py --verdict
```

## Referencias

- Postmortem: `journal/2026-05-19-diagnostico-down-fallido-config-tentativo.md`
- TASK bloqueante: `team-tasks/2026-05-19-task-026-confirmar-pinout-mux-down.md`
