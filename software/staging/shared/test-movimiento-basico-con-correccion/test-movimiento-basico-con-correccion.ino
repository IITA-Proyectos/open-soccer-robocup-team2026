// =============================================================================
// TEST: Movimiento básico adelante/atrás con RAMPAS de aceleración/frenado
// Archivo: staging/shared/test-movimiento-basico-con-correccion/
//          test-movimiento-basico-con-correccion.ino
//
// SOLUCIÓN AL CABECEO:
//   RAMPAS suaves de aceleración y frenado.
//
// PID CON BOOST:
//   Si el heading se desvía más de 10°, la corrección se multiplica x2.5
//   para volver rápido. Errores chicos se corrigen suavemente.
//
// ROBOT 2 (delantero)
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

#define INA1 8
#define INB1 7
#define PWM1 6
#define INA2 11
#define INB2 12
#define PWM2 4
#define INA3 2
#define INB3 5
#define PWM3 3
#define PIN_BOTON 9

// ******************** PARÁMETROS AJUSTABLES ********************

const int VELOCIDAD_BASE = 150;
const unsigned long TIEMPO_MOVIMIENTO = 3000;
const unsigned long PAUSA_ENTRE_MOVIMIENTOS = 500;

// Rampas
unsigned long TIEMPO_RAMPA_MS = 400;

// PID Heading
float Kp = 3.0;
float Ki = 0.08;
float Kd = 0.8;
const float MAX_CORRECCION = 120;
const float INTEGRAL_MAX = 50;

// Boost no-lineal para errores grandes
const float UMBRAL_BOOST = 10.0;
const float FACTOR_BOOST = 2.5;

// ******************** FIN PARÁMETROS ********************

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
float headingOffset = 0;
bool bnoOK = false;

float errorAnterior = 0;
float integral = 0;
unsigned long tiempoAnteriorPID = 0;

enum EstadoTest { ESPERANDO_INICIO, TEST_HEADING, ESPERANDO_LOOP, LOOP_ADELANTE, LOOP_PAUSA_1, LOOP_ATRAS, LOOP_PAUSA_2 };
EstadoTest estadoTest = ESPERANDO_INICIO;
unsigned long tiempoInicioTest = 0;
int cicloActual = 0;
bool botonAnterior = false;

// === GIRÓSCOPO ===
bool inicializarGyro() {
  Serial.println("\n=== INICIALIZANDO BNO055 (NO MOVER!) ===");
  Serial.print("1. Detectando... ");
  unsigned long inicio = millis();
  while (!bno.begin()) { if (millis()-inicio>3000){Serial.println("ERROR!");return false;} delay(100); }
  Serial.println("OK!");
  Serial.print("2. Cristal externo... "); bno.setExtCrystalUse(true); Serial.println("OK!");
  Serial.print("3. Estabilización (1s)... "); delay(1000); Serial.println("OK!");
  Serial.println("4. Calibrando giróscopo (NO MOVER!)...");
  inicio = millis(); bool cal = false;
  while (millis()-inicio < 5000) {
    uint8_t sys,gyro,accel,mag; bno.getCalibration(&sys,&gyro,&accel,&mag);
    Serial.print("   SYS=");Serial.print(sys);Serial.print(" GYR=");Serial.println(gyro);
    if(gyro>=3){Serial.print("   -> Calibrado en ");Serial.print(millis()-inicio);Serial.println("ms!");cal=true;break;}
    digitalWrite(LED_BUILTIN,(millis()/200)%2); delay(250);
  }
  if(!cal)Serial.println("   ADVERTENCIA: Calibración incompleta");
  Serial.println("5. Estableciendo posición cero...");
  float suma=0;
  for(int i=0;i<10;i++){sensors_event_t ev;bno.getEvent(&ev);suma+=ev.orientation.x;
    Serial.print("   Lectura ");Serial.print(i+1);Serial.print("/10: ");Serial.println(ev.orientation.x,1);delay(50);}
  headingOffset=suma/10.0;
  Serial.print("   -> Offset: ");Serial.println(headingOffset,1);
  Serial.println("\n=== GIROSCOPO LISTO ==="); digitalWrite(LED_BUILTIN,HIGH);
  return true;
}

