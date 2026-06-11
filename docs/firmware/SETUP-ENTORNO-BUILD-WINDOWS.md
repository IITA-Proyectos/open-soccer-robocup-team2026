---
title: "Setup de entorno de build/test en Windows (para verificar compilación y correr TDD)"
date: 2026-05-18
author: "Claude (Anthropic - Claude Opus 4.7 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.7 1M, Anthropic)"
status: final
tags: [build, tooling, windows, setup, firmware]
robot: ambos
area: comunicacion
tipo: tutorial
related-tasks: [TASK-023]
---

# Setup de entorno de build/test en Windows

> **Por qué.** La sesión Claude corre en un shell sin compilador C++ y sin red
> para PlatformIO. PlatformIO **cachea** plataformas/toolchains/libs en
> `C:\Users\violl\.platformio`. Si vos corrés los comandos **una vez con
> internet** en una terminal normal, esa caché queda poblada y después Claude
> puede compilar/testear **offline** desde su sesión.

> **⚠️ Actualización 2026-05-18 — la causa raíz NO es "falta de red".**
> Diagnóstico confirmado: en estas máquinas SÍ hay red (git y WebFetch
> funcionan). PlatformIO falla con `HTTPClientError` porque **Avast hace
> MITM/SSL-scanning** y reemplaza los certificados HTTPS por los suyos, que no
> tienen endpoint de revocación comprobable → la pila TLS de Windows
> (schannel, que PlatformIO 6.1.x usa vía `truststore`) corta el handshake
> con `CRYPT_E_NO_REVOCATION_CHECK`. Prueba: `curl …` falla error 35;
> `curl --ssl-no-revoke …` → HTTP 200.
>
> **Fix mejor que el cache offline** (este doc sigue siendo útil para
> cachear, pero con la excepción de Avast PlatformIO funciona CON red, sin
> depender de pre-cachear todo): agregar `*platformio.org*` a las excepciones
> de Avast. **Procedimiento completo en `team-tasks/TASK-025`.** Sin esto,
> apenas se necesite una lib NO cacheada (p.ej. al activar OTOS/ToF de
> TASK-023), el cache offline no alcanza.

Hay dos objetivos:
- **Mínimo:** que se pueda verificar que los 3 programas Teensy **compilan**
  (`pio run -e down/-e top/-e central_robot1`). Solo necesita PlatformIO +
  plataforma Teensy cacheada. **No** necesita compilador C++ host.
- **Completo (recomendado):** correr el **TDD host** (`pio test -e
  test_native`). Necesita además un compilador C++ host (MinGW g++) + la
  plataforma `native` + Unity cacheadas.

---

## Paso 1 — PlatformIO Core

Ya hay PlatformIO 6.1.19 instalado por pip en el entorno de Claude. En **tu**
terminal, confirmá/instalá:

```bat
python -m pip install -U platformio
python -m platformio --version
```

Desactivá auto-update y telemetría (evita el `HTTPClientError` en runs offline):

```bat
python -m platformio settings set enable_telemetry No
python -m platformio settings set check_platformio_interval 999999
python -m platformio settings set check_platforms_interval 999999
python -m platformio settings set check_libraries_interval 999999
python -m platformio settings set enable_prompts No
```

## Paso 2 — Compilador C++ host (MinGW-w64) — para el TDD host

Necesario solo para `pio test -e test_native`. Opción más simple (sin gestor de
paquetes): **WinLibs**.

1. Ir a https://winlibs.com/ → descargar el zip **GCC ... UCRT runtime**
   (64-bit, "Win64", versión release reciente, p.ej. GCC 14.x).
2. Descomprimir en `C:\mingw64` (debe quedar `C:\mingw64\bin\g++.exe`).
3. Agregar `C:\mingw64\bin` al **PATH** del usuario:
   - Inicio → "Editar las variables de entorno del sistema" → Variables de
     entorno → en "Variables de usuario" editar `Path` → Nuevo →
     `C:\mingw64\bin` → Aceptar.
