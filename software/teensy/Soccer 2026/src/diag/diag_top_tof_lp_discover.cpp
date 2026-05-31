// diag_top_tof_lp_discover.cpp — Descubridor EMPIRICO de los pines LP de los ToF.
//
// NO es firmware de competencia. Resuelve un problema concreto: tras el
// recableado del 2026-05-30 (Enzo), los 4 ToF VL53L7CX quedaron TODOS en el
// I2C principal (Wire, pines 18/19) y cada uno tiene su pata LP cableada por
// bodge a un pin del Teensy (los que antes iban a INT, anulando INT). PERO
// ese bodge es fisico: NO esta en el schematic/PCB del repo (ahi los pads
// LP/INT de los ToF figuran como No-Connect). Asi que no se puede saber por
// documentacion a que pin del Teensy cayo cada LP. Este programa lo AVERIGUA.
//
// Idea
// ----
// Los 4 ToF salen de fabrica en la MISMA direccion 0x29. En un mismo bus, el
// pin LP de cada ToF habilita/deshabilita su interfaz I2C:
//   • LP "inactivo"  -> el ToF NO contesta en el bus (no hace ACK).
//   • LP "activo"    -> el ToF contesta en 0x29.
// Entonces: si dejo TODOS los ToF dormidos y voy despertando de a UNO los
// pines candidatos, el pin que hace APARECER 0x29 en el bus ES un pin LP.
// (Con 4 ToF compartiendo 0x29 no se puede distinguir "cuantos" hay mirando
//  solo 0x29 — por eso despierto de a uno: 0 ToF -> 0x29 ausente; 1 ToF -> 0x29
//  presente. La transicion ausente->presente delata el pin LP.)
//
// Polaridad
// ---------
// El VL53L7CX usa LPn = "low power", normalmente ACTIVO EN ALTO (HIGH = ToF
// despierto). Igual el programa AUTODETECTA la polaridad: prueba con baseline
// todos-LOW (asume activo-alto) y, si eso ya muestra 0x29 (señal de que estan
// despiertos con LOW), reintenta con baseline todos-HIGH (activo-bajo).
//
// Que NO hace
// -----------
// No mapea pin -> posicion fisica (frontal/trasero/izq/der). Eso se hace en el
// paso siguiente (enumeracion + ranging): se tapa cada ToF y se mira que
// direccion cambia de distancia. Aca solo sacamos el CONJUNTO de 4 pines LP.
//
// Uso
// ---
//   pio run -e diag_top_tof_lp_discover -t upload
//   pio device monitor -b 115200
//
// Pegale el resultado a Claude: con los 4 pines escribo la enumeracion
// (asignar 0x2A/0x2B/0x2C y leer los 4 a la vez).

#include <Arduino.h>
#include <Wire.h>

