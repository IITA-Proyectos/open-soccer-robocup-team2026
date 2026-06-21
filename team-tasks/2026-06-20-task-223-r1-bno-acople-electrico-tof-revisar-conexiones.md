# TASK-223 — R1 TOP: revisar conexiones eléctricas BNO ↔ ToF (el rangeo de los ToF cuelga el BNO)

- **Asignado:** Enzo (revisión de conexiones/eléctrica en la Zircon)
- **Placa / rango:** TOP (200-299)
- **Prioridad:** **P1** (heading del delantero; hay mitigación temporal con OTOS — ver abajo).
- **Estado:** ⚠️ **PROBABLE PISTA FALSA — cerrar (corrección 2026-06-21).** La causa real del
  "heading no anda" era un flag de config (`bno_left_en=0` en EEPROM), NO acople eléctrico de
  los ToF. Con el fix (primario forzado habilitado) el BNO da heading vivo **con los ToF
  rangeando** (banco 2026-06-21: hdg −15.6 en reposo → 101.4 girado). Ver
  [journal 2026-06-21](../journal/2026-06-21-bno-heading-fix-config-flag-no-era-tof.md). NO
  invertir tiempo de hardware en esto salvo que reaparezca un freeze independiente del flag.
- **Relacionada:** [TASK-207](2026-06-08-task-207-bno-bus-i2c-aparte-wire2.md) (mover BNO a Wire2: atacó la contención I2C, pero la causa real es ELÉCTRICA, no el bus).
- **Diagnóstico completo:** [journal 2026-06-20](../journal/2026-06-20-r1-top-bno-freeze-causa-raiz-tof-ranging.md).

## Qué pasa (resultado de banco)
El heading del BNO primario de R1 (TOP) **se congela cuando los ToF RANGEAN**. Probado por
bisección: con **un solo ToF rangeando** ya se cuelga (ToF#0 y #1 confirmados). Descartado, con
datos, que sea: el chip, el cristal, la frecuencia de lectura, el clock I2C, los buses puenteados
(scan: AISLADOS), o la batería (independiente no cambió nada). → **Acople ELÉCTRICO local en la
placa** entre la corriente pulsada del láser (VCSEL) de los ToF y el BNO (masa / 3V3 / EMI).

## Qué revisar (conexiones) — esto es lo tuyo, Enzo
1. **Masa (GND) del BNO vs ToF:** que la corriente pulsada del VCSEL **NO pase por el retorno de
   masa del BNO**. Buscar masa compartida / un solo punto de retorno largo. Idealmente
   **star-ground**: retorno de los ToF separado del retorno del BNO hasta el punto común.
2. **3V3 del BNO:** ¿comparte rail/traza/conector con los ToF? Agregar **desacoplo local fuerte
   pegado al VDD del BNO** (cerámico 100 nF + bulk 10 µF). Evaluar **LDO o ferrite/RC propio**
   para el BNO, aislado del rail de los ToF.
3. **Cableado/bodge de los ToF:** revisar que ningún cable de los 4 ToF corra pegado a las líneas
   del BNO (acople capacitivo/EMI). (La hipótesis de "un ToF con cable en corto" se descartó:
   dos ToF distintos, cada uno, cuelgan el BNO — no es uno puntual.)
4. **Secundario:** el 2º BNO (0x28 en Wire) volvía a ackear tras tu repaso de soldaduras (antes
   NAKeaba). Confirmar que quedó firme.

## Cómo reproducir / verificar después del fix
1. Flashear `top_robot1_pri_rt` (competencia). `pio device monitor -b 115200` (o capturar USB).
2. Girar el robot ~90° ida y vuelta y mirar `[TOP] ... hdg=...`:
   - **Clavado en 0.0 girando = sigue el bug.**
   - **`hdg` sigue el giro = ARREGLADO.**
3. Bisección si hace falta: `pio run -e top_robot1_pri_rt -t upload` con
   `PLATFORMIO_BUILD_FLAGS="-DTOF_ONLY_INDEX=N"` (N=0..3) corre solo ese ToF.
4. Aislamiento de buses: env `diag_bno_freeze_probe` (scanner I2C dual-bus).

## tema-a-analizar
- **risk-no-fix:** el heading por BNO de R1 es inusable con los ToF activos (= siempre, en
  cancha) → el delantero juega sin rumbo del BNO.
- **risk-fix:** retrabajo eléctrico en la Zircon (masa/desacoplo/LDO). Bajo si es solo agregar
  caps/separar masa; medio si hay que rerutear.
- **tiempo:** ~1-3 h de banco (osciloscopio en 3V3/GND del BNO mientras un ToF rangea para ver
  el bounce/ruido, + probar el desacoplo/masa).

## Mitigación temporal para Incheon (firmware, si el HW no llega)
Usar el **heading del OTOS** (placa DOWN) en vez del BNO cuando los ToF están activos — el OTOS
no sufre este acople. O bajar fuerte la frecuencia de rangeo de los ToF y medir si el acople baja
del umbral. (No es el fix de raíz, pero da rumbo en cancha.)

## Criterio de cierre
Con los ToF rangeando (firmware de competencia), el `hdg` **sigue el giro** de forma estable
(sin clavarse) durante varios minutos. Lo valida el equipo en banco.
