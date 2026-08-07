# ⚡ Kicker Solenoid Optimization

## Resumen
Skill enfocado en el diseño, optimización y disparo de pateadores basados en solenoides (electromagnetos lineales) para el Striker. Un buen pateador es la diferencia entre un gol asegurado y un remate débil interceptable.

## Principios Físicos
El pateador almacena energía eléctrica en capacitores a alto voltaje y la descarga repentinamente a través de una bobina (solenoide), creando un fuerte campo magnético que succiona o repele un núcleo ferromagnético (plunger) que golpea la pelota.

$Energia = \frac{1}{2} C V^2$

## 1. Diseño de Hardware (Precaución: ¡Alto Voltaje!)

> [!CAUTION]
> El circuito de carga maneja voltajes entre 50V y 400V DC. Esto es suficiente para causar lesiones severas. Solo personal capacitado debe ensamblar el hardware y **siempre** incluir resistencias de purga (bleeder resistors) para descargar los capacitores al apagar.

### Componentes:
- **Boost Converter (Elevador)**: Convierte los 12V-16V de la LiPo a 250V+. Ej: Módulo DC-DC ZVS.
- **Capacitores**: Capacitores electrolíticos photoflash (ej. 2x 330uF 330V en paralelo).
- **Solenoides**: Usualmente custom bobinados, o versiones comerciales modificadas. Núcleo de hierro dulce (soft iron).
- **Interruptor (Switching)**: Un **IGBT** (Insulated-Gate Bipolar Transistor) o un MOSFET de alto voltaje/alta corriente. No usar relés mecánicos, se soldarán los contactos. (Ej: RJP4301).
- **Diodo Flyback**: Crítico. Diodo Schottky rápido en antiparalelo a la bobina para absorber los picos de voltaje inducido al apagar el IGBT.

## 2. Control por Software (Modulación del Pulso)

El control del microcontrolador hacia la compuerta (Gate) del IGBT debe ser a través de un optoacoplador para aislar la lógica de control del alto voltaje.

### Disparo Controlado
El alcance del tiro se regula mediante el **ancho del pulso** (tiempo que el IGBT está activado). Típicamente entre 2ms y 15ms.
Pulsos más largos no dan más fuerza (el núcleo satura magnéticamente y físicamente hace tope), solo generan exceso de calor destructivo.

```cpp
void fire_kicker(int power_percentage) {
    int max_time_ms = 12; // Tiempo límite seguro
    int min_time_ms = 3;  // Tiempo para "pase corto"
    
    // Mapear porcentaje (0-100%) a milisegundos
    int kick_time = map(power_percentage, 0, 100, min_time_ms, max_time_ms);
    
    digitalWrite(KICKER_PIN, HIGH);
    delay(kick_time);
    digitalWrite(KICKER_PIN, LOW);
    
    // Bloquear intentos inmediatos (Cooldown)
    last_kick_time = millis();
}
```

## 3. Consideraciones Tácticas
- **Autopase**: Disparos de muy baja potencia hacia las paredes para evadir defensores usando la geometría de la cancha.
- **Monitoreo de Carga**: Medir la tensión de los capacitores con un divisor de tensión (con altísima resistencia, >1Mohm) leído por un pin ADC. No patear si el voltaje no es suficiente, o compensar el tiempo del pulso si el voltaje es menor al objetivo.
