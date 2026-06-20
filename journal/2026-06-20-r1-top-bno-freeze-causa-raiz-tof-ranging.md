# 2026-06-20 — R1 TOP: causa raíz del freeze del heading (BNO) = RANGEO de los ToF (acople eléctrico)

## Resumen ejecutivo
El heading de R1 (BNO055 **primario**, placa TOP, Wire2) se **congela**: el firmware reporta
"salud OK" pero el `hdg` no cambia al girar el robot. Sesión de banco completa (R1 conectado
por USB COM11; flash + captura de serial con pyserial). **Veredicto:**

> El chip BNO está **SANO**. Lo que cuelga la **fusión** del BNO es el **RANGEO de cualquier
> ToF** (los pulsos de corriente del láser VCSEL del VL53L7CX), por **acople ELÉCTRICO local
> en la placa TOP** (masa / 3V3 / EMI compartidos) — **NO** por el bus I2C (está aislado),
> **NO** por la batería (es compartida en la placa), **NO** por el chip / cristal / frecuencia
> de lectura / clock I2C.

Acción → revisar las **conexiones eléctricas BNO ↔ ToF** en la Zircon: [TASK-223](../team-tasks/2026-06-20-task-223-r1-bno-acople-electrico-tof-revisar-conexiones.md) (Enzo).
Actualiza el entendimiento de [TASK-207](../team-tasks/2026-06-08-task-207-bno-bus-i2c-aparte-wire2.md) (mover el BNO a Wire2 atacó la contención I2C, pero NO era esa la causa — es eléctrica).

## Contexto / corrección de una conclusión previa
La sesión anterior concluyó "el chip BNO de R1 está fallado, cambialo; solo anda en modo
magnetómetro". **Esa conclusión estaba MAL** y queda refutada acá: el chip lee y trackea
rotación perfectamente cuando los ToF NO rangean. Gustavo invirtió los dos BNO y el síntoma
siguió → ya apuntaba a software/eléctrico, no al chip. Confirmado.

## Método
- R1 TOP por USB (COM11, Teensy 4.0). Flash con PlatformIO; captura de serial NO interactiva
  con un script pyserial (`cap_serial.py`, temporal) — lee N segundos y vuelca a stdout.
- Convención del diag: heading IMUPLUS, primario en Wire2 (24/25), secundario en Wire (18/19),
  ambos @ 0x28, ToF en Wire enumerados a 0x2A–0x2D. Para discriminar se leyeron **registros
  crudos** del BNO (OPR_MODE, SYS_STATUS, SYS_ERR, EULER 0x1A, GYR_Z 0x18).

## Pruebas y veredictos (todas)
| # | Prueba | Condición | Resultado |
|---|---|---|---|
| 1 | `diag_bno_dual_live` (IMUPLUS, ToF dormidos) | giro 90° y mantener | ambos BNO trackean (~98°), concuerdan 0.6° → **chips OK** |
| 2 | idem | giro continuo | a veces trackea, a veces se congela → **freeze INTERMITENTE** |
| 3 | `diag_bno_freeze_probe` (registros crudos) | reposo | EUL constante (normal), gyro con ruido (vivo); SYS=5/ERR=0 |
| 4 | idem | giro | confirma: el EULER sigue el giro cuando NO está congelado |
| 5 | oscilador INTERNO (`setExtCrystalUse(false)`) | giro | **igual congela → NO es el cristal** |
| 6 | firmware competencia `top_robot1_pri_rt` (ToF activos, 100 Hz) | giro | `hdg=0.0` clavado (cámaras ven la pelota moverse) → **reproduce el síntoma real** |
| 7 | `top_robot1_pri` (BNO a 20 Hz) | giro | clavado → **NO es la frecuencia de lectura** |
| 8 | probe @ **100 kHz**, ToF dormidos | giro | **trackea** → NO es el clock I2C |
| 9 | **batería independiente** a la TOP | giro | clavado → **NO es la alimentación upstream** (BNO y ToF comparten 3V3/GND *en la placa*) |
| 10 | **bisección por LP** (despertar 1 ToF por vez, LP high, SIN rangear) | giro continuo | los 4 trackean → **encender un ToF no lo cuelga** |
| 11 | **rangeo 1 ToF por vez** (`-DTOF_ONLY_INDEX=N`) | giro | **ToF#0 → CONGELA. ToF#1 → CONGELA.** (#2/#3 no completados; ver abajo) |
| 12 | **scan I2C dual-bus** (Wire vs Wire2) | ToF despiertos | ToF (0x29/0x2C) **solo en Wire**; Wire2 solo 0x28 → **buses AISLADOS, no puenteados** |

