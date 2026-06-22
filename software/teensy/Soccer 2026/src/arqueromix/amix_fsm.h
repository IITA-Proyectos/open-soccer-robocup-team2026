// amix_fsm.h — Máquina de estados del ARQUERO 2025 portada a arqueromix.
//
// Port FIEL del ciclo ARQUERO del firmware unificado 2025 (definitivo-arquero_6-9-2026,
// L1016-1205). Son **15 estados** (el enum de abajo; +`esperar_quieto`, +`inicio_lateral_izq`, +`orientar_frente` del modo quieto): el bloque DELANTERO del 2025 NO se porta acá
// (eso es centralmix). El estado INICIAL ya NO es `impulso_inicial` (2025) sino `inicio_retroceder`
// (homing al área, agregado 2026-06-21 banco Virginia). Estados nuevos 2026: inicio_retroceder/
// inicio_avanzar (homing), salir_linea_der/izq (rebote a ciegas), ALINEAR_arco_opp (apuntar al arco rival).
// Ver docs/internal/ANALISIS-FIEL-ARQUERO-2025.md §2 para el flujo exacto.
//
// QUÉ CAMBIA respecto del 2025 (y SOLO esto):
//   - Toda lectura de sensor → g_aio (amix_io.h), poblado por amix_comm (TOP/DOWN serie).
//   - Toda escritura de motor → primitivas de amix_motors.h.
//   - Se AGREGA el gate del árbitro RCJ (si !match_running → parar) al inicio del tick.
//   - Se AGREGA un timeout de SEGURIDAD al retroceso (el 2025 no tenía: podía colgarse).
//
// ⚠️ NO TESTEADO EN HARDWARE. Compila != anda: el sentido de cada primitiva, el signo
// lateral de la pelota y los umbrales en mm se validan/re-tunean en banco.

#pragma once
#include <stdint.h>

namespace iitasoccer {
namespace arqmix {

// Estados REALES del arquero 2025 (nombres = los del flujo, FIEL §2).
// INICIO modificado 2026-06-21 (banco Virginia): en vez de patrullar directo, el arquero hace
// HOMING al área chica — retrocede hasta ver la línea del área, avanza un poco a ciegas, y recién
// ahí patrulla. Así arranca siempre posicionado en su arco.
enum class Estado : uint8_t {
    inicio_lateral_izq,          // MODO QUIETO: estado INICIAL — moverse un poco a la IZQUIERDA, luego → homing.
    inicio_retroceder,           // estado INICIAL (patrulla) / 2º en quieto: ir hacia atrás hasta detectar la línea del área
    inicio_avanzar,              // tras ver la línea: avanzar un poco SIN leer los sensores
    esperar_quieto,              // MODO QUIETO (-DARQMIX_QUIETO): parado esperando la pelota. SOLO parar /
                                 // seguir pelota lateral / patear. NO usa moverce_*/rebote/profundidad.
    moverce_derecha,             // patrulla strafe derecha (adproporcional)
    moverce_izquierda,           // patrulla strafe izquierda (aiproporcional)
    salir_linea_der,             // tocó línea IZQ → sale a la DERECHA a ciegas (sin sensores) y patrulla der
    salir_linea_izq,             // tocó línea DER → sale a la IZQUIERDA a ciegas (sin sensores) y patrulla izq
    PATEANDO_pausa_inicial,      // despeje: pausa 200 ms (deja pasar la inercia)
    ALINEAR_arco_opp,            // despeje: GIRA para apuntar el frente al ARCO RIVAL (goal_opp) antes de patear
    PATEANDO_adelante,           // despeje: golpe de avance 450 ms (avanzar_patear) hacia donde quedó apuntando
    PATEANDO_pausa,              // despeje: pausa 1000 ms
    PATEANDO_atras,              // despeje: retroceso recto hasta ver línea (+ safety)
    avanzar_despues_de_patear,   // reposicionamiento 1000 ms → vuelve a patrullar (o a orientar_frente en quieto)
    orientar_frente,             // MODO QUIETO: tras separarse de la línea, gira para MIRAR AL FRENTE → esperar_quieto
};

void amix_fsm_init();
void amix_fsm_tick();

}  // namespace arqmix
}  // namespace iitasoccer
