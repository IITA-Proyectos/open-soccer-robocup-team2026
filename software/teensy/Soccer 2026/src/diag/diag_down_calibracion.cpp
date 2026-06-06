// diag_down_calibracion.cpp — Calibración interactiva de los sensores de línea
//                             (placa DOWN, Teensy 4.0). NO es firmware de competencia.
//
// Para qué sirve:
//   Herramienta de CANCHA para calibrar los 32 sensores de línea contra el
//   verde real del campo y el blanco real de las líneas, y GUARDAR esa
//   calibración en EEPROM para que sobreviva al apagado. La iluminación y las
//   superficies cambian de un lugar a otro, así que esto se hace en el lugar.
//
//   Usa las funciones de calibración REALES del firmware (line_ring +
//   eeprom_calib), así lo que calibrás acá es exactamente lo que despues usa
//   el firmware de competencia.
//
// Cómo se usa (monitor serie a 115200):
//   1. Poné el robot sobre el VERDE del campo (sensores sobre carpet).
//      Escribí  'c'  + Enter  -> captura el verde.
//   2. Poné los sensores sobre una LÍNEA BLANCA (lo más cubierto posible).
//      Escribí  'b'  + Enter  -> captura el blanco.
//   3. Escribí  't'  + Enter  -> prueba la deteccion (movés el robot y mirás
//      si detecta la linea con la calib recien hecha, ANTES de guardar).
//   4. Si quedó bien, escribí  's'  + Enter  -> guarda en EEPROM (persistente).
//
//   Otros comandos:
//     'v'  ver estado: promedios verde/blanco, umbral y margen por grupo.
//     'm'  ver si hay sensores SOSPECHOSOS (no separan verde de blanco).
//     'l'  cargar la calib guardada en EEPROM (para revisar la que ya tenias).
//     'x'  borrar la calib de EEPROM (forzar recalibracion).
//     'h'  ayuda (vuelve a mostrar estos comandos).
//
// Build (placa DOWN, Teensy 4.0):
//   pio run -e diag_down_calibracion -t upload
//   pio device monitor -b 115200
//
// Nota: usa 4 muxes (32 sensores) y NO toca los OTOS (no hacen falta para
// calibrar la linea). Si tu placa tiene menos muxes, ajustar el flag del env.
//
// Atribución:
//   Author: Claude Opus 4.8 (Anthropic), pedido de Maria Viollaz.

#include <Arduino.h>

#include "config_down.h"
#include "line_ring.h"
#include "line_calib.h"
#include "eeprom_calib.h"

using namespace iitasoccer;

namespace {

// Estado de la calibración en curso (derivada del line_ring tras cada captura).
SensorCalib g_calib[NUM_LINE_SENSORS];
bool g_carpet_listo = false;
bool g_blanco_listo = false;

// Margen minimo para considerar que un sensor separa verde de blanco.
// Alineado con calib_min_margin del firmware (comm_central.cpp). OJO: el firmware
// AUN puede marcar CALIB? si la auto-adaptacion del carpet erosiona el margen en vivo
// por debajo de este valor (ver nota en comm_central.cpp). Banco 2026-06-06: 80 -> 60 -> 40.
constexpr uint16_t MIN_MARGIN = 40;

// Deriva g_calib desde los promedios actuales del line_ring (carpet + white).
void derivar_calib() {
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        lc_set_static(g_calib[i],
                      line_ring_get_carpet_avg(static_cast<uint8_t>(i)),
                      line_ring_get_white_avg(static_cast<uint8_t>(i)));
    }
}

