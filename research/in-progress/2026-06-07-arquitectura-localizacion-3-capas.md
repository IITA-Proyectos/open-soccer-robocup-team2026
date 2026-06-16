---
title: "Arquitectura de localización en cancha — 3 capas (Heading + XY + Visión)"
date-start: 2026-06-07
status: completed
owner: "Elias Viollaz (IITA Salta)"
last-updated: 2026-06-07
priority: P0
robot: ambos
area: control
tipo: diseño
related:
  - docs/firmware/FIRMWARE-PLACA-ARRIBA.md
  - journal/2026-05-31-localizacion-bno-tof-diseno-y-diags.md
  - research/in-progress/2026-05-25-localizacion-tof-imu-analisis.md
  - team-tasks/2026-05-25-task-034-decidir-arquitectura-localizacion-incheon.md
  - software/teensy/Soccer\ 2026/src/top/sensors_imu.cpp
  - software/teensy/Soccer\ 2026/src/top/sensors_tof.cpp
  - software/teensy/Soccer\ 2026/src/shared/localization.cpp
---

# Arquitectura de localización en cancha — 3 capas (Heading + XY + Visión)

> **TL;DR.** El robot conoce su posición en la cancha mediante tres capas
> complementarias. **Capa 1**: heading absoluto fusionando los 2× BNO055 en modo
> IMUPLUS (sin magnetómetro), con lógica en cascada que detecta golpes y descarta
> el sensor que saltó. **Capa 2**: posición XY fusionando los 2× OTOS (base
> continua rápida, en DOWN) con los 4× VL53L7CX ToF (corrección absoluta sin
> deriva, en TOP); el ToF corrige al OTOS cuando coinciden en < 15 cm, y se
> descarta si difieren más. **Capa 3**: cada vez que la cámara ve el arco rival,
> se resetea el XY eliminando la deriva acumulada. El resultado es un único
> `pos XY fusionada` que viaja como parte del `WORLD_SNAPSHOT` al CENTRAL
> cada 10 ms.

---

## Tabla de contenidos

