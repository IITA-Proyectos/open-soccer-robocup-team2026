// diag_top_tof_census.cpp — Censo DEFINITIVO de ToF + control LP en bus unico.
// NO es firmware de competencia.
//
// Por que existe (supera a diag_top_tof_enumerate)
// ------------------------------------------------
// enumerate tenia 2 puntos ciegos que hacian su veredicto ambiguo:
//   1) PERSISTENCIA: las direcciones de los ToF sobreviven al reset del Teensy
//      (solo un power-cycle del modulo las borra). Si una corrida previa dejo
//      un ToF en 0x60/0x2A.., la siguiente arrancaba sucia y "no veia" nada.
//   2) POLARIDAD: enumerate asumia LP activo-ALTO. Si el LP real es activo-BAJO,
//      enumerate produce EXACTAMENTE el mismo output "1 ToF, 0 LP" aunque los 4
//      LP anden perfecto (al "dormir todos" en LOW en realidad los despierta a
//      todos, los aparta juntos a 0x60, y despues no ve nada). => no se podia
//      distinguir "1 solo ToF vivo" de "4 ToF con polaridad invertida".
//
// Este censo resuelve ambos:
//   • RECUPERA: antes de cada intento, junta cualquier ToF disperso
//     (0x2A..0x2D, 0x60) de vuelta a 0x29, asi arranca de un estado conocido
//     sin depender de que hagas power-cycle (igual conviene power-ciclar 1 vez).
//   • PRUEBA LAS 2 POLARIDADES: corre el algoritmo asumiendo activo-ALTO; si no
//     logra aislar ningun ToF, recupera y reintenta asumiendo activo-BAJO.
//   • REPORTA: cuantos ToF distinguibles encontro, con que pin LP cada uno, en
//     que polaridad, y cuantos quedaron "pegados" (LP no controlable).
//
// Metodo de aislamiento (clave del VL53L7CX en bus compartido)
// ------------------------------------------------------------
// Los ToF de fabrica comparten 0x29. Para usarlos en el mismo bus hay que
// cambiarles la direccion de a uno, y eso SOLO se puede si se los puede
// encender/apagar individualmente por su pin LP. Si dos ToF estan en 0x29 a la
// vez, un write de cambio-de-direccion lo reciben LOS DOS y se mueven juntos:
// imposible separarlos. Por eso TODO depende de que el LP de cada ToF este
// electricamente controlado por un pin distinto del Teensy. Este programa
// MIDE exactamente eso.
//
// Secuencia por polaridad:
//   a) "dormir" todos los candidatos. Lo que queda en 0x29 es 1+ ToF con LP NO
//      controlable -> se aparta a 0x60 (stuck). (Si son 2+ pegados, se mueven
//      juntos: no separables, se reporta.)
//   b) por cada pin candidato: dormir todos, despertar SOLO ese pin. Si aparece
//      un ToF en 0x29, ese pin ES su LP -> se lo mueve a 0x2A+i.
//   c) contar leibles y reportar.
//
// Uso
// ---
//   pio run -e diag_top_tof_census -t upload
//   pio device monitor -b 115200
// Recomendado: power-ciclar la placa una vez antes (no solo reset) para partir
// con los ToF en 0x29 de fabrica. El programa igual se auto-recupera.

#include <Arduino.h>
#include <Wire.h>