void mostrar_ayuda() {
    Serial.println();
    Serial.println(F("=== CALIBRACION SENSORES DE LINEA (placa DOWN) ==="));
    Serial.println(F(" 'c' = capturar VERDE  (robot sobre el carpet)"));
    Serial.println(F(" 'b' = capturar BLANCO (sensores sobre la linea)"));
    Serial.println(F(" 't' = probar deteccion en vivo (sin guardar)"));
    Serial.println(F(" 's' = GUARDAR en EEPROM (persistente)"));
    Serial.println(F(" 'v' = ver promedios y umbrales"));
    Serial.println(F(" 'm' = ver sensores sospechosos"));
    Serial.println(F(" 'l' = cargar calib guardada en EEPROM"));
    Serial.println(F(" 'x' = borrar calib de EEPROM"));
    Serial.println(F(" 'h' = esta ayuda"));
    Serial.println(F("=================================================="));
    Serial.print  (F(" estado: VERDE="));
    Serial.print(g_carpet_listo ? "ok" : "--");
    Serial.print(F("  BLANCO="));
    Serial.println(g_blanco_listo ? "ok" : "--");
    Serial.println();
}

void capturar_verde() {
    Serial.println(F("[CAL] Capturando VERDE... no mover (~0.5s)"));
    line_ring_calibrate_carpet();
    g_carpet_listo = true;
    derivar_calib();
    Serial.println(F("[CAL] VERDE capturado."));
}

void capturar_blanco() {
    Serial.println(F("[CAL] Capturando BLANCO... no mover (~0.5s)"));
    line_ring_calibrate_white();
    g_blanco_listo = true;
    derivar_calib();
    Serial.println(F("[CAL] BLANCO capturado."));
}

void ver_promedios() {
    Serial.println(F("[CAL] sensor: verde / blanco / umbral / margen"));
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        const uint16_t c = line_ring_get_carpet_avg(static_cast<uint8_t>(i));
        const uint16_t w = line_ring_get_white_avg(static_cast<uint8_t>(i));
        const int margen = (int)w - (int)c;
        Serial.print(F("  S"));
        if (i + 1 < 10) Serial.print('0');
        Serial.print(i + 1);
        Serial.print(F(": "));
        Serial.print(c);    Serial.print(F(" / "));
        Serial.print(w);    Serial.print(F(" / "));
        Serial.print((c + w) / 2);  Serial.print(F(" / "));
        Serial.print(margen);
        if (abs(margen) < (int)MIN_MARGIN) Serial.print(F("  <-- SOSPECHOSO"));
        Serial.println();
    }
}

void ver_sospechosos() {
    int n_sosp = 0;
    for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
        const uint16_t c = line_ring_get_carpet_avg(static_cast<uint8_t>(i));
        const uint16_t w = line_ring_get_white_avg(static_cast<uint8_t>(i));
        if (abs((int)w - (int)c) < (int)MIN_MARGIN) {
            Serial.print(F("  S"));
            Serial.print(i + 1);
            Serial.print(F(" margen="));
            Serial.println((int)w - (int)c);
            n_sosp++;
        }
    }
    Serial.print(F("[CAL] sensores sospechosos: "));
    Serial.print(n_sosp);
    Serial.print(F(" / "));
    Serial.println(NUM_LINE_SENSORS);
    if (n_sosp == 0) Serial.println(F("[CAL] todos separan verde/blanco OK."));
    else Serial.println(F("[CAL] revisar esos sensores o recalibrar."));
}

void guardar_eeprom() {
    if (!g_carpet_listo || !g_blanco_listo) {
        Serial.println(F("[CAL] FALTA capturar verde Y blanco antes de guardar."));
        return;
    }
    derivar_calib();
    if (ec_save_calibration(g_calib, NUM_LINE_SENSORS)) {
        Serial.println(F("[CAL] *** Calibracion GUARDADA en EEPROM (persistente) ***"));
    } else {
        Serial.println(F("[CAL] ERROR: fallo al guardar en EEPROM."));
    }
}

