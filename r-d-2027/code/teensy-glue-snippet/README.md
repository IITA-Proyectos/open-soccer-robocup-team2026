# teensy-glue-snippet — Ejemplo de cómo conectar el ESP32 al firmware Teensy

> ⚠️ Snippet de **referencia**. **NO se aplica al firmware actual** sin decisión
> explícita del coach. Vive en `r-d-2027/` para que no se mezcle con el código
> de competencia.

## Para qué sirve

Mostrar cómo se integraría el ESP32 Telemetry Bridge al firmware existente de
TOP/CENTRAL/DOWN **sin romper nada**. La regla del proyecto es: con el flag
de competencia (sin `-DENABLE_TELEMETRY_ESP32`) el binario es **byte-idéntico**
al actual.

## Archivo

- `central_telemetry_esp32_glue.example.cpp` — ejemplo basado en CENTRAL.
  El mismo patrón aplica a TOP y DOWN cambiando el `Serial2` por el UART
  libre que corresponda en cada placa.

## Cómo se aplicaría (cuando se apruebe)

1. **platformio.ini** — agregar un env nuevo:
   ```ini
   [env:central_robot1_telem_esp32]
   extends = env:central_robot1
   build_flags =
       ${env:central_robot1.build_flags}
       -DCENTRAL_DEBUG_TELEMETRY
       -DENABLE_TELEMETRY_ESP32
   ```
   El env `central_robot1` (de competencia) **no se toca** — sigue compilando
   el binario byte-idéntico.

2. **Glue** en el archivo de telemetría de CENTRAL: en el lugar donde hoy
   `telemetry_emit_line(line)` hace `Serial.print(line)` (USB CDC), agregar
   `Serial2.print(line)` adicional gateado por `#ifdef ENABLE_TELEMETRY_ESP32`.

3. **Loop**: leer `Serial2.read()` hasta `\n` y pasarle la línea al parser de
   comandos existente (el mismo `td_parse_command`/`tt_parse_command` que ya
   procesa los comandos del USB).

4. **Cableado físico** (ver
   `r-d-2027/specs/esp32-telemetry-bridge-v0-spec.md` §3.2):
   - GND, 3V3 (o Vin), TX, RX entre los 4 pines del UART libre del Teensy
     y los pines configurados en el `config.h` del ESP32.

## Lo que NO hay que hacer

- ❌ Reemplazar el `Serial.print` del USB por el `Serial2.print` — **agregar**,
  no reemplazar. La telemetría USB sigue funcionando idéntica.
- ❌ Mover la lógica pura de `telemetry_*.cpp` a otro lado.
- ❌ Cambiar el schema JSON Lines. El ESP32 es **transparente** al schema.
- ❌ Aplicarlo al env de competencia sin gateado.
- ❌ Mandar mensajes críticos (START/STOP, motor cmd) por este canal.
