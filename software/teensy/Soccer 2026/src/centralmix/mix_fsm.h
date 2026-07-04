// mix_fsm.h — Máquina de estados de centralmix (port del delantero 2025).
//
// CONTRATO. Port 1:1 de la FSM del delantero 2025
// (delantero-sin-zirconLib.cpp), pero LEYENDO g_io (mix_io.h, variables planas
// pobladas por mix_comm desde TOP/DOWN) en vez de las globales seriales crudas, y
// ACTUANDO con las primitivas directas de mix_motors.h. NO usa world_model.
//
// FIDELIDAD vs 2025: el enum era 1:1 con el delantero 2025 (delantero-sin-zirconLib.cpp
// líneas 130-139). DESVÍOS 2026 (pedido Elías, marcados, no silenciados):
//   - Se QUITÓ AVANCE_INICIO (el avance de 700 ms al arranque) — 2026-06-21.
//   - Se AGREGÓ KICKOFF_SEEK como PRIMER estado (arranque del partido).
// (PRIMER_IMPULSO_INICIAL_GIRANDO sigue en el enum por fidelidad 2025 pero NO tiene case
// en el switch — estado declarado-pero-muerto, igual que el 2025.)
//
// ⚠️ NO TESTEADO EN HARDWARE.

#pragma once

namespace iitasoccer {
namespace mix {

// Estados del delantero (base 2025). El arranque es KICKOFF_SEEK (abajo); AVANCE_INICIO
// se quitó 2026-06-21. El resto conserva el orden/nombre del enum 2025.
enum class Estado {
    PRIMER_IMPULSO_INICIAL_GIRANDO,   // declarado en 2025 pero SIN case en el switch (muerto)
    IMPULSO_INICIAL_GIRANDO,
    GIRANDO,
    APUNTAR_PELOTA,
    AVANZANDO,
    CENTRANDO_horario,
    IMPULSO_CENTRANDO_antihorario,
    CENTRANDO_antihorario,
    IMPULSO_CENTRANDO_horario,
    APUNTAR_PELOTA_antihorario,
    APUNTAR_PELOTA_horario,
    PATEANDO_corto_pausa_inicial,
    PATEANDO_corto_adelante,
    PATEANDO_corto_pausa,
    PATEANDO_corto_atras,
    PATEANDO_pausa_inicial,
    PATEANDO_adelante,
    PATEANDO_pausa,
    PATEANDO_atras,
    AVANZANDO_POR_TIEMPO,
    DETECTA_LINEA_1,
    DETECTA_LINEA_2,
    DETECTA_LINEA_3,
    // --- AGREGADO 2026 (coach); redefinido 2026-07-03: PRIMER estado = PATADA de saque. ---
    // Patea recto (avanzar_patear, SOLO avanza) → después IMPULSO_INICIAL_GIRANDO.
    // Se RE-ARMA en CADA START del árbitro (flanco STOP→GO), no solo la primera vez.
    KICKOFF_SEEK,
    ESPERAR,
    TEST,
    // --- AGREGADO 2026 (rama ultrasonido): anti-choque. Si el ultrasonido (montado ALTO, no ve la
    //     pelota) detecta algo a <15 cm, la FSM interrumpe, RETROCEDE y vuelve a BUSCAR (GIRANDO).
    //     Ver mix_config.h: MIX_OBSTACULO_STOP_MM / MIX_EVITAR_MS.
    EVITAR_OBSTACULO,
    KICKOFF_SEEK1,
    KICKOFF_SEEK2,
};

// Inicializa la FSM: estado = KICKOFF_SEEK (primer estado), sella timers (millis()). Llamar
// en setup() DESPUÉS de mix_comm_init() y mix_motors_init().
void mix_fsm_init();

// Avanza un paso de la FSM: lee g_io, ejecuta el case del estado actual con las
// primitivas de mix_motors, y transiciona. NO toca Serial (eso es de mix_comm).
// Llamar en cada loop() DESPUÉS de mix_comm_tick().
// ⚠️ Sugerencia de contrato (no estaba en el 2025): si g_io.match_running == false,
// mix_fsm_tick() debe llamar parar() y NO mover el robot (arranque por árbitro RCJ).
void mix_fsm_tick();

}  // namespace mix
}  // namespace iitasoccer
