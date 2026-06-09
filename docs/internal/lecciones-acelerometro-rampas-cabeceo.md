---
title: "Lecciones aprendidas: Corrección de desplazamiento con acelerómetro y rampas"
date: 2026-04-03
author: "Claude (Anthropic - Claude Opus 4.6) bajo supervisión de María (IITA)"
ai-assisted: true
ai-tool: "Claude (Anthropic)"
status: final
tags: [acelerometro, bno055, cabeceo, rampas, pid, lecciones, movimiento]
robot: delantero
---

# Lecciones aprendidas: Corrección de desplazamiento con acelerómetro y rampas

## 1. EL PROBLEMA ORIGINAL

Durante las pruebas de movimiento lateral (derecha/izquierda) con giroscopio, se observó que el robot se desplazaba hacia adelante involuntariamente. Esto ocurría al arrancar los motores y al cambiar de dirección. El giroscopio corregía la rotación (heading) pero no la traslación (desplazamiento lineal).

El mismo efecto ("cabeceo") se observó en el movimiento adelante/atrás: al arrancar y frenar, el robot sufría un impulso que lo desplazaba perpendicularmente a la dirección de movimiento.

## 2. ENFOQUE 1: CORRECCIÓN CON ACELERÓMETRO (NO RECOMENDADO)

### 2.1 La idea

Usar el acelerómetro lineal del BNO055 (`VECTOR_LINEARACCEL`, que ya viene sin gravedad) para detectar desplazamientos no deseados y corregirlos con los motores usando cinemática omnidireccional.

### 2.2 Desarrollo (7 versiones)

Se desarrollaron 7 versiones iterativas del programa `test-correccion-acelerometro.ino`, cada una corrigiendo problemas encontrados en la anterior:

**v1**: Programa base. PID continuo sobre posición estimada del acelerómetro.
- **Problema encontrado**: Motor 2 tenía dirección invertida en hardware (INA2/INB2 al revés que M1 y M3). Las unidades estaban en metros, produciendo valores de PWM microscópicos (<1). Los motores nunca se activaban.

**v2**: Fix de motor2 invertido. Unidades cambiadas a cm/s².
- **Problema encontrado**: Los motores hacían ruido (zumbaban) pero no giraban. El PWM era menor al mínimo necesario para vencer la fricción estática (~40-50 PWM).

**v3 (fix)**: Ganancias subidas (Kp 3→15), PWM mínimo de motor agregado (45).
- **Problema encontrado**: Después de corregir, el robot no paraba. La vibración de los propios motores era detectada por el acelerómetro, creando un loop de retroalimentación infinito.

**v3 (fix decay)**: Decay de velocidad/posición cambiado de por-iteración a basado-en-tiempo con `pow(base, dt)`. El decay anterior (`vel *= 0.97` por iteración a ~1000Hz) mataba la velocidad instantáneamente: `0.97^1000 ≈ 0` por segundo.
- **Problema encontrado**: Ahora detectaba empujones pero la retroalimentación motores→acelerómetro seguía.

**v4**: Rediseño con máquina de estados: ESCUCHANDO → EMPUJANDO → CORRIGIENDO → PAUSA → ENFRIAMIENTO. El acelerómetro solo se lee con motores apagados, eliminando la retroalimentación.
- **Problema encontrado**: La dirección de corrección estaba invertida (empujaba para el mismo lado). Además, re-disparaba correcciones falsas por vibraciones residuales.

**v5**: Signo de corrección invertido (`-Kp*empujon` → `+Kp*empujon`). Estado ENFRIAMIENTO agregado (500ms ignorando acelerómetro post-corrección). Umbral de inicio subido a 30→40 cm/s².
- **Problema encontrado**: La corrección era desproporcionadamente fuerte. El robot giraba descontroladamente por el heading PID (L_ROTACION=0.6 generaba demasiada rotación).

**v6**: Ganancia bajada (Kp 15→6), L_ROTACION 0.6→0.15, PWM mínimo 45→35, tiempos de enfriamiento aumentados.
- **Resultado**: Funcionaba aceptablemente para empujones manuales fuertes. Dirección correcta, fuerza razonable.

**v7**: Heading PID activo en todos los estados (no solo durante corrección). PID completo con integral. Dos valores de L_ROTACION: 0.5 para heading solo, 0.2 para heading+traslación.
- **Resultado**: Mantiene frente + corrige empujones. Funcional como demostración.

### 2.3 Prueba integrada con movimiento

Se intentó integrar la corrección por acelerómetro en las pausas del programa de movimiento básico (adelante/atrás). El acelerómetro debía detectar el cabeceo al frenar y corregirlo.

**Resultado físico observado**: La corrección del acelerómetro fue contraproducente. En vez de mejorar la posición del robot, lo desacomodó aún más. Cuando arrancó el siguiente movimiento, el PID de heading tuvo que trabajar el doble para corregir la posición errónea que la "corrección" del acelerómetro había causado.

### 2.4 Problemas fundamentales del acelerómetro para esta aplicación

1. **Drift de integración**: Integrar aceleración dos veces (accel→vel→pos) acumula error rápidamente. Incluso con decay, la posición estimada driftea en segundos. No es confiable para saber "dónde estoy".

2. **Retroalimentación motores→sensor**: Los motores generan vibraciones que el acelerómetro detecta. Separar "movimiento propio" de "movimiento externo" requiere una lógica compleja de estados que agrega fragilidad.

