// diag_top_tof_zonemap.cpp — Mapea la ORIENTACION INTERNA de las zonas de cada
// ToF (grilla 8x8). NO es firmware de competencia.
//
// Por que existe
// --------------
// Los 4 ToF VL53L7CX ya enumeran en el bus Wire (banco 2026-05-30):
//   TOF[0]=FRENTE(0x2A), TOF[1]=ATRAS(0x2B), TOF[2]=DERECHA(0x2C), TOF[3]=IZQUIERDA(0x2D)
// Pero cada sensor entrega una grilla 8x8 y el orden de sus 64 zonas depende de
// COMO esta montado fisicamente (rotacion, espejado). El ToF IZQUIERDO es de
// OTRO FABRICANTE y se monto mirando hacia abajo -> puede tener invertidos
// arriba/abajo y/o izquierda/derecha respecto a los otros 3.
//
// Para un "barrido tipo lidar 360" hay que saber, para cada sensor, que columna
// de la grilla corresponde a que angulo real alrededor del robot. Este programa
// lo deja VER: imprime la grilla cruda (indice raw = fila*8 + col) con la zona
// mas cercana marcada [###]. Movés la mano a una posicion CONOCIDA frente a un
// sensor (arriba / abajo / izq / der de su campo de vision) y anotás que
// fila/columna se enciende. Con eso se deduce la transformacion por sensor.
//
// Uso
// ---
//   pio run -e diag_top_tof_zonemap -t upload
//   QUITAR y reponer energia (las direcciones I2C persisten) -> monitor
//   pio device monitor -b 115200
//
// Por serial podes enviar (send_on_enter):
//   '0'..'3'  -> mostrar SOLO ese ToF (0=frente,1=atras,2=der,3=izq)
//   'a'       -> mostrar los 4 rotando
// Default: rota los 4, uno por refresco.
//
// Convencion de impresion: idx = fila*8 + col. Fila 0 arriba, col 0 a la
// izquierda TAL CUAL los entrega la lib (sin reordenar). Asi lo que ves es la
// orientacion REAL del sensor, no una "corregida".

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L7CX.h>