namespace {

// Union de TODAS las hipotesis de pines LP vistas hasta ahora:
//   Gustavo: {9,11,12,22} · analisis silk libre: {2,9,10,11,12,13,23}.
// Son los pines del Teensy fisicamente libres/accesibles como header. El bodge
// LP de cada ToF, si llego al Teensy, tiene que estar en este conjunto.
const uint8_t CANDIDATES[] = {2, 9, 10, 11, 12, 13, 22, 23};
constexpr uint8_t N_CAND = sizeof(CANDIDATES) / sizeof(CANDIDATES[0]);

constexpr uint8_t TOF_DEFAULT_ADDR = 0x29;
constexpr uint8_t BNO_ADDR         = 0x28;
const uint8_t     TARGET_ADDR[]    = {0x2A, 0x2B, 0x2C, 0x2D};
constexpr uint8_t N_TARGET = sizeof(TARGET_ADDR) / sizeof(TARGET_ADDR[0]);
constexpr uint8_t PARK_ADDR        = 0x60;

constexpr uint32_t LP_SETTLE_MS = 120;

bool acks(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// Cambia la dir del ToF que conteste en 'cur' -> 'next' (7-bit).
// Secuencia ST: page-0 (reg 0x7FFF=0x00) + reg 0x0004 = next. Validado: el
// parking 0x29->0x60 de enumerate funciono en banco.
bool change_addr(uint8_t cur, uint8_t next) {
    Wire.beginTransmission(cur);
    Wire.write(0x7F); Wire.write(0xFF); Wire.write(0x00);
    if (Wire.endTransmission() != 0) return false;
    Wire.beginTransmission(cur);
    Wire.write(0x00); Wire.write(0x04); Wire.write(next);
    if (Wire.endTransmission() != 0) return false;
    delay(5);
    return acks(next);
}

void print_hex(uint8_t v) {
    Serial.print("0x");
    if (v < 16) Serial.print('0');
    Serial.print(v, HEX);
}

void set_all(uint8_t level) {
    for (uint8_t i = 0; i < N_CAND; ++i) digitalWrite(CANDIDATES[i], level);
}

// Junta cualquier ToF disperso de vuelta a 0x29 (deshace persistencia previa).
// Despierta TODOS los candidatos en ambos niveles para maximizar que respondan.
void recover_to_default() {
    // Asegurar que ningun LP los tenga apagados: probamos los dos niveles.
    for (uint8_t lvl = 0; lvl < 2; ++lvl) {
        set_all(lvl == 0 ? HIGH : LOW);
        delay(60);
        const uint8_t dispersed[] = {0x2A, 0x2B, 0x2C, 0x2D, PARK_ADDR};
        for (uint8_t k = 0; k < sizeof(dispersed); ++k) {
            // Puede haber varios en la misma dir (herded): mover hasta 3 veces.
            for (uint8_t tries = 0; tries < 3; ++tries) {
                if (acks(dispersed[k])) change_addr(dispersed[k], TOF_DEFAULT_ADDR);
                else break;
            }
        }
    }
}

// Corre el algoritmo de aislamiento con una polaridad. Devuelve cuantos ToF
// aislo (movidos a target). Llena found_pin[]/found_addr[]. 'stuck' = hubo ToF
// con LP no controlable (apartado a PARK).
int isolate(bool active_high, uint8_t found_pin[], uint8_t found_addr[],
            bool& stuck) {
    const uint8_t sleep_lvl = active_high ? LOW : HIGH;
    const uint8_t wake_lvl  = active_high ? HIGH : LOW;
    stuck = false;

    Serial.print("\n--- Intento polaridad ");
    Serial.print(active_high ? "ACTIVO-ALTO (wake=HIGH)" : "ACTIVO-BAJO (wake=LOW)");
    Serial.println(" ---");

    // a) dormir todos; apartar pegado(s).
    set_all(sleep_lvl);
    delay(LP_SETTLE_MS);
    if (acks(TOF_DEFAULT_ADDR)) {
        stuck = true;
        Serial.print("  todos dormidos pero 0x29 presente -> aparto pegado a ");
        print_hex(PARK_ADDR);
        bool ok = change_addr(TOF_DEFAULT_ADDR, PARK_ADDR);
        Serial.println(ok ? " : OK" : " : FALLO write");
        delay(LP_SETTLE_MS);
        if (acks(TOF_DEFAULT_ADDR))
            Serial.println("  (0x29 SIGUE -> 2+ ToF pegados, no separables)");
    } else {
        Serial.println("  todos dormidos -> 0x29 ausente (buena señal: LP apaga)");
    }

    // b) despertar de a uno.
    int n = 0;
    for (uint8_t i = 0; i < N_CAND && n < N_TARGET; ++i) {
        set_all(sleep_lvl);
        digitalWrite(CANDIDATES[i], wake_lvl);
        delay(LP_SETTLE_MS);
        Serial.print("  pin ");
        if (CANDIDATES[i] < 10) Serial.print(' ');
        Serial.print(CANDIDATES[i]);
        Serial.print(" : ");
        if (acks(TOF_DEFAULT_ADDR)) {
            if (change_addr(TOF_DEFAULT_ADDR, TARGET_ADDR[n])) {
                Serial.print("LP OK -> ToF movido a ");
                print_hex(TARGET_ADDR[n]);
                Serial.println();
                found_pin[n]  = CANDIDATES[i];
                found_addr[n] = TARGET_ADDR[n];
                ++n;
            } else {
                Serial.println("desperto ToF en 0x29 pero fallo el cambio de dir");
            }
        } else {
            Serial.println("sin efecto");
        }
    }
    set_all(sleep_lvl);
    return n;
}

void run_once() {
    Serial.println("\n============================================================");
    Serial.println("  TOP — Censo de ToF + control LP (auto-polaridad, bus Wire)");
    Serial.println("============================================================");
    Serial.print("BNO055 (0x28): ");
    Serial.println(acks(BNO_ADDR) ? "PRESENTE (bus OK)" : "AUSENTE (revisar bus!)");

    Serial.println("Recuperando ToF dispersos -> 0x29 ...");
    recover_to_default();
    Serial.print("Tras recuperar, 0x29: ");
    Serial.println(acks(TOF_DEFAULT_ADDR) ? "PRESENTE (hay >=1 ToF vivo)"
                                          : "AUSENTE (ningun ToF responde!)");

    uint8_t fp[N_TARGET], fa[N_TARGET];
    bool stuck = false;

    int n = isolate(/*active_high=*/true, fp, fa, stuck);
    bool used_high = true;
    if (n == 0) {
        Serial.println("\n  activo-ALTO no aislo ninguno -> recupero y pruebo activo-BAJO");
        recover_to_default();
        n = isolate(/*active_high=*/false, fp, fa, stuck);
        used_high = false;
    }

    // Conteo final real desde el bus.
    int readable = 0;
    for (uint8_t a = 0x29; a <= 0x2D; ++a) if (acks(a)) ++readable;
    if (acks(PARK_ADDR)) ++readable;

    Serial.println("\n------------------------------------------------------------");
    Serial.print("Polaridad que funciono: ");
    Serial.println(n > 0 ? (used_high ? "ACTIVO-ALTO" : "ACTIVO-BAJO") : "(ninguna)");
    Serial.print("ToF aislados por LP: ");
    Serial.println(n);
    for (int i = 0; i < n; ++i) {
        Serial.print("   LP pin ");
        Serial.print(fp[i]);
        Serial.print("  ->  ");
        print_hex(fa[i]);
        Serial.println();
    }
    if (stuck) Serial.println("   + 1 (o mas) ToF PEGADO en 0x60 (LP no controlable)");
    Serial.print("ToF leibles totales en el bus: ");
    Serial.println(readable);

    Serial.print("\nVEREDICTO: ");
    if (n >= 3) {
        Serial.println("LP control FUNCIONA en >=3 ToF. Enumeracion viable!");
        Serial.print("  -> En pinout_robotN.h: PIN_TOF_XSHUT = { ");
        for (int i = 0; i < n; ++i) { Serial.print(fp[i]); Serial.print(' '); }
        if (stuck) Serial.print("(+1 pegado) ");
        Serial.println("}");
        Serial.println("  -> Polaridad para el firmware: ver arriba (HIGH/LOW = wake).");
        Serial.println("  -> Siguiente: tapar cada sensor para mapear dir->posicion.");
    } else if (n >= 1) {
        Serial.print(n);
        Serial.println(" ToF controlable(s) por LP, el resto NO.");
        Serial.println("  Los pines que dieron 'sin efecto' en AMBAS polaridades son");
        Serial.println("  LP que no llegaron al Teensy (cable flojo / en pad INT / NC).");
        Serial.println("  ACCION Enzo: multimetro de continuidad pin-Teensy -> pad LP de");
        Serial.println("  los ToF que faltan. Con <3 LP no hay localizacion 2D completa.");
    } else {
        Serial.println("CERO ToF controlables por LP en ninguna polaridad.");
        Serial.println("  Solo 1 ToF es usable (el que responde fijo). El bodge LP no");
        Serial.println("  esta dando control. Causas: cables LP en pad INT (no LP), o");
        Serial.println("  sin continuidad, o solo 1 ToF realmente comunica en el bus.");
        Serial.println("  ACCION Enzo: multimetro: (1) cada pad LP de ToF -> que pin del");
        Serial.println("  Teensy; (2) que los 4 ToF tengan 3V3 y SDA/SCL continuos.");
    }
    Serial.println("============================================================\n");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) {}

    for (uint8_t i = 0; i < N_CAND; ++i) {
        pinMode(CANDIDATES[i], OUTPUT);
        digitalWrite(CANDIDATES[i], LOW);
    }

    Wire.begin();
    Wire.setClock(400000);

    run_once();
}

void loop() {
    delay(6000);
    run_once();
}
