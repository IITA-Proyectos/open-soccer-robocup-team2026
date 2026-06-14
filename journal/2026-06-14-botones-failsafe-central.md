# 2026-06-14 — Fail-safe: botón físico de arranque DESHABILITADO por default (CENTRAL)

**Pedido (Gustavo):** desactivar el botón físico de arranque en TODOS los programas de la
CENTRAL (producción + el de testeo del arquero). En los DOS robots el pulsador onboard del
Zircon (pin 9) dio problemas — se sospecha lógica/polaridad invertida respecto a lo que el
programa lee, y el 2026-06-12 quedó CLAVADO → GO permanente. Política: ir a fail-safe; si hace
falta arrancar, se usa el **árbitro** o **comandos por serie**.

## Qué se hizo (cambio de DEFAULT, no por-env)

En vez de agregar `-DCENTRAL_MANUAL_START_NO_BUTTON` a cada env (frágil, se olvida en programas
futuros), se **invirtió el default en el código**: el botón físico ahora se lee SOLO si se
compila con **`-DCENTRAL_ENABLE_PHYSICAL_BUTTON`** (opt-in explícito que **ningún env define**).

- `src/central/main_central.cpp`: el `pinMode(pin9)` y el `if (digitalRead(PIN_MANUAL_START_BUTTON)==LOW) cmd_go=true`
  pasaron de `#ifndef CENTRAL_MANUAL_START_NO_BUTTON` a `#ifdef CENTRAL_ENABLE_PHYSICAL_BUTTON`.
- El teclado serie `g`/`s`/ENTER (juez desde la PC) y el árbitro (pines 5/6 del TOP → world_model)
  **NO se tocaron** — siguen siendo el control válido.
- `CENTRAL_MANUAL_START_NO_BUTTON` quedó **muerto** (solo en comentarios + flag redundante en los
  envs `*_nobtn`, que ahora son redundantes — se conservan por compatibilidad de nombres).
- Comentarios de `config_central.h` y `platformio.ini` actualizados.

**Efecto por tipo de env:**
- **Competencia** (`central_robot1`, `central_robot2`, `_wdt`…): **byte-idéntico** — el bloque del
  botón está dentro de `CENTRAL_ENABLE_MANUAL_START`, que esos envs no definen (ni compilaban el botón).
- **Banco/arquero/práctica** (los que sí tienen `CENTRAL_ENABLE_MANUAL_START`): el binario CAMBIA
  (el botón ya no se lee); GO/STOP queda por teclado serie / árbitro.

## Verificación (workflow: 4 compiladores + 1 auditor)

- **Los 21 envs `central_robot*` compilan** SUCCESS (Teensy 4.1) — el cambio no rompió nada
  (un `PIN_MANUAL_START_BUTTON` "sin usar" en los envs con MANUAL_START no es error: es constexpr).
- **Auditor `fail_safe_ok`:** botón OFF en el firmware de juego, serie intacto, árbitro intacto,
  flag viejo muerto, **0 envs** definen `CENTRAL_ENABLE_PHYSICAL_BUTTON`.
- **Pendiente del equipo (regla hardware):** confirmar en banco que el arquero arranca/para por
  `g`/`s` (y por el árbitro) y que el botón clavado YA NO lo hace patrullar solo.

## Fuera de scope (reportado, NO tocado)

Tienen su propio botón en sketches APARTE (no el firmware de juego):
- `diag_central_brake`, `diag_central_strafe`, `diag_central_line_sweep`, `diag_central_drive_straight`:
  botón pin 9 **con fallback por serie (ENTER)** → se podrían desactivar si Gustavo quiere.
- `diag_central_motors`: botón **SIN fallback serie** (arranca solo por botón) → habría que
  agregarle control por serie ANTES de poder desactivarlo.
- `src/main.cpp` (legacy `teensy41_legacy`, robot 2025): botón propio; no se toca (legacy).