namespace {

constexpr uint8_t LP_PIN[]   = {9, 10, 11, 12};
const char*       LABEL[]    = {"FRENTE", "ATRAS ", "DERECHA", "IZQUIERDA"};
constexpr uint8_t ADDR[]     = {0x2A, 0x2B, 0x2C, 0x2D};
constexpr uint8_t N_TOF      = 4;
constexpr uint8_t WAKE = HIGH, SLEEP = LOW;
constexpr uint8_t DEFAULT_ADDR = 0x29;
constexpr uint8_t RES = 64;          // 8x8 para ver toda la grilla
constexpr uint8_t W   = 8;           // ancho de la grilla
constexpr uint8_t RANGE_HZ = 10;
constexpr uint32_t SETTLE_MS = 120;

Adafruit_VL53L7CX     g_tof[N_TOF];
VL53L7CX_ResultsData  g_res;
bool                  g_ready[N_TOF] = {false,false,false,false};
int                   g_show = -1;   // -1 = rotar todos; 0..3 = uno fijo
uint8_t               g_rot  = 0;

bool acks(uint8_t a){ Wire.beginTransmission(a); return Wire.endTransmission()==0; }

bool raw_change_addr(uint8_t cur, uint8_t next){
    Wire.beginTransmission(cur); Wire.write(0x7F); Wire.write(0xFF); Wire.write(0x00);
    if (Wire.endTransmission()!=0) return false;
    Wire.beginTransmission(cur); Wire.write(0x00); Wire.write(0x04); Wire.write(next);
    if (Wire.endTransmission()!=0) return false;
    delay(5); return acks(next);
}

void recover(){
    for (uint8_t i=0;i<N_TOF;i++){ pinMode(LP_PIN[i],OUTPUT); digitalWrite(LP_PIN[i],WAKE);}
    delay(60);
    const uint8_t disp[]={0x2A,0x2B,0x2C,0x2D,0x60};
    for (uint8_t k=0;k<sizeof(disp);k++)
        for (uint8_t t=0;t<4 && acks(disp[k]);t++) raw_change_addr(disp[k],DEFAULT_ADDR);
}

void enumerate(){
    for (uint8_t i=0;i<N_TOF;i++){ pinMode(LP_PIN[i],OUTPUT); digitalWrite(LP_PIN[i],SLEEP);}
    delay(SETTLE_MS);
    for (uint8_t i=0;i<N_TOF;i++){
        digitalWrite(LP_PIN[i],WAKE); delay(SETTLE_MS);
        Serial.print("[init "); Serial.print(LABEL[i]); Serial.print("] ");
        if (!acks(DEFAULT_ADDR)){ Serial.println("no aparecio en 0x29; salteo"); digitalWrite(LP_PIN[i],SLEEP); continue; }
        if (!g_tof[i].begin(DEFAULT_ADDR,&Wire,400000)){ Serial.println("begin() fallo"); continue; }
        if (!g_tof[i].setAddress(ADDR[i])){ Serial.println("setAddress fallo"); continue; }
        g_tof[i].setResolution(RES);
        g_tof[i].setRangingFrequency(RANGE_HZ);
        if (!g_tof[i].startRanging()){ Serial.println("startRanging fallo"); continue; }
        g_ready[i]=true;
        Serial.print("OK en 0x"); Serial.println(ADDR[i],HEX);
    }
}

void print_grid(uint8_t i){
    if (!g_ready[i]){ Serial.print(LABEL[i]); Serial.println(": (off)"); return; }
    if (!g_tof[i].isDataReady() || !g_tof[i].getRangingData(&g_res)) return;

    // Encontrar la zona valida mas cercana.
    int best_idx=-1; int16_t best=32767;
    for (uint8_t z=0; z<RES; z++){
        uint8_t s=g_res.target_status[z]; int16_t mm=g_res.distance_mm[z];
        if ((s==5||s==6||s==9) && mm>0 && mm<best){ best=mm; best_idx=z; }
    }

    Serial.print("==== ToF["); Serial.print(i); Serial.print("] ");
    Serial.print(LABEL[i]); Serial.print("  @0x"); Serial.print(ADDR[i],HEX);
    Serial.print("  (idx = fila*8 + col; fila0 arriba, col0 izq, SIN reordenar)");
    if (best_idx>=0){
        Serial.print("  min="); Serial.print(best); Serial.print("mm @ fila ");
        Serial.print(best_idx/W); Serial.print(" col "); Serial.print(best_idx%W);
        Serial.print(" (idx "); Serial.print(best_idx); Serial.print(")");
    }
    Serial.println();

    Serial.print("       col:");
    for (uint8_t c=0;c<W;c++){ Serial.print("   "); Serial.print(c); }
    Serial.println();
    for (uint8_t row=0; row<W; row++){
        Serial.print("  fila "); Serial.print(row); Serial.print(": ");
        for (uint8_t c=0;c<W;c++){
            uint8_t z=row*W+c;
            uint8_t s=g_res.target_status[z]; int16_t mm=g_res.distance_mm[z];
            bool valid=(s==5||s==6||s==9);
            char buf[8];
            if (z==(uint8_t)best_idx){ snprintf(buf,sizeof(buf),"[%3d]", (valid&&mm>=0)?mm:0); }
            else if (!valid || mm<0)  { snprintf(buf,sizeof(buf)," --- "); }
            else                      { snprintf(buf,sizeof(buf)," %3d ", mm); }
            Serial.print(buf);
        }
        Serial.println();
    }
    Serial.println();
}

void read_serial_cmd(){
    while (Serial.available()){
        char c=Serial.read();
        if (c>='0'&&c<='3'){ g_show=c-'0'; Serial.print(">> mostrando solo ToF "); Serial.println(g_show); }
        else if (c=='a'||c=='A'){ g_show=-1; Serial.println(">> mostrando los 4 (rotando)"); }
    }
}

}  // namespace

void setup(){
    Serial.begin(115200);
    uint32_t t0=millis(); while(!Serial && (millis()-t0)<3000){}
    Serial.println("\n===== TOP — Zonemap de los 4 ToF (orientacion interna) =====");
    Serial.println("Comandos: 0=frente 1=atras 2=der 3=izq  a=todos");
    Serial.println("Mové la mano arriba/abajo/izq/der de un sensor y mirá que fila/col se marca [###].");
    Wire.begin(); Wire.setClock(400000);
    Serial.print("BNO055 (0x28): "); Serial.println(acks(0x28)?"PRESENTE":"AUSENTE");
    recover();
    enumerate();
    Serial.println("Listo.\n");
}

void loop(){
    read_serial_cmd();
    static uint32_t last=0;
    if (millis()-last < 700) return;   // refresco lento, legible
    last=millis();

    if (g_show>=0){ print_grid((uint8_t)g_show); return; }
    // rotar
    print_grid(g_rot);
    g_rot=(g_rot+1)%N_TOF;
}