namespace {

// Pines candidatos = los 7 pines del Teensy 4.0 que estan FISICAMENTE LIBRES
// y accesibles como header en esta placa (analisis cruzado de otra sesion,
// silk del PCB): 2, 9, 10, 11, 12, 13, 23
//   silk: OUT2, OUT1C, CS, MOSI, MISO, SCK, A9.
// Enzo uso 4 de estos 7 para el bodge LP. Un cable volante solo puede caer en
// un pin accesible -> el LP de cada ToF DEBE estar en este conjunto (si es que
// llego al Teensy).
//
// v3 (2026-05-30): CLAVE — ahora SI incluimos el pin 13. La v2 lo excluia (lo
// usaba de LED) y eso era un punto ciego: si un cable LP cayo en el 13, la v2
// nunca lo bajaba y daba "concluyente roto" como falso negativo. El pin 13
// tiene el LED onboard, pero igual se puede manejar como GPIO para sondear LP.
// Ya NO usamos 13 como LED de estado.
//
// Por que NO barremos el resto: el forense del PCB confirma que tanto LP como
// INT de los ToF son NC (sin cobre al Teensy). Si el bodge funciona, el cable
// nuevo solo pudo ir a un header libre = este set. Barrer pines ocupados
// (HC-SR04 pin 7, UARTs, I2C, camaras) solo agrega riesgo de contencion.
const uint8_t CANDIDATES[] = {2, 9, 10, 11, 12, 13, 23};
constexpr uint8_t N_CAND = sizeof(CANDIDATES) / sizeof(CANDIDATES[0]);

constexpr uint8_t TOF_DEFAULT_ADDR = 0x29;  // direccion de fabrica del VL53L7CX
constexpr uint8_t BNO_ADDR         = 0x28;  // sanity-check: el BNO del bus

// Demora tras cambiar un LP para que el ToF arranque su interfaz I2C.
constexpr uint32_t LP_SETTLE_MS = 120;

// ¿Contesta 'addr' en el bus? (true = ACK = dispositivo presente)
bool acks(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

void set_all_candidates(uint8_t level) {
    for (uint8_t i = 0; i < N_CAND; ++i) digitalWrite(CANDIDATES[i], level);
}

// Corre el descubrimiento con una polaridad dada.
//   active_high=true  -> baseline LOW (dormidos), se DESPIERTA poniendo HIGH.
//   active_high=false -> baseline HIGH (dormidos), se DESPIERTA poniendo LOW.
// Devuelve cuantos LP encontro; llena found[] con los pines.
int discover(bool active_high, uint8_t found[], int max_found) {
    const uint8_t sleep_level = active_high ? LOW : HIGH;
    const uint8_t wake_level  = active_high ? HIGH : LOW;

    // 1) Todos dormidos -> baseline.
    set_all_candidates(sleep_level);
    delay(LP_SETTLE_MS);
    const bool base_29 = acks(TOF_DEFAULT_ADDR);

    Serial.print("  [");
    Serial.print(active_high ? "activo-ALTO" : "activo-BAJO");
    Serial.print("] baseline (todos dormidos): 0x29 ");
    Serial.println(base_29 ? "PRESENTE (mal: algun ToF sigue despierto)"
                           : "ausente (ok)");

    if (base_29) {
        // Con todos "dormidos" 0x29 igual contesta -> esta polaridad no sirve
        // (o hay un LP fuera de la lista / hardwired). El caller probara la otra.
        return -1;
    }

    // 2) Despertar de a uno; el que haga aparecer 0x29 es un LP.
    int n = 0;
    for (uint8_t i = 0; i < N_CAND; ++i) {
        const uint8_t pin = CANDIDATES[i];
        digitalWrite(pin, wake_level);
        delay(LP_SETTLE_MS);
        const bool now_29 = acks(TOF_DEFAULT_ADDR);
        digitalWrite(pin, sleep_level);
        delay(40);

        Serial.print("    pin ");
        if (pin < 10) Serial.print(' ');
        Serial.print(pin);
        Serial.print(" -> 0x29 ");
        if (now_29) {
            Serial.println("APARECE  ==> ES PIN LP de un ToF");
            if (n < max_found) found[n] = pin;
            ++n;
        } else {
            Serial.println("no");
        }
    }
    return n;
}

void run_once() {
    Serial.println("\n========================================================");
    Serial.println("  TOP — Descubridor de pines LP de los ToF (bus Wire)");
    Serial.println("========================================================");

    // Sanity del bus: el BNO debe estar.
    Serial.print("Sanity: BNO055 (0x28) en Wire: ");
    Serial.println(acks(BNO_ADDR) ? "PRESENTE (bus OK)" : "AUSENTE (revisar bus!)");

    uint8_t found[N_CAND];
    int n = discover(/*active_high=*/true, found, N_CAND);
    if (n < 0) {
        Serial.println("  -> reintento asumiendo LP activo-BAJO...");
        n = discover(/*active_high=*/false, found, N_CAND);
    }

    Serial.println("--------------------------------------------------------");
    if (n < 0) {
        // Ni con TODO el rango seguro (0..33 menos I2C/cam/LED) se logro apagar
        // los ToF. Como ya no quedan pines por probar, esto es CONCLUYENTE:
        Serial.println("RESULTADO (concluyente): con TODOS los pines manejables");
        Serial.println("en su nivel 'dormir', al menos un ToF sigue contestando");
        Serial.println("en 0x29. O sea: el pin LP de >=1 ToF NO esta siendo");
        Serial.println("controlado electricamente por el Teensy. Causas tipicas:");
        Serial.println("  (a) el cable del bodge quedo en el pad INT (salida del");
        Serial.println("      sensor), no en LP -> driving no lo apaga.");
        Serial.println("  (b) el cable LP quedo flojo / sin continuidad -> el");
        Serial.println("      pull-up del modulo lo deja siempre despierto.");
        Serial.println("  (c) LP de ese ToF no se cableo (quedo NC).");
        Serial.println("ACCION: Enzo revisa continuidad pin-Teensy -> pad LP de");
        Serial.println("cada ToF con multimetro. Con 4 ToF en 0x29 no se pueden");
        Serial.println("enumerar hasta que CADA LP sea controlable por separado.");
    } else if (n == 0) {
        Serial.println("RESULTADO: baseline OK (apague todos) pero ningun pin");
        Serial.println("individual reactivo 0x29. Raro: revisar soldaduras ToF.");
    } else {
        Serial.print("RESULTADO: encontre ");
        Serial.print(n);
        Serial.println(" pin(es) LP de ToF:");
        Serial.print("  PIN_TOF_XSHUT[] candidatos = { ");
        for (int i = 0; i < n; ++i) {
            Serial.print(found[i]);
            if (i < n - 1) Serial.print(", ");
        }
        Serial.println(" }");
        if (n == 4) {
            Serial.println("  -> 4 ToF detectados. Pegale esto a Claude para la enumeracion.");
        } else {
            Serial.print("  -> esperaba 4 ToF, encontre ");
            Serial.print(n);
            Serial.println(". Revisar soldaduras / ampliar candidatos.");
        }
        Serial.println("  (El mapeo pin->posicion frontal/tras/izq/der se hace");
        Serial.println("   en el paso de ranging tapando cada sensor.)");
    }
    Serial.println("========================================================\n");
}

}  // namespace

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 3000) { /* esperar USB hasta 3s */ }

    // Todos los candidatos como salida, en LOW (estado seguro por defecto).
    for (uint8_t i = 0; i < N_CAND; ++i) {
        pinMode(CANDIDATES[i], OUTPUT);
        digitalWrite(CANDIDATES[i], LOW);
    }

    Wire.begin();
    Wire.setClock(400000);

    run_once();
}

void loop() {
    // Re-corre cada ~5 s para que puedas recablear/resoldar y ver el resultado
    // actualizado sin resetear.
    for (int i = 0; i < 10; ++i) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        delay(500);
    }
    run_once();
}
