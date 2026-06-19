// mix_fsm.h — Máquina de estados de centralmix (port del delantero 2025).
//
// CONTRATO. Port 1:1 de la FSM del delantero 2025
// (delantero-sin-zirconLib.cpp), pero LEYENDO g_io (mix_io.h, variables planas
// pobladas por mix_comm desde TOP/DOWN) en vez de las globales seriales crudas, y
// ACTUANDO con las primitivas directas de mix_motors.h. NO usa world_model.
//
// ⚠️ DISCREPANCIA DE CONTEO (marcada, no silenciada): la tarea pide "28 estados".
// El enum REAL del 2025 (la fuente que manda, delantero-sin-zirconLib.cpp líneas
// 130-139) declara 24 estados nombrados — los 24 que están abajo, verbatim. NO hay
// 28: no inventé 4 estados para llegar al número. Si el equipo quiere 4 estados
// extra (p.ej. ESPERA_INICIO/STOP por match_running, o separar PRIMER_IMPULSO de
// IMPULSO), se agregan como decisión explícita; este contrato refleja el 2025 tal
// cual. (PRIMER_IMPULSO_INICIAL_GIRANDO está en el enum 2025 pero NO tiene case en
// el switch — es un estado declarado-pero-muerto; lo conservo por fidelidad.)
//
// ⚠️ NO TESTEADO EN HARDWARE.

#pragma once

namespace iitasoccer {
namespace mix {

// Los 24 estados del delantero 2025, en el MISMO orden del enum original.
enum class Estado {
    AVANCE_INICIO,
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
};

// Inicializa la FSM: estado = AVANCE_INICIO, sella los timers (millis()). Llamar en
// setup() DESPUÉS de mix_comm_init() y mix_motors_init().
void mix_fsm_init();

// Avanza un paso de la FSM: lee g_io, ejecuta el case del estado actual con las
// primitivas de mix_motors, y transiciona. NO toca Serial (eso es de mix_comm).
// Llamar en cada loop() DESPUÉS de mix_comm_tick().
// ⚠️ Sugerencia de contrato (no estaba en el 2025): si g_io.match_running == false,
// mix_fsm_tick() debe llamar parar() y NO mover el robot (arranque por árbitro RCJ).
void mix_fsm_tick();

}  // namespace mix
}  // namespace iitasoccer
