> ⚠️ **STAGING CONGELADO (2026-06-03) — NO subir más material a `software/staging/`.**
> Antes de tocar o agregar algo, leé **`software/staging/up_board/00-LEER-PRIMERO-recomendaciones-reuso.md`**.
> Este scratch repite bugs ya resueltos. Usá el stack de PRODUCCIÓN testeado en
> `software/teensy/Soccer 2026/src/` (ver el "mapa de reúso" del documento).

---
title: "Fix Rampa de Pateo: Reset de velocidadActualPateo"
date: 2026-03-20
author: "Claude (Anthropic) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: staging
tags: [pateo, rampa, bug, staging, delantero]
---

# Fix Rampa de Pateo: Reset de velocidadActualPateo

## El problema

El delantero tiene una función `avanzar_patear()` que incrementa la velocidad gradualmente (rampa de aceleración) para que las ruedas no patinen al patear:

```cpp
int velocidadActualPateo = 0;  // arranca en 0

void avanzar_patear() {
    if (velocidadActualPateo < velocidadFinalPateo) {
        velocidadActualPateo += pasoPateo;  // sube de a 5
    }
    // aplica velocidadActualPateo a los motores
}
```

El problema es que `velocidadActualPateo` **nunca se resetea a 0** cuando termina una patada. Entonces:
- Primera patada: rampa correcta, 0 → 5 → 10 → ... → 240 ✅
- Segunda patada: arranca en 240 inmediatamente, sin rampa ❌
- Tercera patada: igual, 240 instantáneo ❌

La rampa de aceleración **solo funciona la primera vez** en todo el partido.

## La solución

Resetear `velocidadActualPateo = 0` al entrar a cada estado de pateo. Esto se hace en las transiciones, no en la función.

---

## Cambio (SOLO DELANTERO — el arquero no tiene rampa)

### Buscar: PATEANDO_adelante (patada larga)
```cpp
    case PATEANDO_adelante:
      avanzar_patear();
      if (millis() - millis_inicio_estado >= 500) 
```

### Reemplazar por:
```cpp
    case PATEANDO_adelante:
      if (millis_inicio_estado == millis()) velocidadActualPateo = 0; // resetear rampa
      avanzar_patear();
      if (millis() - millis_inicio_estado >= 500) 
```

**Nota**: La condición `millis_inicio_estado == millis()` no es confiable porque millis() cambia. Mejor opción:

### Buscar: las transiciones que llevan a PATEANDO_adelante

Hay 2 lugares donde se entra a `PATEANDO_adelante`:

**Lugar 1 — desde PATEANDO_pausa_inicial:**
```cpp
    case PATEANDO_pausa_inicial:
      parar();
      if (millis() - millis_inicio_estado >= 1000) 
      {
        estado = PATEANDO_adelante;
        millis_inicio_estado = millis();
      }  
```

Agregar reset antes del cambio de estado:
```cpp
    case PATEANDO_pausa_inicial:
      parar();
      if (millis() - millis_inicio_estado >= 1000) 
      {
        velocidadActualPateo = 0; // resetear rampa
        estado = PATEANDO_adelante;
        millis_inicio_estado = millis();
      }  
```

**Lugar 2 — PATEANDO_corto_adelante (patada corta):**
```cpp
    case PATEANDO_corto_pausa_inicial:
      parar();
      if (millis() - millis_inicio_estado >= 500) 
      {
        estado = PATEANDO_corto_adelante;
        millis_inicio_estado = millis();
      }  
```

Agregar reset:
```cpp
    case PATEANDO_corto_pausa_inicial:
      parar();
      if (millis() - millis_inicio_estado >= 500) 
      {
        velocidadActualPateo = 0; // resetear rampa
        estado = PATEANDO_corto_adelante;
        millis_inicio_estado = millis();
      }  
```

**Lugar 3 — AVANCE_INICIO (avance inicial que usa avanzar_patear):**
```cpp
    case AVANCE_INICIO: 
      avanzar_patear();
```

Este es el primer uso, `velocidadActualPateo` arranca en 0 globalmente, así que está bien. Pero por seguridad, agregar al inicio del setup:

```cpp
  velocidadActualPateo = 0; // asegurar que la rampa arranque limpia
  millis_inicio_estado = millis();
```

### Resumen de cambios

Son **2 líneas agregadas** en el delantero:

| Ubicación | Línea a agregar |
|-----------|----------------|
| `case PATEANDO_pausa_inicial:` antes de `estado = PATEANDO_adelante;` | `velocidadActualPateo = 0;` |
| `case PATEANDO_corto_pausa_inicial:` antes de `estado = PATEANDO_corto_adelante;` | `velocidadActualPateo = 0;` |

### Aplicar en:
- `software/robot-delantero/definitivo-delantero` únicamente
- El arquero usa `avanzar_patear()` con velocidad fija (sin rampa), así que no aplica

---

## Cómo verificar que funciona

1. Subir programa con el fix al delantero
2. Hacer que el robot patee la pelota (primera patada)
3. Poner la pelota de nuevo y hacer que patee de nuevo (segunda patada)
4. **Observar**: ¿la segunda patada tiene la misma rampa de aceleración que la primera?
5. **Si funciona**: ambas patadas deberían verse y sonar igual (aceleración progresiva)
6. **Si NO funciona**: la segunda patada sería más brusca (arranca a máxima velocidad)

## Riesgo

Mínimo. Son 2 líneas que setean una variable a 0. No cambian ninguna lógica de comportamiento, solo aseguran que la rampa arranque de cero cada vez.