1. [Capa 1 — Heading absoluto (BNO055 dual)](#1-capa-1--heading-absoluto-bno055-dual)
2. [Capa 2 — Posición XY (OTOS + ToF)](#2-capa-2--posición-xy-otos--tof)
3. [Capa 3 — Corrección por observación del arco (cámaras)](#3-capa-3--corrección-por-observación-del-arco-cámaras)
4. [Complementariedad entre capas](#4-complementariedad-entre-capas)

---

## 1. Capa 1 — Heading absoluto (BNO055 dual)

### Descripción

El heading absoluto es el dato más confiable de localización. Se obtiene con los
dos sensores BNO055 en modo **IMUPLUS**, que no usa el magnetómetro — descartado
porque los motores de los robots rivales generan interferencia magnética que lo
perturba.

Se usan ambos BNO055 con redundancia activa: si uno sufre un golpe, el
acelerómetro de ese sensor registra una aceleración brusca que corrompe su
estimación de orientación. El otro puede haber absorbido el impacto de forma
diferente y seguir midiendo correctamente.

> **Corrección 2026-06-09 (banco, i²c scan, commit 9da8e9e — TASK-207).** El diseño original
> ponía los 2 BNO en el mismo bus `Wire` (0x28 + 0x29). El **scan real de ROBOT2** mostró que
> los 2 BNO están en **buses SEPARADOS**, ambos en 0x28: uno en `Wire` (18/19, con los 4 ToF) y
> el otro en **`Wire2` (24/25)** solo. El de `Wire2` (sin ToF → sin contención) es el
> **PRIMARIO/confiable** (el que comparte bus con los ToF es el que se CONGELA = SECUNDARIO).
> Tabla actualizada abajo. (El 0x29-en-`Wire` del diseño viejo era la unidad RIGHT, que además
> resultó FALLADA — el robot corre con 1 BNO sano.)
>
> > ⚠️ **Corrección 2026-06-15 (banco, AMBOS robots R1 y R2 — confirmada por Gustavo).** NO existe
> > ningún BNO en 0x29; tampoco hay unidad "RIGHT" ni BNO fallado. Cada robot lleva **2× BNO055
> > sanos, AMBOS en 0x28**, en buses separados: PRIMARIO en `Wire2` (24/25), solo, sin contención;
> > SECUNDARIO en `Wire` (18/19), junto a los 4 ToF. El robot corre con los **2 BNO sanos**, no
> > con uno. (0x29 sigue siendo la dirección de fábrica de los ToF VL53L7CX, que se reasignan a
> > 0x2A–0x2D al enumerar — eso es correcto y no cambia.)

| Sensor | Bus | Dirección | Rol | Modo |
|--------|-----|-----------|-----|------|
| BNO055 PRIMARIO (confiable) | **Wire2** (24/25) | 0x28 | fuente preferida (sin ToF, no se congela) | IMUPLUS |
| BNO055 SECUNDARIO (respaldo) | Wire (18/19) | 0x28 | respaldo (comparte bus con los 4 ToF) | IMUPLUS |

### Lógica de fusión en cascada

Cada **10 ms** la placa TOP lee los dos sensores y toma una decisión en cascada:

```
BNO055 PRIMARIO              BNO055 SECUNDARIO
 Wire2(24/25)·0x28·IMUPLUS    Wire(18/19)·0x28·IMUPLUS
 (solo, no se congela)        (comparte bus con los 4 ToF)
         \                           /
          \                         /
           [   Calcular diff        ]
           [ |heading_izq - heading_der| ]
                       |
              diff < 2°? ──── sí ──→ Promedio (izq + der) / 2
                       |
                      no
                       |
              diff < 10°? ─── sí ──→ ¿calib iguales? (gyro 0–3)
                       |                       |
                      no                      sí → Promedio
                       |
             Mayor calib_gyro gana (ese sensor gana)
                       |
             Golpe detectado (diff > 10°)
             → ver historial últimos 100 ms
                       |
             El que menos varió es confiable
             abs(ahora – hace 100 ms) menor
                       |
             → heading_fusionado enviado a CENTRAL cada 10 ms
```

| Condición | Acción |
|-----------|--------|
| `diff < 2°` — ambos coinciden | Promedio `(izq + der) / 2` |
| `2° ≤ diff < 10°` — calib iguales | Promedio `(izq + der) / 2` |
| `2° ≤ diff < 10°` — calib distintas | Gana el de mayor `calib_gyro` |
| `diff ≥ 10°` — golpe | Buscar en historial 100 ms; descartar el que más varió |

---

## 2. Capa 2 — Posición XY (OTOS + ToF)

### Descripción

La posición XY combina dos fuentes con características opuestas y complementarias:

- **OTOS (U5 y U6, en DOWN):** rápidos, continuos, no se confunden con robots,
  pero acumulan deriva con el tiempo.
- **4× VL53L7CX ToF (en TOP, Wire · 0x2A–0x2D):** posición absoluta sin deriva,
  pero pueden fallar si un robot rival bloquea el rayo hacia la pared.

La placa **DOWN** mide el desplazamiento continuo con los dos OTOS y lo envía al
TOP. La placa **TOP** toma esos datos, los combina con las lecturas de los 4 ToF
corregidas por el heading de la Capa 1, y produce una posición XY más confiable
que cualquiera de los dos sensores por separado.

### Diagrama de fusión

```
OTOS U5          OTOS U6          4× ToF VL53L7CX        Heading
Wire · DOWN      Wire1 · DOWN     Wire · TOP 0x2A–0x2D   Capa 1 · fusionado
     \               /                    |                    |
      \             /                     |                    |
   ¿U5 y U6 coinciden?              Compensar heading
     diff XY < 3 cm               dist × cos(heading)
           |                               |
          sí → Promedio XY          Validar grilla 8×8
          (U5 + U6) / 2          >40 zonas iguales = pared
           |                               |
          no                        ¿Lectura válida?
          ↓                      sigma bajo + es pared
     Menor sigma gana                      |
     (uno patinó, ignorar)                sí
           |                               |
           ↓                        pos_tof XY
       pos_otos XY             (absoluta, sin deriva)
      (base continua)                      |
               \                          /
                \                        /
             ¿ToF y OTOS coinciden?
                 diff XY < 15 cm
                       |
                      sí → Corregir OTOS con ToF
                      |    (elimina deriva acumulada)
                      no → Mantener OTOS
                            (ToF bloqueado ese ciclo)
                       |
                pos XY fusionada
             → WORLD_SNAPSHOT → CENTRAL

ciclo 10 ms · DOWN calcula OTOS · TOP calcula ToF y fusiona
```

### Lógica de las dos ramas

**Rama OTOS (izquierda):** los dos sensores se validan entre sí. Si difieren
menos de 3 cm se promedian. Si difieren más (uno patinó), se usa el de menor
sigma.

**Rama ToF (derecha):**
1. La lectura cruda se compensa con el heading de la Capa 1 para proyectar la
   distancia perpendicular real a la pared: `dist_real = dist × cos(heading_offset)`.
2. Se valida la grilla 8×8 del VL53L7CX: si más de 40 zonas coinciden, hay pared
   sólida (no un robot).
3. Si la lectura es limpia → `pos_tof XY` (absoluta, sin deriva).
4. Si no → se descarta ese ciclo.

**Fusión final:**

| Condición | Acción |
|-----------|--------|
| `\|pos_tof − pos_otos\| < 15 cm` | ToF corrige al OTOS, eliminando la deriva acumulada |
| `\|pos_tof − pos_otos\| ≥ 15 cm` | Hay algo bloqueando; se continúa con OTOS solo ese ciclo |

---

## 3. Capa 3 — Corrección por observación del arco (cámaras)

Cada vez que una de las cámaras ve el arco rival, el robot conoce exactamente su
distancia y ángulo a ese arco. Como las dimensiones del campo son fijas y
conocidas, eso se convierte en posición absoluta dentro del campo.

Con esa posición absoluta se **resetea el XY** de la Capa 2: se elimina la deriva
acumulada y se vuelve a empezar desde un punto conocido y confiable.

---

## 4. Complementariedad entre capas

Las tres capas operan en paralelo y se refuerzan mutuamente:

| Capa | Fuentes | Qué aporta | Limitación principal |
|------|---------|------------|----------------------|
| **Capa 1** | 2× BNO055 (TOP) | Heading absoluto, 100 Hz, redundante | Solo orientación, no posición |
| **Capa 2** | 2× OTOS (DOWN) + 4× ToF (TOP) | Posición XY continua, corrección absoluta | Deriva OTOS; ToF falla con robots bloqueando |
| **Capa 3** | Cámaras OpenMV N6 (TOP) | Reset absoluto XY cuando ve el arco | Solo disponible cuando el arco es visible |

> **Capa 1** siempre sabe hacia dónde apunta el robot.  
> **Capa 2** sabe dónde está el robot en el corto plazo.  
> **Capa 3** corrige la Capa 2 cada vez que hay una referencia visual disponible.
