// mix_seguir.h — SEGUIR la pelota rápido, apuntándola SOBRE LA MARCHA, y frenar a 10 cm.
//
// PROBLEMA que resuelve (de centralmix): ahí había 2 estados, AVANZAR y APUNTAR_PELOTA, y APUNTAR
// obligaba a FRENAR y girar en el propio eje → lento. Acá NO hay "parar para girar": el robot apunta
// la pelota MIENTRAS avanza, sumando un giro proporcional al ángulo de la pelota (control en P). Como
// el ángulo se corrige todo el tiempo, persigue "serpenteando" en vez de a los tirones.
//
// CÓMO (3 ideas que usan los equipos de RoboCup; ver fuentes en el chat):
//   1) CAMPO DE FUERZA: el robot TRASLADA hacia la pelota (es omni → va directo, no necesita estar
//      mirándola para acercarse). Rápido.
//   2) APUNTADO PROPORCIONAL (continuo): suma un GIRO = Kp · ángulo_pelota → el frente se va alineando
//      con la pelota sin frenar. Esto reemplaza al estado APUNTAR lento. Mantiene la pelota en cámara.
//   3) VELOCIDAD POR CERCANÍA (halo near/far): lejos va a full; cerca de 10 cm DESACELERA y FRENA, y
//      se queda ahí. Si la pelota se aleja, vuelve a perseguir. Anticipa la pelota EN MOVIMIENTO.
//
// REUSA mix_io (datos cámara/IMU) y mix_mover_vector / parar de mix_motors. Programa aparte, aditivo.
// ⚠️ NO TESTEADO EN HARDWARE. La velocidad de la pelota la calcula el TOP (en el sim, el shim).

#pragma once

namespace iitasoccer {
namespace mix {

void mix_seguir_init();              // estado = SEGUIR.
void mix_seguir_tick();              // un paso: lee g_io, decide, mueve. Respeta el árbitro.
const char* mix_seguir_estado_nombre();

// Debug:
float mix_seguir_aim_deg();          // ángulo al objetivo (pelota predicha) que está usando
float mix_seguir_dist_cm();          // distancia a la pelota

}  // namespace mix
}  // namespace iitasoccer
