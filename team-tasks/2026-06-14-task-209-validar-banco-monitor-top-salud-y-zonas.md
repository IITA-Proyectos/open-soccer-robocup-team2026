# TASK-209 — Validar en banco: monitor TOP "salud" + zonas de ToF por telemetría

- **Asignado:** Virginia / Elías (equipo humano — requiere placa TOP)
- **Prioridad:** P1 (operativo para Incheon; no bloqueante)
- **Estado:** ✅ **CERRADA** (banco 2026-06-14, Gustavo).
- **Creada:** 2026-06-14 (sesión Claude autónoma — journal
  `journal/2026-06-14-monitor-top-salud-y-zonas-telemetria.md`)
- **Depende de:** firmware `top_robot2_pri` re-flasheado con el campo `z` (commit de
  esta sesión). El monitor viaja DORMIDO en el binario de competencia.

## ✅ Resultado de banco 2026-06-14 (Gustavo)

- **Monitor `--top-salud` ANDA en la placa TOP real**: conecta, el firmware arranca a
  mandar (handshake STREAM ON/PING), se ve dato real de sensores y zonas.
- **Bug PARPADEO encontrado y arreglado** (la ventana se redimensionaba cada frame;
  `pack_propagate` + ancho fijo del header; commit `b42e220` + test de regresión).
- **Regresión snapshot→CENTRAL: OK** — `diag_central_rx_all` mostró el WorldSnapshot del
  TOP a la CENTRAL **66 Hz, 0 CRC, 0 seqGap**, decodificado entero (el monitor de solo
  lectura NO degrada el envío a CENTRAL).
- Sub-checks finos NO reportados uno por uno (tapar un sensor → rojo; botones de config;
  pelota fantasma; cable-pull → modo partido). **Quedan como verificación opcional**, no
  bloquean. La grilla de zonas se vio con dato real (no "pendiente firmware").

Journal del banco: `journal/2026-06-14-banco-monitor-top-validado-y-top-central-ok.md`.

## Contexto

Se agregó (host-testeado, NO validado en placa):
1. App PC `python -m monitor_base --top-salud` — tablero de salud por sensor +
   grilla de zonas de ToF + botones de config.
2. Firmware: el stream ahora emite las **zonas crudas 4×4** por ToF (campo `z`,
   aditivo). Antes solo viajaba 1 distancia por sensor.

`pio run -e top_robot2_pri` = SUCCESS y el golden host coincide, pero **eso NO
prueba que el dato real fluya por USB**. Eso lo cierra esta TASK.

## Procedimiento

```powershell
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
pio run -e top_robot2_pri -t upload          # re-flashear la TOP (trae el monitor dormido + zonas)
# (la TOP tarda ~40 s en bootear por los ToF; conectar la batería)
cd tools\monitor-base
python -m monitor_base --top-salud --port auto    # o --port COMx
```

## Criterio de cierre (medible)

- [ ] La app **conecta** y el firmware **arranca a mandar** al conectar (handshake
      STREAM ON/PING) — el tablero deja de decir "esperando datos".
- [ ] **Salud por sensor correcta**: tapar/desconectar un sensor lo pone en ROJO/GRIS
      (ToF tapado → SIN DATO; cámara desconectada → FALLA; etc.).
- [ ] **Zonas reales**: la grilla 4×4 de cada ToF muestra distancias que **cambian**
      al acercar la mano a una parte del campo de visión (no "pendiente firmware",
      no todo gris). Confirma que el campo `z` fluye de verdad.
- [ ] **Botones de config** efectivos: `TOF n OFF` apaga ese sensor (se va a SIN DATO),
      `CFG SAVE` persiste (sobrevive power-cycle), `CFG LOAD/RESET` andan.
- [ ] **Pelota fantasma**: con ambas cámaras viendo objetos naranjas distintos, el
      tile "Pelota (fusión)" se pone en REVISAR (delta > 200 mm).
- [ ] Al **sacar el cable**, el robot vuelve solo a modo partido (no queda en debug).

## Regresión a chequear (vecinos)

- [ ] El loop del TOP **sigue a ~20-25 Hz** con las zonas en el stream (el frame pasó
      de ~950 B a ~1350 B; medir Δloop en el panel `[TOP]` y `resync` de cámaras).
- [ ] El **WorldSnapshot a la CENTRAL** no se degrada (el arquero/delantero siguen
      andando con la TOP en modo monitor — el monitor es de SOLO LECTURA).

## Si algo falla

- Zonas todo gris pero distancia OK → revisar `fill_zones`/`sensors_tof_get_zone_mm`
  (status de zona 5/6/9) en `sensors_tof.cpp`.
- Frame truncado / parser se queja → el buffer del glue es 2048; si con 6 ToF a
  futuro no entra, subirlo.
