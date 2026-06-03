> ⚠️ **STAGING CONGELADO (2026-06-03) — NO subir más material a `software/staging/`.**
> Antes de tocar o agregar algo, leé **`software/staging/up_board/00-LEER-PRIMERO-recomendaciones-reuso.md`**.
> Este scratch repite bugs ya resueltos. Usá el stack de PRODUCCIÓN testeado en
> `software/teensy/Soccer 2026/src/` (ver el "mapa de reúso" del documento).

---
title: "Fix UART: Sincronización robusta del protocolo OpenMV-Teensy"
date: 2026-03-20
author: "Claude (Anthropic) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: staging
tags: [uart, comunicacion, openmv, staging, confiabilidad]
---

# Fix UART: Sincronización robusta del protocolo OpenMV-Teensy

## El problema

El protocolo UART entre la OpenMV y el Teensy transmite 9 bytes:
```
[201][Xp][Yp][202][Xam][Yam][203][Xaz][Yaz]
```

El código actual hace esto:
```cpp
if (Serial1.available() >= 9) {
    header1 = Serial1.read();
    if (header1 == 201) {
        // lee 8 bytes más
    } else {
        // descarta UN byte, marca todo como no detectado
    }
}
```

**Qué falla**: Si llega un byte corrupto o se pierde un byte, el código descarta solo 1 byte pero los otros 8 quedan en el buffer. En el siguiente ciclo, lee datos desfasados (un valor Y como si fuera un header, un header como si fuera una coordenada, etc.).

**Síntomas en competencia**:
- La pelota "parpadea" (se detecta y se pierde aleatoriamente)
- El robot pierde la pelota cuando la tiene enfrente
- Coordenadas basura causan movimientos erráticos

**Problema adicional**: Si una coordenada vale 201, 202 o 203 (posible: ~201cm de distancia), el Teensy la confunde con un header.

## La solución

Dos cambios:
1. **En el Teensy**: Buscar el header descartando bytes basura con un while-peek
2. **En la OpenMV**: Clampear coordenadas a 1-200 para que nunca coincidan con headers

---

## Cambio 1 — Teensy: Lectura UART robusta (AMBOS ROBOTS)

### Buscar en `loop()` (bloque de lectura serial):
```cpp
  if (Serial1.available() >= 9)
  {
    header1 = Serial1.read();
    if (header1 == 201)
    {
      codedXp = Serial1.read();
      codedYp = Serial1.read();
      header2 = Serial1.read(); 
      codedXam = Serial1.read();
      codedYam = Serial1.read();
      header3 = Serial1.read(); 
      codedXaz = Serial1.read();
      codedYaz = Serial1.read(); 

      if (header1 == 201 && header2 == 202 && header3 == 203)
      {
        Xp = codedXp; 
        Yp = codedYp - 100; 
        Xam = codedXam; 
        Yam = codedYam - 100; 
        Xaz = codedXaz; 
        Yaz = codedYaz - 100; 

        // Calcular el ángulos
          anguloPelota = atan2(Yp, Xp) * 180.0 / PI;
          anguloArco_Amarillo = atan2(Yam, Xam) * 180.0 / PI;
          anguloArco_Azul = atan2(Yaz, Xaz) * 180.0 / PI;

        // Mostrar datos
  //      Serial.print("X pelota: "); Serial.print(Xp);
  //      Serial.print(" | Y pelota: "); Serial.print(Yp);
  //      Serial.print(" | X arco amarillo: "); Serial.print(Xam);
  //      Serial.print(" | Y arco amarillo: "); Serial.print(Yam);
  //      Serial.print(" | X arco azul: "); Serial.print(Xaz);
  //      Serial.print(" | Y arco azul: "); Serial.println(Yaz);


        if ( Xp == 0 )
        { haypelota = false; }
        else
        { 
          haypelota = true; 
          millis_pelota = millis();           
        }

        if ( Xam == 0 )
        { hayarco_amarillo = false; }
        else
        { hayarco_amarillo = true; }
        
        if ( Xaz == 0 )
        { hayarco_azul = false; }
        else
        { hayarco_azul = true; }
  
  //      Serial.print("pelota: "); Serial.print(haypelota); 
  //      Serial.print("| arco amarillo: "); Serial.print(hayarco_amarillo);
  //      Serial.print("| arco azul: "); Serial.println(hayarco_azul);
      }
    } 
    else
    {
     hayarco_azul= false;
     hayarco_amarillo=false;
     haypelota=false; 
    }  
  }
```