float leerHeading() {
  if(!bnoOK)return 0;
  sensors_event_t ev; bno.getEvent(&ev);
  float h=ev.orientation.x-headingOffset;
  if(h>180)h-=360; if(h<-180)h+=360;
  return h;
}

// === PID CON BOOST ===
void resetPID() { errorAnterior=0; integral=0; tiempoAnteriorPID=millis(); }

float calcularCorreccionPID(float headingActual) {
  float error = 0 - headingActual;
  unsigned long ahora = millis();
  float dt = (ahora - tiempoAnteriorPID) / 1000.0;
  if (dt <= 0) dt = 0.01;
  tiempoAnteriorPID = ahora;

  float P = Kp * error;
  integral += error * dt;
  integral = constrain(integral, -INTEGRAL_MAX, INTEGRAL_MAX);
  float I = Ki * integral;
  float derivada = (error - errorAnterior) / dt;
  float D = Kd * derivada;
  errorAnterior = error;

  float correccion = P + I + D;

  // BOOST: error grande → corrección más fuerte → vuelve rápido
  if (abs(error) > UMBRAL_BOOST) {
    correccion *= FACTOR_BOOST;
  }

  return constrain(correccion, -MAX_CORRECCION, MAX_CORRECCION);
}

// === RAMPA ===
float calcularFactorRampa(unsigned long tiempoTranscurrido, unsigned long tiempoTotal) {
  if (tiempoTranscurrido < TIEMPO_RAMPA_MS)
    return (float)tiempoTranscurrido / (float)TIEMPO_RAMPA_MS;
  unsigned long tiempoRestante = tiempoTotal - tiempoTranscurrido;
  if (tiempoRestante < TIEMPO_RAMPA_MS)
    return (float)tiempoRestante / (float)TIEMPO_RAMPA_MS;
  return 1.0;
}

// === MOVIMIENTO CON RAMPA ===
void moverAdelante(int velocidadBase, float factorRampa) {
  int velReal = (int)(velocidadBase * factorRampa);
  float correccion = calcularCorreccionPID(leerHeading());
  correccion *= factorRampa;
  int velM1 = constrain(velReal + (int)correccion, 0, 255);
  int velM2 = constrain(velReal - (int)correccion, 0, 255);
  analogWrite(PWM1, velM1); digitalWrite(INA1, 1); digitalWrite(INB1, 0);
  analogWrite(PWM2, velM2); digitalWrite(INA2, 0); digitalWrite(INB2, 1);
  analogWrite(PWM3, 0); digitalWrite(INA3, 0); digitalWrite(INB3, 0);
}

void moverAtras(int velocidadBase, float factorRampa) {
  int velReal = (int)(velocidadBase * factorRampa);
  float correccion = calcularCorreccionPID(leerHeading());
  correccion *= factorRampa;
  int velM1 = constrain(velReal - (int)correccion, 0, 255);
  int velM2 = constrain(velReal + (int)correccion, 0, 255);
  analogWrite(PWM1, velM1); digitalWrite(INA1, 0); digitalWrite(INB1, 1);
  analogWrite(PWM2, velM2); digitalWrite(INA2, 1); digitalWrite(INB2, 0);
  analogWrite(PWM3, 0); digitalWrite(INA3, 0); digitalWrite(INB3, 0);
}

void parar() {
  analogWrite(PWM1,0);digitalWrite(INA1,0);digitalWrite(INB1,0);
  analogWrite(PWM2,0);digitalWrite(INA2,0);digitalWrite(INB2,0);
  analogWrite(PWM3,0);digitalWrite(INA3,0);digitalWrite(INB3,0);
}

