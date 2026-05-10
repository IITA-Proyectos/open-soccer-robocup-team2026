---
name: vibe-mechanical-design
description: Use when designing or iterating mechanical parts for the robot (chassis, dribbler, kicker, sensor mounts, omni wheel hubs) using AI-accelerated CAD pipelines. Goes from sketch/intent to printable STL/STEP with verification gates (clearance, motor torque match, manufacturability, BOM check). Specialised for RoboCup-class robots with 3D-printed + manually-fabricated parts.
---

# Vibe Mechanical Design — Pipeline IA → Pieza Verificable

> **Status: outline only — content pending iteration.**

## When to use

- Diseño de chasis, dribbler, kicker (solenoide), mounts de sensores, hubs de ruedas omni.
- Modificación de piezas existentes en `hardware/mechanical/`.
- Generar STL/STEP listos para impresión 3D (PLA / PETG / ABS) o corte láser.
- Iteración rápida cuando hay deadline (testing → ajuste → reimpresión).

## When NOT to use

- Diseño electrónico (PCB) — usar `vibe-pcb-design`.
- Cambios mínimos a piezas existentes que se pueden hacer en 5 minutos en Tinkercad sin proceso completo.
- Diseño desde cero sin restricciones reales del robot (es academicismo).

## Pipeline esperado (planned content)

[TODO: desarrollar con piezas reales del robot 2026]

1. **Intent capture** — describir la pieza en palabras: función, restricciones, interfaces (qué se atornilla, qué pasa por adentro, qué se monta).
2. **Constraint extraction** — extraer dimensiones obligatorias:
   - Motor shaft (TT motor: ~5.5mm flat, hub estándar).
   - Screw holes (M3 / M4, depende del subsistema).
   - Clearance vs piezas vecinas (sensores IR, cámara, batería).
3. **Generación CAD** — IA genera código OpenSCAD / Fusion script / Tinkercad sketch.
4. **Verification gates antes de imprimir:**
   - Clearance check vs piezas vecinas (diferencia booleana en CAD).
   - Motor torque vs masa estimada (si es chasis o pieza estructural).
   - Imprimibilidad (overhangs > 45° → soportes obligatorios; orientación de capas).
   - Tolerancias de impresora (verificadas con calibración reciente).
   - BOM update si introduce hardware nuevo (tornillos, tuercas, insertos).
5. **Output** — STL/STEP en `hardware/mechanical/3d-prints/` con metadata:
   - Material recomendado.
   - Print orientation.
   - Infill (por defecto 30%, estructural 50%+).
   - Supports y/n.

## Verification gates (no negociable)

- **NUNCA** mandar a imprimir sin clearance check contra piezas vecinas.
- Calibración de impresora vigente (test cube reciente, < 2 semanas).
- Para piezas estructurales: print orientation y infill explícitos en metadata.
- Si la pieza interactúa con motor/sensor: verificar pinout/dimensiones con datasheet, no solo con foto.

## Tools assumed (planned)

[TODO: definir stack del equipo IITA]
- ¿Tinkercad (web)? → para piezas simples, accesible para alumnos.
- ¿Fusion 360 (free para students)? → para piezas paramétricas y assemblies.
- ¿OpenSCAD? → para piezas generadas por código (mejor con IA).
- ¿FreeCAD? → alternativa open source.

Mezcla razonable: Tinkercad para prototipos rápidos, Fusion para piezas finales con assemblies, OpenSCAD para piezas que vale parametrizar (escala, variantes).

## Pre-flight checklist

Antes de mandar a imprimir:
- [ ] STL exportado en mm (no en pulgadas).
- [ ] Mesh sin errores (no holes, no inverted normals).
- [ ] Clearance verificada contra ensamblaje completo.
- [ ] Print orientation decidida y documentada.
- [ ] Soportes evaluados (¿necesarios? ¿dónde se quitan?).
- [ ] Tiempo y material estimados (slicer).
- [ ] Backup de la versión anterior si es revisión.

## References

- `hardware/mechanical/` — diseños actuales del robot.
- `AI-INSTRUCTIONS.md` sección 7 — convenciones de hardware.
- Ruedas omni de referencia: estándar 3-omni configuración 120° entre ejes.
