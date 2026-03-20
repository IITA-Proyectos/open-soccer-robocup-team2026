---
title: "Cambios BNO055: de NDOF a IMUPLUS con init mejorado"
date: 2026-03-20
author: "Claude (Anthropic) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: staging
tags: [giroscopo, bno055, imuplus, staging]
---

# Cambios BNO055: de NDOF a IMUPLUS con init mejorado

## Resumen

Estos son los cambios exactos a aplicar en los programas del delantero y el arquero para mejorar la inicialización del giróscopo BNO055.

**Documento de referencia**: `docs/internal/giroscopo-bno055-analisis-tecnico.md`

---

## Cambio 1 — Modo IMUPLUS (1 línea, CRITICO)

**Por qué**: El modo NDOF activa el magnetómetro, que es interferido por los motores DC del robot. IMUPLUS usa solo acelerómetro + giróscopo, eliminando la interferencia magnética.

**Riesgo**: Bajo. El heading ahora será relativo al encendido en vez de al norte magnético, que es exactamente lo que queremos.

### Buscar (en `setup()`):
```cpp
  if (!bno.begin()) {
    Serial.println("¡No se pudo encontrar el BNO055!");
    while (1);
  }
  bno.setExtCrystalUse(true);

 sensors_event_t event;
  bno.getEvent(&event);
  initialYaw   = event.orientation.x; // 0..360
```

### Reemplazar por:
```cpp
  // ---------- GIROSCOPO (IMUPLUS = sin magnetometro) ----------
  Serial.println("BNO055: iniciando en modo IMUPLUS...");
  unsigned long bno_inicio = millis();
  while (!bno.begin(Adafruit_BNO055::OPERATION_MODE_IMUPLUS)) {
    if (millis() - bno_inicio > 3000) {
      Serial.println("ERROR: BNO055 no detectado! Continuando sin giroscopo.");
      break;  // NO bloquear el robot
    }
    delay(100);
  }
  bno.setExtCrystalUse(true);

  // Esperar estabilizacion post-init
  delay(1000);

  // Esperar calibracion del giroscopo (robot QUIETO)
  Serial.println("BNO055: calibrando gyro (no mover!)...");
  bno_inicio = millis();
  while (millis() - bno_inicio < 2000) {
    uint8_t sys_cal, gyro_cal, accel_cal, mag_cal;
    bno.getCalibration(&sys_cal, &gyro_cal, &accel_cal, &mag_cal);
    if (gyro_cal >= 3) {
      Serial.println("BNO055: gyro calibrado!");
      break;
    }
    delay(100);
  }

  // Capturar heading inicial (promedio de 10 lecturas)
  float suma_yaw = 0;
  for (int j = 0; j < 10; j++) {
    sensors_event_t event;
    bno.getEvent(&event);
    suma_yaw += event.orientation.x;
    delay(20);
  }
  initialYaw = suma_yaw / 10.0;
  Serial.print("BNO055: listo. initialYaw = ");
  Serial.println(initialYaw);
```

### Aplicar en:
- `software/robot-delantero/definitivo-delantero` → función `setup()`
- `software/robot-arquero/definitivo-arquero_6-9-2026` → función `setup()`

---

## Cambio 2 — Fix `currentYaw` en arquero (SOLO ARQUERO, CRITICO)

**Por qué**: El arquero usa `currentYaw` (valor absoluto 0-360 del BNO055) en comparaciones que solo funcionan si el robot apunta al norte magnético al encender. Hay que usar `error` (normalizado a ±180).

### Buscar (en CENTRANDO_horario y CENTRANDO_antihorario):
```cpp
        if ( (millis()- millis_inicio_estado >= 5000) && ((currentYaw <= 10) or (currentYaw >= 350)))
```

### Reemplazar por:
```cpp
        if ( (millis()- millis_inicio_estado >= 5000) && (abs(error) <= 10))
```

### Buscar (en los chequeos de línea blanca del CENTRANDO):
```cpp
            if ((currentYaw <= 90) or (currentYaw >= 270))
```

### Reemplazar por:
```cpp
            if (abs(error) <= 90)
```

**Nota**: Este cambio aparece en múltiples lugares dentro de CENTRANDO_horario y CENTRANDO_antihorario. Buscar TODAS las ocurrencias.

---

## Cambio 3 — Eliminar `START_BYTE` no usado (AMBOS, trivial)

### Buscar:
```cpp
#define START_BYTE 0xAA;
```

### Acción: Eliminar la línea (tiene punto y coma extra en el #define y no se usa).

---

## Verificación post-cambio

Después de aplicar los cambios:

1. **Compilar**: Verificar que compila sin errores ni warnings
2. **Serial Monitor**: Verificar que muestra los mensajes de calibración
3. **Test estático**: Robot quieto, heading debe mantenerse en ~0°
4. **Test dinámico**: Girar el robot 90° y verificar que heading cambia ~90°
5. **Test con motores**: Encender motores y verificar que heading no salta erráticamente
