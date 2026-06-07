# r-d-2027/code — Código stub de los subproyectos

> ⚠️ **NO ES CÓDIGO DE COMPETENCIA.** Stubs y prototipos del subproyecto R&D
> 2027. Compilan/corren standalone; **no se mezclan** con
> `software/teensy/Soccer 2026/`.

## Contenido

| Carpeta | Qué es | Compila / corre como |
|---|---|---|
| `esp32-bridge-firmware/` | Firmware completo del ESP32 puente UART↔WiFi (FASE A). | PlatformIO standalone (Arduino-ESP32 core). |
| `teensy-glue-snippet/` | Snippet de ejemplo de cómo cablearlo al firmware de telemetría existente del Teensy. **NO se aplica al firmware actual sin decisión del coach.** | Snippet de referencia, NO se compila tal cual. |
| `pc-udp-listener/` | Listener Python standalone que recibe el stream UDP y lo vuelca a stdout o a un archivo. | Python 3.8+, `python -m udp_listener` (sin deps externas). |

## Reglas

1. **Cada subcarpeta es un proyecto independiente** con su propio `README.md`
   y sus propias dependencias. No importa código de `software/teensy/Soccer 2026/`.
2. **WiFi credentials / MAC peers** NO se commitean. Cada subcarpeta tiene un
   `*.example.*` con la plantilla; el archivo real está en `.gitignore`.
3. **NO ejecutar en partido oficial.** Solo banco/entrenamiento.

## Cómo arrancar (orden recomendado)

1. Leer `r-d-2027/decisions/2026-06-07-esp32-telemetry-bridge.md` (entender el "por qué").
2. Leer `r-d-2027/specs/esp32-telemetry-bridge-v0-spec.md` (entender el "cómo").
3. **Sin hardware**: correr el listener Python (`pc-udp-listener/`) y mandarle
   datos sintéticos con `nc -u` o un script de mock para verificar parser.
4. **Con un ESP32**: copiar `esp32-bridge-firmware/include/config.example.h`
   a `config.h`, llenar WiFi y compilar/flashear.
5. **Con Teensy + ESP32**: ver `teensy-glue-snippet/` y discutir con el coach
   si se aplica al firmware real.
