// amix_comm.h — capa de comunicación de arqueromix: TOP/DOWN → variables planas.
//
// ÚNICO punto que toca Serial. Espejo de centralmix/mix_comm.h, con UNA diferencia de
// diseño (pedido de Virginia): el HEADING viene del SNAPSHOT del TOP (por serie), NO de
// un BNO local del CENTRAL. Las placas CENTRAL 2026 no traen BNO propio (el rumbo se
// procesa en el TOP), así que esto es lo correcto y además más simple (sin Wire/BNO).
//
// ⚠️ NO TESTEADO EN HARDWARE.

#pragma once

namespace iitasoccer {
namespace arqmix {

// Inicializa los enlaces TOP (Serial7) y DOWN (Serial1) a 230400 + colchón RX.
void amix_comm_init();

// Drena ambos enlaces, decodifica (proto.h) y POBLA g_aio. Llamar en cada loop().
void amix_comm_tick();

}  // namespace arqmix
}  // namespace iitasoccer