void cargar_eeprom() {
    if (ec_load_calibration(g_calib, NUM_LINE_SENSORS)) {
        Serial.println(F("[CAL] Calib cargada de EEPROM. Umbrales:"));
        for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
            Serial.print(F("  S"));
            if (i + 1 < 10) Serial.print('0');
            Serial.print(i + 1);
            Serial.print(F(" umbral="));
            Serial.print(g_calib[i].threshold);
            Serial.print(F(" (v="));
            Serial.print(g_calib[i].carpet);
            Serial.print(F(" b="));
            Serial.print(g_calib[i].white);
            Serial.println(F(")"));
        }
    } else {
        Serial.println(F("[CAL] EEPROM vacia o invalida (no hay calib guardada)."));
    }
}

void borrar_eeprom() {
    ec_erase_calibration();
    Serial.println(F("[CAL] Calib de EEPROM borrada."));
}

// Modo prueba: muestra en vivo cuantos sensores ven blanco con la calib actual.
bool g_modo_test = false;
elapsedMillis g_since_test_print;

void procesar_comando(char c) {
    switch (c) {
        case 'c': case 'C': capturar_verde();  break;
        case 'b': case 'B': capturar_blanco(); break;
        case 's': case 'S': guardar_eeprom();  break;
        case 'v': case 'V': ver_promedios();   break;
        case 'm': case 'M': ver_sospechosos(); break;
        case 'l': case 'L': cargar_eeprom();   break;
        case 'x': case 'X': borrar_eeprom();   break;
        case 'h': case 'H': mostrar_ayuda();   break;
        case 't': case 'T':
            g_modo_test = !g_modo_test;
            Serial.print(F("[CAL] modo prueba deteccion: "));
            Serial.println(g_modo_test ? "ON (mové el robot sobre la linea)" : "OFF");
            break;
        default: break;   // ignorar \n, \r, espacios
    }
}

}  // namespace

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    Serial.begin(115200);
    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* spin */ }

    line_ring_init();

    // Captura inicial de verde (asume robot sobre carpet al encender), igual que
    // hace el firmware. El usuario puede recapturar con 'c' cuando quiera.
    Serial.println(F("[CAL] init... capturando verde inicial (no mover)"));
    line_ring_calibrate_carpet();
    g_carpet_listo = true;
    derivar_calib();

    // Si hay calib en EEPROM, avisamos (no la pisamos hasta que el usuario
    // capture y guarde de nuevo).
    SensorCalib tmp[NUM_LINE_SENSORS];
    if (ec_load_calibration(tmp, NUM_LINE_SENSORS)) {
        Serial.println(F("[CAL] (hay una calib previa guardada en EEPROM; 'l' para verla)"));
    }

    mostrar_ayuda();
}

void loop() {
    line_ring_tick();   // muestrear sensores continuamente

    // Procesar comandos del monitor (un caracter por vez).
    while (Serial.available() > 0) {
        procesar_comando(static_cast<char>(Serial.read()));
    }

    // Modo prueba: cada 300 ms muestra cuantos sensores ven blanco + el angulo.
    if (g_modo_test && g_since_test_print >= 300) {
        g_since_test_print = 0;
        int n_blanco = 0;
        for (int i = 0; i < NUM_LINE_SENSORS; ++i) {
            if (line_ring_get_white(static_cast<uint8_t>(i))) n_blanco++;
        }
        Serial.print(F("[TEST] sensores en blanco: "));
        if (n_blanco < 10) Serial.print('0');
        Serial.print(n_blanco);
        Serial.print(F("/"));
        Serial.print(NUM_LINE_SENSORS);
        Serial.print(F("  angulo="));
        Serial.print(line_ring_get_angle_deg(), 1);
        Serial.print(F("  depth="));
        Serial.print(line_ring_get_depth());
        Serial.print(F("  imm_exit="));
        Serial.println(line_ring_get_imminent_exit() ? "SI" : "no");
    }

    // LED: parpadeo lento = listo.
    digitalWrite(PIN_LED_STATUS, (millis() / 500) % 2);
}