3. **Mapeo de ejes sensor→robot**: La orientación del BNO055 respecto al robot no es trivial. Los signos de corrección tuvieron que descubrirse experimentalmente (el signo teórico `-Kp*pos` era incorrecto, se necesitó `+Kp*pos`).

4. **Proporcionalidad irreal**: La posición estimada por doble integración del acelerómetro no refleja el desplazamiento real con precisión suficiente. La corrección basada en esta estimación puede ser excesiva o insuficiente.

5. **Complejidad vs beneficio**: 7 versiones, múltiples bugs, parámetros difíciles de calibrar (umbrales de aceleración, tiempos de enfriamiento, ganancias, decay rates, PWM mínimos). El sistema es frágil y sensible a cambios en la superficie, batería, o peso del robot.

### 2.5 Veredicto: NO usar en competencia

**El acelerómetro del BNO055 NO es adecuado para corrección de posición en este robot.** Los problemas de drift, retroalimentación, y calibración lo hacen poco confiable. En competencia, donde las condiciones cambian (alfombra diferente, batería bajando, choques con otros robots), este sistema fallaría de formas impredecibles.

**El acelerómetro SÍ sirve para**:
- Detección de impactos/choques (umbral alto, sin integración)
- Detección de caídas o vuelcos (eje Z)
- Complementar el giroscopio para fusión de sensores (ya lo hace el BNO055 internamente)

**Para corrección de posición se necesitarían**: encoders en las ruedas (odometría), sensores ToF laterales, o visión por cámara. Estos dan información de posición directa, no derivada.

## 3. ENFOQUE 2: RAMPAS DE ACELERACIÓN/FRENADO (RECOMENDADO)

### 3.1 La idea

En vez de corregir el desplazamiento después de que ocurre, eliminarlo de raíz. El cabeceo ocurre porque los motores pasan de 0 a 150 PWM instantáneamente. La solución: subir y bajar la velocidad gradualmente (perfil trapezoidal).

### 3.2 Implementación

Una sola función `calcularFactorRampa()` que devuelve un factor de 0.0 a 1.0:
- Primeros N ms: sube linealmente de 0 a 1 (aceleración gradual)
- Medio del movimiento: se mantiene en 1 (velocidad constante)
- Últimos N ms: baja linealmente de 1 a 0 (frenado gradual)

La velocidad y la corrección PID se multiplican por este factor.

### 3.3 Resultado físico observado

Con rampa de 400ms, el cabeceo se redujo notablemente. El robot arranca y frena sin el impulso brusco que causaba el desplazamiento lateral.

### 3.4 Ventajas sobre el acelerómetro

- **Simple**: 15 líneas de código vs 500+
- **Robusto**: no depende de sensores adicionales ni calibración fina
- **Predecible**: el comportamiento es determinístico
- **Sin retroalimentación**: no hay loop sensor→motor→sensor
- **Ajustable en vivo**: un solo parámetro (`TIEMPO_RAMPA_MS`) controlable por Serial

### 3.5 Calibración

- El usuario probó rampas entre 300ms y 400ms
- Se puede comparar en vivo mandando `0` (sin rampa) vs `+` (más rampa)
- Regla: la rampa no debería superar 1/3 del tiempo de movimiento

## 4. MEJORA ADICIONAL: PID CON BOOST NO-LINEAL

### 4.1 Problema

Cuando una fuerza externa desviaba mucho el heading del robot (empujón de otro robot, choque), el PID con Kp=3.0 tardaba en corregir.

### 4.2 Solución

Agregar un multiplicador cuando el error supera un umbral:

```cpp
if (abs(error) > 10.0) {    // más de 10 grados
    correccion *= 2.5;       // corrección 2.5x más fuerte
}
```

Esto mantiene la suavidad para errores chicos (funcionamiento normal) pero responde agresivamente a desvíos grandes (fuerzas externas). También se subió `MAX_CORRECCION` de 80 a 120 para darle rango al boost.

## 5. ARCHIVOS GENERADOS

| Archivo | Ubicación | Estado |
|---------|-----------|--------|
| `test-correccion-acelerometro.ino` | `staging/shared/test-correccion-acelerometro/` | v7 — Funcional como demo, NO para competencia |
| `test-movimiento-basico-con-correccion.ino` | `staging/shared/test-movimiento-basico-con-correccion/` | Rampas + PID boost — RECOMENDADO para competencia |

## 6. RESUMEN DE DECISIONES

| Decisión | Motivo |
|----------|--------|
| **NO usar acelerómetro para corrección de posición** | Drift, retroalimentación, complejidad, resultados físicos contraproducentes |
| **SÍ usar rampas de aceleración/frenado** | Simple, robusto, elimina la causa del cabeceo |
| **SÍ usar PID con boost no-lineal** | Corrección rápida de desvíos grandes sin afectar suavidad normal |
| **Mantener heading PID del programa original** | Kp=3, Ki=0.08, Kd=0.8 funcionan bien con las rampas |

## 7. RECOMENDACIÓN PARA COMPETENCIA

Usar el programa con **rampas + PID boost** como base para el movimiento del robot. La rampa de 300-400ms es suficiente para eliminar el cabeceo sin sacrificar demasiada velocidad de respuesta. El PID boost garantiza que choques con otros robots se corrijan rápido.

El acelerómetro queda descartado para corrección de posición, pero el código desarrollado (v7) puede servir como referencia futura si se implementa fusión de sensores con encoders u odometría.
