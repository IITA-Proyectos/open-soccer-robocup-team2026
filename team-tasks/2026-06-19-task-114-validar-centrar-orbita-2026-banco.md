# TASK-114 — Validar en banco el candidato de órbita de `centrar_*` (centralmix R1)

- **Asignado:** Elías
- **Placa / rango:** CENTRAL (100-199)
- **Prioridad:** P2 — mejora de comportamiento de `centralmix` (que es una prueba
  paralela). Detrás de un flag, APAGADO por defecto → cero riesgo para lo que anda hoy.
- **Estado:** PENDIENTE (banco). **Solo Elías/equipo lo cierra** (regla de HW).
- **Depende de:** [TASK-113](2026-06-19-task-113-validar-centralmix-banco.md) (validación general de centralmix).

## Contexto (qué se hizo y por qué)

En `centralmix`, las primitivas `centrar_horario()/centrar_antihorario()` (port 2025)
**orbitan mal en la geometría de ruedas 2026**: decodifican a **casi puro strafe**
(vx≈±64, ω·R≈±8) → el robot se desliza de costado y **la pelota se le escapa** en vez
de orbitarla. (El FSM espera que la pelota quede centrada durante la órbita: salta a
`APUNTAR_PELOTA_*` si `|anguloPelota| ≥ 15`.)

Se preparó un **candidato rebalanceado a rotación-dominante** que mantiene la misma
dirección de órbita (horario = strafe-IZQ + giro CW) y supera los pisos físicos de PWM
{70,70,107}. Verificado por análisis de cinemática (decode contra `src/shared/kinematics.cpp`;
workflow 2026-06-19, 6 agentes, 0 refutaciones fatales). Queda **detrás del flag
`MIX_CENTRAR_ORBIT_2026`, apagado por defecto** — se habilita solo para esta prueba.

- Código: `src/centralmix/mix_motors.cpp` (`centrar_horario/antihorario`, gated).
- Tunables: `src/centralmix/mix_config.h` → `MIX_CENTRAR_FRONT` (=80), `MIX_CENTRAR_REAR` (=170).
- Flag: `platformio.ini` env `central_robot1_mix` (línea `build_flags` comentada).

## Cómo probar

1. En `platformio.ini`, env `[env:central_robot1_mix]`, **descomentar**:
   `build_flags = ${env:central_robot1.build_flags} -DMIX_CENTRAR_ORBIT_2026`
2. `pio run -e central_robot1_mix -t upload`
3. **MEDIR** primero `d` = distancia centro-del-robot ↔ pelota cuando está "capturada"
   (apoyada al frente). Es el dato que fija el ratio de órbita ideal.
4. Observar la órbita con la pelota:
   - Si la pelota **deriva hacia ADENTRO** del giro → sobra rotación → **bajar `MIX_CENTRAR_REAR`**.
   - Si la pelota **se va de COSTADO** (afuera) → falta rotación → **subir `MIX_CENTRAR_REAR`**.
   - Confirmar el SENTIDO físico (horario/antihorario, izq/der) de R1. Si orbita al
     revés: intercambiar las etiquetas `horario`↔`antihorario` en el FSM, **NO** tocar
     los signos en `mix_motors.cpp` (es el `+180` de traslación R1 marcado "A VERIFICAR").
5. **Vigilar TEMPERATURA de la trasera (M3):** el default `REAR=170` ya está algo sobre
   el techo térmico (~150 PWM sostenido; motores brushed 5V@7.4V se queman >~70%). Usar
   **órbitas cortas** y tacto térmico. Si calienta, bajar `REAR` hacia 150 (acepta más
   sobre-rotación) o sumar dribbler.

## tema-a-analizar

- **risk-no-fix:** `centrar_*` sigue siendo casi-puro-strafe → la fase de centrado/órbita
  de `centralmix` no retiene la pelota → no se llega a patear bien al arco. (Solo afecta
  a `centralmix`, que es la rama de prueba; el stack actual no cambia.)
- **risk-fix:** el candidato **sobre-rota** un poco (ratio ≈1.83 vs ideal ≈1.0) porque el
  techo térmico impide el ratio ideal sin quemar la trasera (conflicto de 3 vías: piso
  delantero 70 vs ratio R/d vs techo 150). La corrección `APUNTAR_PELOTA` del FSM recoge
  la deriva residual, pero la órbita puede salir "a tirones". Riesgo térmico real en la
  trasera si la órbita se alarga.
- **tiempo:** ~30-45 min de banco (medir `d`, 2-3 iteraciones de `MIX_CENTRAR_REAR`, chequeo térmico).

## Criterio de cierre

Elías valida en banco que, con el flag ON y `MIX_CENTRAR_FRONT/REAR` ajustados, el robot
**orbita la pelota manteniéndola centrada** sin recalentar la trasera, y **DA EL OK**.
Recién con ese OK se decide si se vuelve default (sacar el flag o invertirlo).
Mientras tanto: **flag OFF por defecto**, comportamiento actual intacto.

## Temas relacionados que quedaron abiertos (no tocados acá)

- `impulso_centrando_horario/antihorario` (en `mix_fsm.cpp`) tienen **el mismo defecto**
  (casi puro strafe) y además su **emparejamiento nombre↔dirección parece cruzado**
  respecto de los `centrar_*` que preceden. Si el candidato de `centrar_*` se aprueba,
  conviene rebalancearlos igual (mismo flag) y revisar ese emparejamiento.
