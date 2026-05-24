# `.captures/` — datos crudos del `scripts/diag_capture.py`

> **🟢 Datos del 2026-05-24 son VÁLIDOS** (carpet.json / blanco.json / negro.json).
> Generados durante el hardware-up del anillo de línea, con el firmware
> `diag_down` corregido (pinout validado empíricamente). Verdict del test:
> 0 sensores muertos, 9 OK, 22 SOSPECHOSO (los SOSPECHOSO responden bien
> físicamente, solo no llegan al umbral 300 del script calibrado para cancha
> real RoboCup).
>
> ⚠️ **Los valores absolutos NO son representativos de cancha real**: el
> test se hizo sobre una **mesa con vidrio** (superficie reflectante). En
> cancha verde RoboCup los valores absolutos van a ser menores y el rango
> blanco-negro probablemente más limpio. Hay que recalibrar umbrales una
> vez que se monte el robot en cancha real.
>
> Ver `journal/2026-05-24-hardware-up-down-anillo-linea.md` para análisis
> completo + datos de las 4 iteraciones (pre-batería, post-batería con
> firmware viejo, post-Fix #1, post-Fix #2).

## Cómo regenerar (cualquier sesión)

```powershell
cd "C:\Users\violl\iitasoccer\open-soccer-robocup-team2026\software\teensy\Soccer 2026"
# Compilar + flashear el diag (apretar el botón de la Teensy cuando aparezca el loader):
& "C:\Users\violl\AppData\Roaming\Python\Python314\Scripts\pio.exe" run -e diag_down -t upload

# Las 3 capturas (con el Serial Monitor CERRADO):
python scripts\diag_capture.py --port COM10 --label carpet --duration 2   # robot sobre superficie ambiente
python scripts\diag_capture.py --port COM10 --label blanco --duration 2   # sobre cartulina blanca
python scripts\diag_capture.py --port COM10 --label negro  --duration 2   # sobre fondo negro o al aire

# Veredicto automático:
python scripts\diag_capture.py --verdict
```

## Referencias

- Hardware-up validado: `journal/2026-05-24-hardware-up-down-anillo-linea.md`
- Pinout canónico: `hardware/electronics/down-board-pack/01-pinout-y-posiciones.md`
- Postmortem original (caso cerrado): `journal/2026-05-19-diagnostico-down-fallido-config-tentativo.md`
- TASK seguimiento: `team-tasks/2026-05-19-task-026-confirmar-pinout-mux-down.md` (validated-empirically, P2)