// === SERIAL ===
void procesarComandoSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case '+': TIEMPO_RAMPA_MS+=50; if(TIEMPO_RAMPA_MS>1000)TIEMPO_RAMPA_MS=1000;
        Serial.print("  >> RAMPA=");Serial.print(TIEMPO_RAMPA_MS);Serial.println("ms"); break;
      case '-': if(TIEMPO_RAMPA_MS>50)TIEMPO_RAMPA_MS-=50;
        Serial.print("  >> RAMPA=");Serial.print(TIEMPO_RAMPA_MS);Serial.println("ms"); break;
      case '0': TIEMPO_RAMPA_MS=0; Serial.println("  >> RAMPA=0 (sin rampa)"); break;
      case 'v':
        Serial.println("\n  === VALORES ===");
        Serial.print("  VEL=");Serial.print(VELOCIDAD_BASE);
        Serial.print(" T_MOV=");Serial.print(TIEMPO_MOVIMIENTO);Serial.println("ms");
        Serial.print("  RAMPA=");Serial.print(TIEMPO_RAMPA_MS);Serial.println("ms");
        Serial.print("  PID Kp=");Serial.print(Kp,1);Serial.print(" Ki=");Serial.print(Ki,2);Serial.print(" Kd=");Serial.println(Kd,1);
        Serial.print("  BOOST: >"); Serial.print(UMBRAL_BOOST,0);Serial.print("° x");Serial.println(FACTOR_BOOST,1);
        Serial.print("  MAX_CORR=");Serial.println(MAX_CORRECCION,0);
        Serial.println("  ================\n"); break;
    }
  }
}

bool botonPresionado() {
  bool e=(digitalRead(PIN_BOTON)==HIGH);
  if(e&&!botonAnterior){botonAnterior=e;delay(50);return true;}
  botonAnterior=e; return false;
}
void esperarSoltarBoton() { while(digitalRead(PIN_BOTON)==HIGH)delay(10); delay(50); botonAnterior=false; }

// === SETUP ===
void setup() {
  Serial.begin(19200); delay(500);
  Serial.println("\n****************************************************");
  Serial.println("*  MOVIMIENTO CON RAMPAS + PID BOOST               *");
  Serial.println("*  Errores >10° se corrigen 2.5x más rápido        *");
  Serial.println("*  Robot: ROBOT 2 (delantero)                       *");
  Serial.println("****************************************************\n");
  Serial.println("CMD: +/-=rampa  0=sin rampa  v=valores\n");
  Serial.print("RAMPA=");Serial.print(TIEMPO_RAMPA_MS);Serial.print("ms ");
  Serial.print("Kp=");Serial.print(Kp,1);
  Serial.print(" BOOST: >");Serial.print(UMBRAL_BOOST,0);Serial.print("° x");Serial.println(FACTOR_BOOST,1);

  pinMode(LED_BUILTIN,OUTPUT); digitalWrite(LED_BUILTIN,LOW);
  pinMode(PIN_BOTON,INPUT);
  pinMode(INA1,OUTPUT);pinMode(INB1,OUTPUT);pinMode(PWM1,OUTPUT);
  pinMode(INA2,OUTPUT);pinMode(INB2,OUTPUT);pinMode(PWM2,OUTPUT);
  pinMode(INA3,OUTPUT);pinMode(INB3,OUTPUT);pinMode(PWM3,OUTPUT);
  parar();

  bnoOK=inicializarGyro();
  if(!bnoOK){Serial.println("*** ERROR BNO055 ***");
    while(true){digitalWrite(LED_BUILTIN,HIGH);delay(100);digitalWrite(LED_BUILTIN,LOW);delay(100);}}

  Serial.println("\nBOTON -> Test heading -> BOTON -> Loop adelante/atrás\n");
}

