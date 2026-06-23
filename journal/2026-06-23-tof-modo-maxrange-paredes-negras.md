---
title: "ToF modo MAX-RANGE — paredes NEGRAS de cancha (4×4 + 2 Hz + continuo) para medir alcance"
date: 2026-06-23
status: vivo (modo de banco; medir hasta qué distancia detecta la pared negra)
placa: TOP (ROBOT2)
env: top_robot2_pri_tofmaxrange (banco) — competencia top_robot2_pri BYTE-IDÉNTICA
autor: "Claude Opus 4.8 (1M context), vía Claude Code — pedido Virginia"
testeado-en-hardware: NO (compila; lo cierra el equipo)
---

# Por qué el ToF no ve las paredes: son NEGRAS (no es el montaje)

## El dato que cambió el diagnóstico (banco Virginia)

El piloto 8×8 mostró que los ToF leen el PISO (cerca) pero NO las paredes (0,9-1,2 m). Primera
hipótesis: montaje (sensor mirando al piso). **Descartada por Virginia en banco:** inclinó el robot
arriba/abajo lentamente y movió la posición — **la pared no aparece en ninguna**. Si fuera geometría,
alguna zona pegaría. Dato clave: **las paredes son de madera pintada de NEGRO** (por reglamento RCJ,
contraste para las cámaras).

**Causa raíz = reflectancia IR.** El VL53L7CX mide tiempo de vuelo de un VCSEL infrarrojo; el **negro
absorbe el IR** → muy poco retorno → señal por debajo del umbral → `target_status` inválido → la zona
se descarta (sin lectura). El piso (alfombra clara, cerca) sí devuelve. Es un problema de
SEÑAL/alcance-con-target-oscuro, no de que el sensor "no llegue" (el rango está en 4 m y vio 1896 mm).

## Modo MAX-RANGE (gateado, para MEDIR hasta dónde detecta el negro)

Env `top_robot2_pri_tofmaxrange` = `top_robot2_pri` + `-DTOP_TOF_MAXRANGE -DTOP_ENABLE_TOF_CONTINUOUS`.
Configura los ToF para **máxima detección de targets oscuros**:
- **4×4 (16 zonas)** en vez de 8×8: cada zona integra ~4× más luz → más alcance por zona con negro.
- **Ranging a 2 Hz** (`TOF_RANGING_FREQ_HZ`): mucho más **tiempo de integración por medición** → más
  fotones acumulados → más alcance con baja reflectancia. (Es la perilla principal contra el negro.)
- **Modo CONTINUO** (`TOP_ENABLE_TOF_CONTINUOUS`): VCSEL siempre on → más señal/inmunidad que el autónomo.
- **`TOF_STALE_TIMEOUT_MS` 250 → 2000 ms** (sensors_tof.h, gateado): a 2 Hz un frame tarda 500 ms; con
  250 ms cada lectura expiraría antes del siguiente frame y se vería vacía.

Todo bajo `#ifdef` → **competencia byte-idéntica** (verificado: `top_robot2_pri` md5 del `.hex` =
`a36e934e…`, igual al baseline).

## Verificación

- `pio run -e top_robot2_pri_tofmaxrange` SUCCESS + `top_robot2_pri` SUCCESS y **byte-idéntico** (md5).
- NO testeado en hardware (regla #1).

## Plan de banco

1. Flashear `top_robot2_pri_tofmaxrange` en la TOP.
2. Abrir `python -m monitor_base --tof-setup` (vista 4×4 con distancia por zona).
3. Pared negra de frente al ToF, **alejar el robot de a poco** y anotar **hasta qué distancia la zona
   sigue mostrando la pared** (deja de verla = límite de alcance con negro). Probar los 4 ToF.
4. Si a 2 Hz + continuo el alcance con negro llega a ~0,9-1,2 m (medio ancho/largo de cancha desde el
   centro) → se puede localizar por paredes. Si no llega → evaluar bajar aún más la frecuencia (1 Hz),
   o que la localización use solo las paredes CERCANAS (cuando el robot está cerca de un lateral).

## Comando de flasheo

```
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"; pio run -e top_robot2_pri_tofmaxrange -t upload
```
Volver a competencia: `pio run -e top_robot2_pri -t upload`.
