// =============================================================================
// LEGACY - Temporada 2025
// Fuente: https://github.com/IITA-Proyectos/RoboCupJunior-Soccer-Open-League-2025
// Archivo original: /Arquero
// Autores originales: Equipo 2025 - Grupo Control/Movilidad
// Migrado por: Claude (Anthropic) el 2026-02-21
// =============================================================================

#include <Arduino.h>
#include <zirconLib.h>
#define TRIG 9
#define ECHO 10 

int direccion = 1; // 0 = derecha, 1 = izquierda

// NOMBRAR SENSORES 
int s1 = readLine(1); // Izquierda
int s2 = readLine(2); // Centro
int s3 = readLine(3); // Derecha


// FUNCION GIRAR 
// Giro no bloqueante
bool girando = false;
unsigned long tiempoInicioGiro = 0;
unsigned long duracionGiro = 0;
int direccionGiro = 0;

void girar(float angulo) 
{
    if (girando) return;  // Ya está girando, no reiniciar
    duracionGiro = abs(angulo * 43 / 200);
    tiempoInicioGiro = millis();
    girando = true;
    direccionGiro = (angulo > 0) ? 0 : 1;

    motor1(potencia, direccionGiro);
    motor2(potencia, direccionGiro);
    motor3(potencia, direccionGiro);
}

// FUNCIÓN PARA DETENER LOS MOTORES 
void detenerMotores()
{
    motor1(0, 0); motor1(0, 1);
    motor2(0, 0); motor2(0, 1);
    motor3(0, 0); motor3(0, 1);
    girando = false;
}

// FUNCION PARA ENCENDER IR HACIA ADELANTE POR UN TIEMPO (milisegundos) 
void Adelante(unsigned long tiempoEncendido) 
{
  static unsigned long inicio = 0;
  static bool activo = false;

  if (!activo) 
  {
    inicio = millis();
    activo = true;
    motor2(100, 1);
    motor3(100, 1);
  }

  if (activo && millis() - inicio >= tiempoEncendido) 
  {
    motor2(0, 0);
    motor3(0, 0);
    activo = false;
  }
}

long medirDistancia() 
{
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duracion = pulseIn(ECHO, HIGH);
  long distancia = duracion * 0.034 / 2;
  return distancia;
}

void setup(void) 
{
    Serial.begin(9600); 
    InitializeZircon(); 
    pinMode(TRIG, OUTPUT);
    pinMode(ECHO, INPUT);
}

void loop(void) 
{ 
  long d = medirDistancia();
  Serial.print("Distancia: ");
  Serial.print(d);
  Serial.println(" cm");
  
  // Finalizar giro si ya pasó el tiempo 
  if (girando && millis() - tiempoInicioGiro >= duracionGiro)
  {
      detenerMotores();
      girando = false; 
  }
    
  // NOTA: El código original continúa con lógica de oscilación en el arco
  // usando giróscopo + sensores de línea + ultrasonido.
  // Ver archivo original completo en el repo 2025.
}