// === LOOP ===
void loop() {
  static unsigned long ultimoPrint = 0;
  procesarComandoSerial();

  switch (estadoTest) {
    case ESPERANDO_INICIO:
      if(botonPresionado()){esperarSoltarBoton();estadoTest=TEST_HEADING;Serial.println(">>> TEST HEADING <<<");}
      break;

    case TEST_HEADING:
      if(millis()-ultimoPrint>200){
        float h=leerHeading();
        Serial.print("Heading: ");if(h>=0)Serial.print("+");Serial.print(h,1);Serial.println("°");
        digitalWrite(LED_BUILTIN,abs(h)<10?HIGH:(millis()/200)%2); ultimoPrint=millis();
      }
      if(botonPresionado()){esperarSoltarBoton();estadoTest=ESPERANDO_LOOP;
        Serial.println("\nBOTON para LOOP (+/- para rampa en vivo)\n");}
      break;

    case ESPERANDO_LOOP:
      if(botonPresionado()){esperarSoltarBoton();cicloActual=1;
        Serial.println("***** LOOP *****\n>>> CICLO 1: ADELANTE <<<");
        resetPID();tiempoInicioTest=millis();estadoTest=LOOP_ADELANTE;}
      break;

    case LOOP_ADELANTE: {
      unsigned long t=millis()-tiempoInicioTest;
      float rampa=calcularFactorRampa(t,TIEMPO_MOVIMIENTO);
      moverAdelante(VELOCIDAD_BASE,rampa);
      digitalWrite(LED_BUILTIN,HIGH);
      if(millis()-ultimoPrint>200){
        float h=leerHeading(); int vel=(int)(VELOCIDAD_BASE*rampa);
        Serial.print("  [ADEL] H:");if(h>=0)Serial.print("+");Serial.print(h,1);
        Serial.print("° vel=");Serial.print(vel);Serial.print(" r=");Serial.print(rampa,2);
        if(abs(h)>UMBRAL_BOOST)Serial.print(" [BOOST]");
        Serial.print(" t=");Serial.print(t/1000.0,1);Serial.println("s");
        ultimoPrint=millis();
      }
      if(t>=TIEMPO_MOVIMIENTO){parar();Serial.println("  -> Pausa");
        tiempoInicioTest=millis();estadoTest=LOOP_PAUSA_1;}
      break;
    }

    case LOOP_PAUSA_1:
      parar(); digitalWrite(LED_BUILTIN,(millis()/100)%2);
      if(millis()-tiempoInicioTest>=PAUSA_ENTRE_MOVIMIENTOS){
        Serial.println(">>> CICLO "+String(cicloActual)+": ATRAS <<<");
        resetPID();tiempoInicioTest=millis();estadoTest=LOOP_ATRAS;}
      break;

    case LOOP_ATRAS: {
      unsigned long t=millis()-tiempoInicioTest;
      float rampa=calcularFactorRampa(t,TIEMPO_MOVIMIENTO);
      moverAtras(VELOCIDAD_BASE,rampa);
      digitalWrite(LED_BUILTIN,LOW);
      if(millis()-ultimoPrint>200){
        float h=leerHeading(); int vel=(int)(VELOCIDAD_BASE*rampa);
        Serial.print("  [ATRAS] H:");if(h>=0)Serial.print("+");Serial.print(h,1);
        Serial.print("° vel=");Serial.print(vel);Serial.print(" r=");Serial.print(rampa,2);
        if(abs(h)>UMBRAL_BOOST)Serial.print(" [BOOST]");
        Serial.print(" t=");Serial.print(t/1000.0,1);Serial.println("s");
        ultimoPrint=millis();
      }
      if(t>=TIEMPO_MOVIMIENTO){parar();Serial.println("  -> Pausa");
        tiempoInicioTest=millis();estadoTest=LOOP_PAUSA_2;}
      break;
    }

    case LOOP_PAUSA_2:
      parar(); digitalWrite(LED_BUILTIN,(millis()/100)%2);
      if(millis()-tiempoInicioTest>=PAUSA_ENTRE_MOVIMIENTOS){
        cicloActual++;Serial.println("\n>>> CICLO "+String(cicloActual)+": ADELANTE <<<");
        resetPID();tiempoInicioTest=millis();estadoTest=LOOP_ADELANTE;}
      break;
  }
}
