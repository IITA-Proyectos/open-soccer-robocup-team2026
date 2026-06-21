# arqueromix — arquero 2025 sobre datos de TOP/DOWN (prueba aislada)

Hermano ARQUERO de `centralmix`. Es el **arquero 2025** (FSM + manejo directo de motores)
alimentado por los datos que mandan **TOP y DOWN** por serie, **sin tocar `src/central/`**.

- **Heading**: del **snapshot del TOP** (no BNO local). `-DARQMIX_HEADING_OTOS` usa el OTOS.
- **Línea/piso**: del **DOWN** (LineStatusV2).
- **Pelota/arcos**: del **snapshot del TOP**.
- **Motores**: PWM directo (`analogWrite`/`digitalWrite`), pines Zircon 2026.

## Compilar / flashear (Virginia juega arquero con R2)

```bash
cd "software/teensy/Soccer 2026"
pio run -e central_robot2_arqueromix            # compilar
pio run -e central_robot2_arqueromix -t upload  # flashear la CENTRAL de R2
# TOP y DOWN NO cambian: top_robot2_pri + down_robot2
```

Volver al arquero de competencia: `pio run -e central_robot2_arquero -t upload`.

## ⚠️ Estado

**COMPILA · NO validado en banco.** Compila ≠ anda. Antes de confiar: verificar el sentido
de cada motor (ruedas al aire), el signo lateral de la pelota, y re-tunear los umbrales
(pasaron de píxeles a mm). Todo el detalle en `DOCUMENTACION.md` §7 y en la TASK-114.

Arranque (banco): el árbitro (`match_running`) o, si se habilita el juez por serie, `g`=GO / `s`=STOP.
