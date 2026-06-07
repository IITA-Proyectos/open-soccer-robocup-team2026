---
title: "Saber la Posición en la Cancha — Arquitectura de 3 Capas"
date: 2026-06-07
status: diseño
audience: equipo IITA Soccer Open
tags: [localizacion, bno055, tof, vl53l7cx, otos, imu, heading, fusion, pose, camara, top-board, down-board]
---

# Saber la Posición en la Cancha — Arquitectura de 3 Capas

> Documento de diseño del sistema de localización del robot. Define las tres capas
> que se complementan para estimar de forma robusta el heading absoluto y la
> posición XY del robot dentro del campo durante el juego.

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
dos sensores BNO055 operando en modo **IMUPLUS**, que no usa el magnetómetro
(descartado porque los motores de los robots rivales generan interferencia magnética
que lo perturba).

Se usan ambos BNO055 con redundancia activa: si uno sufre un golpe, el acelerómetro
de ese sensor genera una aceleración brusca que corrompe su estimación de orientación.
El otro sensor puede haber absorbido el impacto de forma diferente y seguir midiendo
correctamente.

- **BNO055 izquierdo:** Wire · 0x28 · IMUPLUS
- **BNO055 derecho:** Wire · 0x29 · IMUPLUS

### Diagrama de flujo de fusión

```
BNO055 izquierdo          BNO055 derecho
 Wire·0x28·IMUPLUS         Wire·0x29·IMUPLUS
        \                       /
         \                     /
          [  Calcular diff     ]
          [ |heading_izq - heading_der| ]
                    |
           diff < 2°? ──── sí ──→ Promedio (izq + der) / 2
                    |
                   no
                    |
           diff < 10°? ─── sí ──→ ¿calib iguales? (gyro 0-3)
                    |                     |
                   no                    sí → Promedio
                    |
          Mayor calib_gyro gana
                    |
          Golpe detectado (diff > 10°)
          ver historial últimos 100 ms
                    |
          El que menos varió es confiable
          abs(ahora - hace 100 ms) menor
                    |
          → heading_fusionado enviado a CENTRAL cada 10 ms
```

### Lógica de decisión en cascada

Cada **10 ms** la placa TOP lee los dos sensores y toma una decisión en cascada:

| Condición | Acción |
|-----------|--------|
| `diff < 2°` — ambos coinciden | Promedio simple `(izq + der) / 2` |
| `2° ≤ diff < 10°` — discrepancia menor, calib iguales | Promedio simple |
| `2° ≤ diff < 10°` — discrepancia menor, calib distintas | Gana el de mayor `calib_gyro` |
| `diff ≥ 10°` — golpe detectado | Buscar en historial 100 ms; descartar el que más varió |

El resultado es un único valor, **`heading_fusionado`**, que viaja por UART a la
placa CENTRAL en cada ciclo.

---

## 2. Capa 2 — Posición XY (OTOS + ToF)

### Descripción

El objetivo es conocer la posición XY del robot en coordenadas de cancha.
La arquitectura combina dos fuentes con características complementarias:

- **OTOS (U5 y U6 en DOWN):** rápidos, continuos, no se confunden con robots,
  pero acumulan deriva con el tiempo.
- **4× VL53L7CX ToF (en TOP, Wire · 0x2A–0x2D):** posición absoluta sin deriva,
  pero pueden fallar si un robot rival bloquea el rayo hacia la pared.

La placa **DOWN** mide el desplazamiento continuo con los dos OTOS y lo envía al
TOP. La placa **TOP** toma esos datos, los combina con las lecturas de los 4 ToF
corregidas por el heading de la Capa 1, y produce una posición XY más confiable
que cualquiera de los dos sensores por separado.

### Diagrama de flujo de fusión

```
OTOS U5         OTOS U6        4× ToF VL53L7CX       Heading
Wire · DOWN     Wire1 · DOWN   Wire · TOP 0x2A–0x2D   Capa 1 · fusionado
    \               /                  |                    |
     \             /                   |                    |
  ¿U5 y U6 coinciden?           Compensar heading
    diff XY < 3 cm              dist × cos(heading)
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
   (base continua)                     |
            \                         /
             \                       /
          ¿ToF y OTOS coinciden?
              diff XY < 15 cm
                    |
                   sí → Corregir OTOS con ToF
                   |    (elimina deriva acumulada)
                   no → Mantener OTOS
                         (ToF bloqueado)
                    |
             pos XY fusionada
          → WORLD_SNAPSHOT → CENTRAL

ciclo 10 ms · DOWN calcula OTOS · TOP calcula ToF y fusiona
```

### Lógica de fusión

El flujo tiene **dos ramas paralelas** que se fusionan al final:

**Rama OTOS (izquierda):**
Los dos sensores se validan entre sí. Si difieren menos de 3 cm se promedian.
Si difieren más (uno patinó), se usa el de menor sigma.

**Rama ToF (derecha):**
1. La lectura cruda se compensa con el heading de la Capa 1 para proyectar
   la distancia perpendicular real a la pared: `dist_real = dist × cos(heading_offset)`.
2. Se valida la grilla 8×8 del VL53L7CX: si más de 40 zonas coinciden, hay
   una pared sólida enfrente (no un robot).
3. Si la lectura es limpia → `pos_tof XY` (absoluta, sin deriva).
4. Si no → se descarta ese ciclo.

**Fusión final:**

| Condición | Acción |
|-----------|--------|
| `\|pos_tof - pos_otos\| < 15 cm` | ToF corrige el OTOS, eliminando la deriva acumulada |
| `\|pos_tof - pos_otos\| ≥ 15 cm` | Hay algo bloqueando; se continúa con OTOS solo ese ciclo |

---

## 3. Capa 3 — Corrección por observación del arco (cámaras)

### Descripción

Cada vez que una de las cámaras ve el arco rival, el robot conoce exactamente
su distancia y ángulo a ese arco. Como las dimensiones del campo son fijas y
conocidas, esa observación se convierte en posición absoluta dentro del campo.

Con esa posición absoluta se **resetea el XY** de la Capa 2: se elimina la deriva
acumulada y se vuelve a empezar desde un punto conocido y confiable.

---

## 4. Complementariedad entre capas

Las tres capas operan en paralelo y se refuerzan mutuamente:

| Capa | Fuente | Qué aporta | Limitación |
|------|--------|------------|------------|
| **Capa 1** | BNO055 × 2 | Heading absoluto, confiable, 100 Hz | Solo orientación, no posición |
| **Capa 2** | OTOS × 2 + ToF × 4 | Posición XY continua, sin deriva a corto plazo | Deriva OTOS a largo plazo; ToF falla con robots bloqueando |
| **Capa 3** | Cámaras OpenMV N6 | Reset absoluto XY cuando ve el arco | Solo disponible cuando el arco es visible |

> **Capa 1** siempre sabe hacia dónde apunta el robot.
> **Capa 2** sabe dónde está el robot en el corto plazo.
> **Capa 3** corrige la Capa 2 cada vez que hay una referencia visual disponible.
