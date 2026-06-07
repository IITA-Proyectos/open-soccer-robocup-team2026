# pc-udp-listener — Listener Python para el ESP32 Telemetry Bridge

Receptor UDP standalone que recibe el stream de JSON Lines del ESP32 y lo
vuelca a stdout (o a un archivo). Sirve para:
- **Diagnóstico inmediato**: confirmar que el ESP32 está reenviando datos.
- **Grabar sesiones**: pipe a un archivo y replay posterior.
- **Plantilla** para integrar UDP en `tools/monitor-base/` (que hoy es por
  serial).

> R&D 2027. Standalone, sin dependencias externas (Python stdlib).

## Requisitos

- Python 3.8+.
- Misma red WiFi que el ESP32.
- (Opcional) firewall permitiendo UDP en el puerto configurado (default 8765).

## Uso

```bash
# Escuchar broadcast en el puerto default (8765) y volcar a stdout
python udp_listener.py

# Especificar puerto y archivo de log
python udp_listener.py --port 8765 --log session.jsonl

# Filtrar solo líneas con "src":"down"
python udp_listener.py --filter '"src":"down"'

# Mandar un comando al ESP32 (en otra terminal)
python udp_send.py 192.168.1.42 8764 'CAL CARPET'
```

## Archivos

| Archivo | Qué hace |
|---|---|
| `udp_listener.py` | Recibe UDP en un puerto, vuelca a stdout y/o archivo, con filtro opcional. |
| `udp_send.py` | Manda una línea por UDP a un destino (para probar el path comando→ESP32→Teensy). |

## Próximos pasos (no urgentes)

- Agregar a `tools/monitor-base/monitor_base.py` un modo `--udp PORT` que
  use el mismo parser que el modo `--serial`. La idea es que la GUI no
  cambie, solo el origen de las líneas.
