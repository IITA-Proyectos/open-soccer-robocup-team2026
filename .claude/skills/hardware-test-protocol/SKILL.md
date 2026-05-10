---
name: hardware-test-protocol
description: Use when designing or executing a hardware test for any robot subsystem (mechanical part fit, PCB power-on, sensor calibration, motor/driver behavior, comm link latency, FSM integration, full match scrimmage). Defines test setup, measurable acceptance criteria, regression checks on neighbor subsystems, and journal documentation format. This is the "test plan in real hardware" that the vibe-* skills and rcj-soccer-coach reference.
---

# Hardware Test Protocol — Cómo se Prueba un Cambio en el Robot

> **Status: outline only — content pending iteration.**

## When to use

- Después de imprimir una pieza nueva (encaje, juego, interferencia).
- Después de fabricar/soldar una placa o cambiar firmware que toca pines.
- Después de calibrar sensor (IR línea, IR pelota, IMU, cámara).
- Después de cambiar parámetros de motor (PWM, PID).
- Después de integrar dos subsistemas que antes funcionaban por separado.
- Antes de partido real (scrimmage de pretemporada).

## When NOT to use

- Cambios sólo de documentación / playbooks.
- Refactor cosmético sin cambio funcional.
- Trabajo en simulación pura (decirlo explícito en la doc).

## Estructura de un test plan (obligatoria)

Toda propuesta de cambio que toque hardware se acompaña de este bloque:

```
### Test plan — [nombre del cambio]

**Subsistema afectado:** [mecánica | electrónica | sensor | motor | comm | visión | integración]
**Robot:** [arquero | delantero | ambos]
**Setup necesario:**
- Hardware: [robot ensamblado, batería cargada, banco de pruebas, cancha, …]
- Firmware version: [git hash o tag]
- Condiciones: [iluminación, temperatura, suelo, …]

**Pasos:**
1. [acción concreta — "alimentar 12V, medir corriente en reposo"]
2. [acción concreta — "comandar motor 50% PWM, medir velocidad rueda con encoder/cronómetro"]
3. ...

**Criterio de aceptación (medible):**
- [valor objetivo con tolerancia — "velocidad 0.8 m/s ± 10%"]
- [comportamiento observable — "robot mantiene heading ± 5° durante 10s"]

**Regression check (subsistemas vecinos):**
- [verificar que X sigue funcionando — "compilación firmware completa sin warnings nuevos"]
- [verificar que Y no se degradó — "motores aún responden a comandos de DriveBase"]

**Documentación esperada (journal entry):**
- Foto/video del setup.
- Datos crudos (CSV/log si aplica).
- Comparación antes/después (cuando aplica).
- Lo que falló y por qué (si falló).
```

## Patrones por subsistema (planned content)

[TODO: desarrollar cada uno con datos reales del lab IITA]

### Mechanical fit test
- Imprimir → encajar a mano → medir clearance con calibre o feeler gauge.
- Atornillar → verificar torque y sin deformación.
- Encajar en assembly completo (no solo aislado).

### Motor calibration
- Banco: medir RPM vs PWM (curva completa, ambos sentidos).
- Robot: medir velocidad rueda con encoder o stopwatch + cinta métrica.
- Verificar reverso simétrico.
- Stall current measurement (¿hace tropezar el regulador?).

### Sensor calibration
- **IR línea:** medir lectura sobre carpet vs línea blanca en 5 puntos.
- **IR pelota:** medir respuesta vs distancia (10/20/30/50/100 cm) por cada sensor del array.
- **IMU BNO055:** verificar drift en estático 30 segundos. Probar bajo presencia de motor / corriente alta (interferencia magnética).
- **Cámara OpenMV:** ver `openmv-vision-tuning` skill — protocolo dedicado.

### Comm link
- UART OpenMV↔Teensy: medir packet loss en 60s a 10Hz.
- ESP-NOW inter-robot: medir latencia con timestamp roundtrip (objetivo < 20ms).
- Módulo árbitro RCJ: verificar start/stop reactivo (< 200ms desde botón).

### FSM integration
- Forzar transición a cada estado y verificar acción esperada.
- Casos límite (ball lost, partner lost, ambos al mismo tiempo).
- Test bajo "ataque" (pelota oscilante, robot rival empujando).

### End-to-end scrimmage
- 2 robots vs 1 (handicap para encontrar bugs).
- 5 min reloj corrido.
- Logging completo + video.
- Una entrada en `journal/` por scrimmage, sí o sí.

## Anti-patterns (qué NO hacer)

- ❌ "Probar en cancha" sin criterio de aceptación.
- ❌ "Anda bien" como conclusión sin medición.
- ❌ Test solo en simulación cuando el cambio toca hardware.
- ❌ Probar solo el camino feliz (sin casos límite).
- ❌ No documentar fracasos. Los fracasos son los datos más valiosos.
- ❌ Saltarse regression check porque "es un cambio chico".
- ❌ Batería distinta a la real (siempre testar con batería de competencia, no fuente de banco).

## Equipment list (planned)

[TODO: confirmar con Gustavo qué hay en el lab IITA Salta]
- Banco de pruebas (mesa con plano de referencia).
- Multímetro / osciloscopio.
- Encoder de bench / tacómetro.
- Carpet de cancha (parche para sensores línea).
- Calibre digital.
- Cancha completa o parche representativo.
- Cámara para video (smartphone alcanza).
- Sistema de logging (serial monitor + CSV export).
- Batería de competencia + 1 backup cargado.

## References

- `journal/` — donde se documentan los resultados.
- `testing/protocols/` — protocolos formales reutilizables.
- `testing/results/YYYY-MM-DD-descripcion.md` — resultados con fecha.
- `AI-INSTRUCTIONS.md` sección 5.
- `engineering-journal` skill — formato de la entrada de journal post-test.