4. **Abrir una terminal NUEVA** y verificar:
   ```bat
   g++ --version
   gcc --version
   ```
   Debe imprimir la versión (no "no se reconoce").

> Alternativa con gestor de paquetes (MSYS2): instalar MSYS2 desde
> https://www.msys2.org/ , luego `pacman -S mingw-w64-ucrt-x86_64-gcc` y
> agregar `C:\msys64\ucrt64\bin` al PATH. WinLibs es más directo.

## Paso 3 — Pre-cachear PlatformIO (UNA vez, con internet)

En tu terminal, en la carpeta del proyecto Teensy:

```bat
cd "C:\Users\violl\iitasoccer\soccer-main\software\teensy\Soccer 2026"
```

> ⛔ **OJO (fix 2026-06-11):** este paso decía `C:\Users\violl\futbol2026\...` — ese
> directorio es un clon VIEJO/divergente ("señuelo", ver HANDOFF-NUEVA-SESION.md).
> Compilar o flashear desde ahí = firmware desactualizado SIN ningún error visible.
> El repo real es **`C:\Users\violl\iitasoccer\soccer-main`**.

**3a. Cachear plataforma Teensy + verificar que compila DOWN:**
```bat
python -m platformio run -e down
```
Esto baja la plataforma `teensy` + toolchain ARM (varios cientos de MB, una
vez) y compila el binario DOWN. Si termina en `SUCCESS`, DOWN compila.

Repetir para las otras placas (verifica que compilan):
```bat
python -m platformio run -e top
python -m platformio run -e central_robot1
python -m platformio run -e central_robot2
```
> Si `top` o `down` fallan por `lib_deps` (OTOS/ToF) → es el tema de TASK-023;
> para el **Plan 1 (line-sensing core)** no se necesitan esas libs. Reportame
> el error exacto si aparece.

**3b. Cachear plataforma native + Unity (para el TDD host):**
```bat
python -m platformio test -e test_native -f test_proto
```
Baja la plataforma `native` + framework Unity (una vez) y corre la suite
`test_proto` que ya existe. Si pasa, el TDD host funciona.

## Paso 4 — Confirmar que quedó offline-capable

Cerrá internet (o no importa) y, en tu terminal, repetí:
```bat
python -m platformio run -e down
python -m platformio test -e test_native -f test_proto
```
Si ambos funcionan **sin red**, la caché en `C:\Users\violl\.platformio` está
lista y Claude podrá usarla desde su sesión.

## Paso 5 — Avisarle a Claude

Decile a Claude: *"entorno listo"*. Claude correrá desde su sesión:
- `python -m platformio run -e down` → verifica compilación.
- `python -m platformio test -e test_native -f <suite>` → corre el TDD real
  (rojo→verde) tarea por tarea del plan DOWN.

## Troubleshooting

| Síntoma | Causa | Fix |
|---|---|---|
| `HTTPClientError` al instalar/resolver libs o plataformas | **Avast MITM SSL** (no es falta de red) — ver callout arriba | Paso 1 mitiga (menos llamadas al registry); **fix real: excepción Avast, TASK-025**. Si persiste, pre-cachear con internet |
| `g++: command not found` en `test_native` | MinGW no en PATH o terminal vieja | Paso 2.4: terminal NUEVA tras editar PATH |
| `pio run -e down` falla con lib OTOS/ToF | `lib_deps` sin resolver (TASK-023) | No bloquea Plan 1; reportar error textual |
| Descargas lentísimas | Primera vez baja toolchain ARM (~grande) | Es una sola vez; queda cacheado |

## Notas

- Mínimo para "que compilen los 3 programas": **Paso 1 + Paso 3a**. No hace
  falta MinGW para esto.
- Para el **TDD del Plan DOWN** (recomendado): además **Paso 2 + Paso 3b**.
- La caché vive en `C:\Users\violl\.platformio` (compartida entre tu terminal
  y la sesión de Claude porque es el mismo usuario Windows).