### Reemplazar por:
```cpp
  // --- LECTURA UART ROBUSTA ---
  // Descartar bytes basura hasta encontrar header 201
  while (Serial1.available() >= 9) {
    if (Serial1.peek() != 201) {
      Serial1.read(); // descartar byte basura
      continue;
    }

    // Leer paquete completo de 9 bytes
    header1 = Serial1.read(); // 201
    codedXp = Serial1.read();
    codedYp = Serial1.read();
    header2 = Serial1.read();
    codedXam = Serial1.read();
    codedYam = Serial1.read();
    header3 = Serial1.read();
    codedXaz = Serial1.read();
    codedYaz = Serial1.read();

    // Validar que los 3 headers son correctos
    if (header2 == 202 && header3 == 203) {
      // Paquete válido — decodificar
      Xp = codedXp;
      Yp = codedYp - 100;
      Xam = codedXam;
      Yam = codedYam - 100;
      Xaz = codedXaz;
      Yaz = codedYaz - 100;

      anguloPelota = atan2(Yp, Xp) * 180.0 / PI;
      anguloArco_Amarillo = atan2(Yam, Xam) * 180.0 / PI;
      anguloArco_Azul = atan2(Yaz, Xaz) * 180.0 / PI;

      if (Xp == 0) { haypelota = false; }
      else         { haypelota = true; millis_pelota = millis(); }

      if (Xam == 0) { hayarco_amarillo = false; }
      else           { hayarco_amarillo = true; }

      if (Xaz == 0) { hayarco_azul = false; }
      else           { hayarco_azul = true; }
    }
    // Si headers no matchean, el while descartará los bytes al buscar el próximo 201
    break; // procesar un paquete por ciclo de loop
  }
```

### Aplicar en:
- `software/robot-delantero/definitivo-delantero` → bloque de lectura serial en `loop()`
- `software/robot-arquero/definitivo-arquero_6-9-2026` → bloque de lectura serial en `loop()`

---

## Cambio 2 — OpenMV: Clampear coordenadas para evitar colisión con headers

### Buscar en el programa de visión (`enviar coordenadas 2 arcos y pelota`):
```python
    packet = [
        201, int(Xp), int(codedYp),
        202, int(Xam), int(codedYam),
        203, int(Xaz), int(codedYaz)
    ]
```

### Reemplazar por:
```python
    # Clampear coordenadas a 1-200 para que nunca coincidan con headers (201/202/203)
    # El valor 0 sigue significando "no detectado"
    def safe(val):
        if val == 0:
            return 0  # 0 = no detectado
        return max(1, min(200, int(val)))

    packet = [
        201, safe(Xp), safe(codedYp),
        202, safe(Xam), safe(codedYam),
        203, safe(Xaz), safe(codedYaz)
    ]
```

### Aplicar en:
- `software/vision/enviar coordenadas 2 arcos y pelota` → bloque de envío UART

---

## Cambio 3 — OpenMV: Quitar print() que reduce FPS (OPCIONAL)

### Buscar:
```python
    uart.write(bytearray(packet))
    print("Enviando:", packet)
```

### Reemplazar por:
```python
    uart.write(bytearray(packet))
    # print("Enviando:", packet)  # descomentar solo para debug
```

---

## Qué mejora concretamente

| Aspecto | Antes | Después |
|---------|-------|--------|
| Byte basura en buffer | Se desincroniza TODO | Se descarta byte por byte hasta resincronizar |
| Headers en medio de datos | Se confunde | Se validan los 3 headers (201+202+203) |
| Coordenada = 201/202/203 | Se confunde con header | Imposible (clampeado a 1-200) |
| Paquete corrupto | Marca todo como no detectado | Descarta silenciosamente y busca el siguiente |
| FPS de OpenMV | Reducido por print() cada frame | Sin overhead |

## Riesgo

Bajo. La lógica de decodificación (restar 100 a Y, calcular ángulos, detectar presencia) es idéntica. Solo cambia cómo se busca el inicio del paquete y cómo se valida.

El único riesgo es si el `while` tarda demasiado descartando bytes, pero el `break` al final asegura que se procesa un solo paquete por ciclo de `loop()`.
