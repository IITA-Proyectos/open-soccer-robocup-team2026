// amix_fsm.h — Máquina de estados del ARQUERO (MODO QUIETO) portada a arqueromix.
//
// 2026-06-29 (decisión equipo): se ELIMINÓ el modo PATRULLA — el arquero es SÓLO QUIETO.
// El enum de abajo ya NO tiene moverce_*/salir_linea_*/avanzar_despues_de_patear (eran de
// patrulla). El texto histórico debajo puede mencionarlos como referencia del port 2025.
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
    esperar_quieto,              // parado esperando la pelota. SOLO parar / seguir pelota lateral / patear.
    PATEANDO_pausa_inicial,      // despeje: pausa 200 ms (deja pasar la inercia)
    ALINEAR_arco_opp,            // despeje: GIRA para apuntar el frente al ARCO RIVAL (goal_opp) antes de patear
    PATEANDO_adelante,           // despeje: golpe de avance 450 ms (avanzar_patear) hacia donde quedó apuntando
    frenar_patada,               // MODO QUIETO: si detecta LÍNEA pateando → contra-empuje fuerte atrás (mata el impulso, no salirse) → pausa
    PATEANDO_pausa,              // despeje: pausa 1000 ms
    PATEANDO_atras,              // despeje: retroceso recto hasta ver línea (+ safety)
    orientar_frente,             // tras patear, gira (cámara/giroscopio, heading→0) a MIRAR al arco rival → atrás
    acomodar_linea,              // ANTES de quedar quieto — si toca línea (lateral/atrás), despegarse al lado opuesto
    acomodar_orientar,           // ANTES de quedar quieto — re-orientar al frente (heading→0) → quieto mirando al frente
#ifdef ARQMIX_RETRO_BRAKE_ON_LINE
    escapar_adelante,            // MODO QUIETO (gateado): tras tocar la LÍNEA volviendo del pateo → AVANZA al frente HASTA despegarse (como el homing) → acomodar
#endif
};

void amix_fsm_init();
void amix_fsm_tick();

}  // namespace arqmix
}  // namespace iitasoccer
