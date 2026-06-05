# `_deprecated-2025/` — programas del robot 2025 (NO ejecutables hoy)

> ⚠️ **HISTÓRICO. NO compilar ni flashear con el hardware/firmware actual (2026).**
> Estos son los programas del robot del **Nacional 2025** (arquitectura mono-placa con `zirconLib`).
> Son **incompatibles de raíz** con el sistema 2026 (3 placas Teensy + cámaras OpenMV N6). Se
> conservan solo como **referencia histórica**. El firmware VIVO está en
> `software/teensy/Soccer 2026/src/`.

## Por qué son incompatibles (de raíz)

| Programa | Incompatibilidad |
|---|---|
| `robot-delantero/definitivo-delantero.cpp` | usa `zirconLib` (no compila), **kicker físico** (el robot 2026 no tiene), parser de cámara **9 B v1** (sin CRC/END), BNO local en el Teensy de motores, sin `MOTOR_INVERT`, HW Zircon Mark1/Naveen1 ausente |
| `robot-delantero/delantero-sin-zirconLib.cpp` | mono-placa 2025, lee la línea por `analogRead` directo, cámara 9 B v1, kicker |
| `robot-arquero/definitivo-arquero_6-9-2026` | ídem (zirconLib, BNO local, kicker) |
| `libraries/zirconLib/zirconLib.{cpp,h}` | SDK 2025; **`zirconLib.cpp` ni siquiera compila** (llave colgante); API de pines del Zircon viejo |
| `vision/enviar coordenadas 2 arcos y pelota` | script OpenMV **v1** (9 B, X sin offset, sin CRC), `pyb.LED` (crashea en la N6) |

## El equivalente VIVO (2026)
- **Estrategia/decisión** → `src/central/strategy.cpp` (FSM dual, sin kicker, con PIDs y clamp).
- **Cámaras** → `hardware/electronics/camera{Front,Back}-pack/firmware/openmv/cam-*-n6.py` (contrato **v2**, 11 B, CRC8+END).
- **Lo que valía la pena de estos programas ya se analizó y portó** (heurísticas seguras):
  ver `research/in-progress/2026-06-04-estrategias-anteriores-plan-port.md`.

> Lo que SÍ sirve como lección (no como código): "ganaron 2025 con poco y robusto, sin heading ni IMU"
> → el **modo degradado** importa. Pero el código no se ejecuta.
