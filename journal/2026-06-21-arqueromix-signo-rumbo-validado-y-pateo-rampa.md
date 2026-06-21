---
title: "arqueromix — signo de rumbo VALIDADO en banco (-1 default) + pateo con rampa como el delantero"
date: 2026-06-21
author: "Claude Opus 4.8 (1M context)"
requested-by: "Virginia Viollaz (banco)"
tipo: firmware-tuning
toca-competencia: NO (solo src/arqueromix/, build aislado)
---

# Sesión 2026-06-21 (banco, 4ª iteración) — cierre del rumbo + pateo con rampa

## Reporte de Virginia
1. La versión con la corrección de rumbo INVERTIDA (`-DARQMIX_FLIP_HEADING`, env `_flip`) fue la
   que **funcionó correctamente** (el arquero queda derecho, sin la vuelta de 180°). → guardar esa
   como la buena.
2. Ahora **encuentra bien la pelota pero no apunta como corresponde** al despejar → copiar la forma
   de pateo del DELANTERO (`centralmix`), que lo hace con una **rampa de aceleración**.

## Cambios
**(1) Signo de rumbo VALIDADO → default.** `AMIX_HEADING_CORRECT_SIGN` pasa a **-1 por default**
(el validado en banco). Se removió el env `_flip` (ya no hace falta; se flashea directo
`central_robot2_arqueromix`). Fallback al signo viejo: `-DARQMIX_HEADING_SIGN_OLD` (solo diagnóstico).

**(2) Pateo con RAMPA (port del delantero).** `avanzar_patear` del arquero pasó de PWM **fijo
asimétrico** (180/120 → veraba, "no apuntaba") a una **rampa SIMÉTRICA** igual a la del delantero
(`centralmix/mix_motors.cpp`): sube de 0 a `AMIX_KICK_VEL_FINAL`(180) de a `AMIX_KICK_PASO`(20) cada
`AMIX_KICK_INTERVALO_MS`(10 ms). Patrón `M1=+vel, M2=-vel, M3=0` = avance RECTO (por eso ahora
apunta). La rampa se resetea en `parar()`/init → cada despeje arranca de 0. Estado de la rampa
(`s_kick_vel/prev_ms/active`) igual que el delantero.

## Verificación
- **Compila SUCCESS** `central_robot2_arqueromix` (con el signo correcto y la rampa baked-in).
- Competencia byte-idéntica (solo `src/arqueromix/`, build aislado).
- ⚠️ El pateo con rampa NO está validado en HW todavía (compila ≠ anda) — lo prueba Virginia.

## Pendiente (Virginia / banco)
- Probar el despeje con rampa: ¿apunta/empuja la pelota recto ahora? Si no llega, subir
  `AMIX_KICK_VEL_FINAL` (180→210…). Si patea de costado, achicar `AMIX_TOL_KICK_DEG` (30→20).
- Con el arquero ya derecho (signo -1) + el pateo nuevo, validar el ciclo completo: patrulla →
  sigue la pelota → despeja → vuelve a patrullar. Es el criterio de cierre de TASK-114.

## Referencias
- `src/arqueromix/DOCUMENTACION.md §15` (cierre) + §13/§14 (cámara / 180°).
- Pateo con rampa del delantero: `src/centralmix/mix_motors.cpp::avanzar_patear`.
- Iteraciones previas del día: journals `2026-06-21-arqueromix-*` (port, fix cámara, inicio 180°).
