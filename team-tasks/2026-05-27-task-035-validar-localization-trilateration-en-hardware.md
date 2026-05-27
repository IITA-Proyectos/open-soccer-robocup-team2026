---
id: TASK-035
title: "Validar Sprint 1 localizacion (trilateracion) en hardware real"
date_created: 2026-05-27
date_due: 2026-06-15
assigned: [Virginia o Elias]
priority: P1
status: pending
estimated_hours: 1
blocks: [scope-firmware-pose-incheon]
blocked_by: [TASK-033]
tags: [hardware-test, top-board, localizacion, sprint1]
---

# TASK-035 — Validar Sprint 1 localizacion en hardware

## Prerequisitos

- Bodge XSHUT terminado (TASK-033 done).
- Placa TOP funcionando con 4 TOFs enumerados (verificar con
  `pio device monitor` corriendo `diag_top_tof_adafruit` que cada
  TOF entrega lecturas distintas al mover objetos por delante).

## Procedimiento

1. Llevar el robot a la cancha real (lab IITA).
2. Con el robot apagado, ponerlo en el centro de la cancha apuntando
   al arco rival (lado +Y por convencion).
3. Encender el robot.
4. Flashear el diag:
   ```
   pio run -t clean -e diag_localization_live
   pio run -e diag_localization_live -t upload
   pio device monitor -b 115200
   ```
5. Confirmar en el monitor: banner aparece, "BNO offset = X deg",
   "init OK".

## Paso 0: Verificar signo del BNO055 (PREREQUISITO)

**Antes de validar la trilateración**, hay que confirmar que el BNO055
da heading positivo cuando el robot gira CCW (antihorario visto desde
arriba). El código de `src/shared/localization.cpp` asume esa convención
— si el BNO da CW positivo (lo más común en IMUs consumer), la
trilateración va a salir reflejada y NINGÚN criterio de aceptación va a
pasar.

### Cómo verificar

1. Con el robot encendido y `diag_localization_live` corriendo, ANOTAR
   el `BNO offset = X deg` que aparece en el banner del setup.
2. **Sin apagar el robot**, levantarlo y rotarlo manualmente 90° a la
   DERECHA (sentido horario visto desde arriba) — el robot ahora apunta
   hacia donde estaba el lateral derecho.
3. Mirar el `hdg=X` printeado en el monitor durante el siguiente ciclo.
4. Si `hdg` es **negativo** (~-90° = -9000 centideg) → ✅ CCW positivo,
   convención correcta. Continuar con los 4 criterios de abajo.
5. Si `hdg` es **positivo** (~+90° = +9000 centideg) → ❌ CW positivo,
   convención invertida. Avisar al coach para fix de 1 línea en
   `src/shared/localization.cpp` (invertir el signo del heading en el
   cálculo de `heading_deg = (in.bno_heading_centideg - cfg.bno_offset_centideg) / 100;`,
   o re-mapear `cfg.tof_mount_angle_deg[]` desde `{0, 180, 90, 270}` a
   `{0, 180, 270, 90}` para compensar).

### Si el signo está mal y no se puede esperar al coach

Workaround inmediato (NO mergear a main sin discusión):
```cpp
// En localization_compute (src/shared/localization.cpp), reemplazar:
int16_t heading_deg = (in.bno_heading_centideg - cfg.bno_offset_centideg) / 100;
// por:
int16_t heading_deg = -(in.bno_heading_centideg - cfg.bno_offset_centideg) / 100;
```

Después de cualquier fix, **volver a correr los 4 criterios** abajo.

## Criterios de aceptacion (los 4 que pasan = TASK done)

- [ ] **Centro de cancha (1215, 910)**: pose printeada cada 500 ms
      muestra `x=1215±30`, `y=910±30`, `hdg=0±2°`.
- [ ] **Esquina propia (0, 0)**: mover robot ahi → pose `x=0±30`, `y=0±30`.
- [ ] **Esquina rival (2430, 1820)**: pose `x=2430±30`, `y=1820±30`.
- [ ] **Rotacion 90° derecha en centro**: pose `x=1215±30`, `y=910±30`,
      `hdg=90±2°`.

## Si algun criterio falla

- Pose incorrecto pero consistente: revisar `BNO offset` printeado al
  init. Si no es 0 cuando el robot apunta al arco rival, hay error
  de orientacion al encender. Power cycle con orientacion correcta.
- Pose `INVALID` la mayoria del tiempo: revisar I2C scan con
  `diag_top_tof_adafruit` que los 4 TOFs responden.
- Pose salta entre ciclos: el filtro temporal de Sprint 2 lo va a
  resolver. Para Sprint 1, anotar y seguir.

## Cierre

Pegar logs del monitor + medicion con metro en el journal:
`journal/2026-XX-XX-validacion-hardware-localization-sprint1.md`.
Actualizar este TASK con status=done.
