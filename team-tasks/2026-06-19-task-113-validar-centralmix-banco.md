# TASK-113 — Validar en banco el centralmix (port delantero 2025 sobre TOP/DOWN)

- **Placa:** CENTRAL (R1, delantero)
- **Asignado:** equipo (banco) — Gustavo / Elías
- **Prioridad:** P2 (es una PRUEBA paralela; el stack actual `src/central/` no se toca ni se reemplaza hasta que esto se valide)
- **Estado:** abierta
- **Build:** `pio run -e central_robot1_mix -t upload` (compila OK 2026-06-19). Escape: cualquier env de competencia de siempre.

## Por qué
Versión experimental: FSM + manejo directo de motores del delantero 2025, alimentado por
datos de TOP/DOWN (`src/centralmix/`, sin world_model). Objetivo: debuggear con la FSM
conocida. Compila pero **compilar ≠ anda**. Ver journal 2026-06-19-centralmix-port-delantero-2025.

## Cómo validar (en orden — cada paso desbloquea el siguiente)
1. **Heading source primero** (decisión + verificación): confirmar si la placa CENTRAL de R1
   tiene BNO local (Wire@0x28) o si hay que usar el heading del snapshot del TOP. Si no hay BNO
   local → cambiar `mix_comm` a heading de snapshot, o flashear con `-DMIX_HEADING_OTOS`.
   (El BNO del TOP de R1 estaba muerto el 2026-06-19 AM → se cambia a la tarde.)
2. **Primitivas de motor, una por una, SIN FSM** (robot en soporte, ruedas al aire): verificar
   que `avanzar` va al frente, `girar` gira, `centrar_horario/antihorario` orbitan en el sentido
   correcto, `avanzar_patear`/`retroceder_patear` patean. ⚠️ El 2025 usaba OTRO mapeo de pines →
   alguna primitiva puede salir invertida o lateral. Corregir signos por rueda en `mix_motors.cpp`.
3. **Comm:** confirmar que `g_io` se puebla (pelota x/y, arcos, heading, línea, match_running)
   con datos reales de TOP/DOWN (telemetría/serial). Sin esto la FSM ve todo en cero.
4. **FSM completa, robot en piso:** buscar→apuntar→avanzar→orbitar→patear. Re-tunear umbrales
   que pasaron de píxeles 2025 a mm/grados 2026: `MIX_TOL_CERCANIA` (cercanía pelota),
   `MIX_TOL_CENTRADO` (alineación con arco), sector de línea (±30°).
5. **Arco rival:** confirmar a qué arco ataca R1 (default AMARILLO; `-DMIX_ATTACK_BLUE` invierte).

## Criterio de cierre
- Las primitivas de motor van en el sentido correcto.
- `g_io` refleja TOP/DOWN en vivo.
- El delantero hace el ciclo completo y patea al arco correcto, con umbrales re-tuneados.
- **Decisión:** si anda → se promueve como alternativa de delantero; si no → se descarta y se
  sigue con `src/central/` (sin costo, build aislado).

## Escape / rollback
Flashear cualquier env de competencia de la CENTRAL (`central_robot1` / `central_robot1_delantero_practica`). El centralmix no toca nada de eso.