### Evidencia clave (registros crudos, prueba 11, solo ToF#0 rangeando, girando):
```
hdg=0.0  ... min_obst=545..1128   (min_obst varía = ToF#0 rangeando + robot girando; hdg CLAVADO)
```
### Scan dual-bus (prueba 12):
```
ToF dormidos:    Wire= 0x28, 0x2C       Wire2= 0x28
ToF despiertos:  Wire= 0x28, 0x29, 0x2C  Wire2= 0x28      <- 0x29/0x2C NUNCA en Wire2 => aislados
```

## Causa raíz
**El rangeo de cualquier ToF cuelga la fusión del BNO por acople eléctrico local.**
- Encender un ToF (LP high) NO lo cuelga; **rangear SÍ** → es la actividad del VCSEL (pulsos de
  corriente fuertes), no el simple consumo en reposo.
- Basta **UN** ToF rangeando (probado #0 y #1). No es la suma de los 4, ni un cable puntual.
- Los buses I2C están **aislados** (scan) → el ruido NO viaja por el bus. Y la batería
  independiente no cambió nada → no es upstream. ⇒ El acople es **local en la placa TOP**:
  masa compartida (ground bounce de la corriente pulsada del VCSEL), o ruido en el 3V3, o EMI
  radiada hacia el BNO/su cristal. El chip sigue ackeando I2C (por eso "salud OK") pero su
  motor de fusión se clava.

## Para confirmar el mecanismo fino (pendiente, banco)
Osciloscopio en **3V3 y GND del BNO** mientras un ToF rangea → buscar caída/ruido/ground-bounce
sincronizado con el rangeo. Eso define si el fix va por desacoplo, por masa (star-ground) o por
EMI/blindaje.

## Hallazgos secundarios
- **El BNO secundario (0x28 en Wire) volvió a ackear** tras el repaso de soldaduras de Gustavo
  (antes NAKeaba = conexión abierta). Esa soldadura quedó resuelta.
- Un ToF retiene su dirección enumerada (0x2C visto aún "dormido") mientras tiene 3V3 — recordar
  power-cycle al re-enumerar.

## Pendiente de esta sesión (honesto)
- Rangeo por-ToF: se completaron **#0 y #1** (ambos congelan). **#2 y #3 no se corrieron**
  (se pivoteó a la hipótesis de bus). Dado que los buses están aislados y dos ToF distintos ya
  lo cuelgan, se infiere "cualquier ToF", pero #2/#3 quedan para cerrar formalmente.
- Mecanismo fino (masa vs supply vs EMI): falta el osciloscopio.

## Herramientas creadas (reusables, quedan en el repo)
- `src/diag/diag_bno_freeze_probe.cpp` + env `diag_bno_freeze_probe` — **scanner I2C dual-bus**
  (test de aislamiento Wire vs Wire2). (El archivo evolucionó durante la sesión: probe de
  registros → bisección por LP → scanner; quedó como scanner.)
- `-DTOF_ONLY_INDEX=N` (gate en `sensors_tof.cpp`) — inicializa/rangea **solo el ToF N**
  (bisección por-ToF). Default sin el flag = competencia byte-idéntica.
- `-DDIAG_NO_TOF` (gate en `main_top.cpp`) — corre el firmware completo con los ToF apagados.
  Default sin el flag = competencia byte-idéntica.

## Nota de honestidad
Durante la sesión me corregí varias veces (dije "chips OK", luego "congelado", luego refiné):
es un fallo **intermitente** y cada captura agarró un estado distinto. Las hipótesis afiladas
de Gustavo (frecuencia de lectura, un ToF con cable malo, buses puenteados) se probaron una por
una y se descartaron con datos — ese descarte ordenado es lo que dejó la causa eléctrica en pie.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
