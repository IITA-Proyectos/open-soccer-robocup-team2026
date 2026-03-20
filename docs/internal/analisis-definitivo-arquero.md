---
title: "Análisis del Programa Definitivo del Arquero"
date: 2026-03-20
author: "Claude (Anthropic) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: final
tags: [arquero, analisis, bugs, mejoras, confiabilidad]
---

# Análisis del Programa Definitivo del Arquero

## Archivos analizados

- `software/robot-arquero/definitivo-arquero_6-9-2026` (~900 líneas, 33KB)
- `software/vision/enviar coordenadas 2 arcos y pelota` (OpenMV, compartido con delantero)

## Resumen ejecutivo

El programa del arquero es un **clon casi completo del delantero** (~95% del código es idéntico) con el estado inicial cambiado a `impulso_inicial` (modo arquero) en vez de `AVANCE_INICIO` (modo delantero). Contiene los ~28 estados del delantero completos, más ~10 estados propios del arquero. Esto confirma que ambos programas se mantuvieron como un solo codebase donde se comenta/descomenta la parte activa.

El comportamiento del arquero es: oscilar lateralmente en la línea del arco propio, detectar pelota acercándose, interceptar moviéndose hacia ella, y patear para despejar. Es una estrategia defensiva sólida para la categoría.

---

## 1. ARQUITECTURA Y COMPORTAMIENTO

### 1.1 Flujo de estados del arquero

```
impulso_inicial (40ms)
    └─→ moverce_derecha ←──────────────────────┐
         ├─→ [ve pelota cerca y centrada] ──→ PATEANDO_pausa_inicial_arquero
         ├─→ [ve pelota a la izquierda] ──→ moverce_izquierda
         ├─→ [detecta línea blanca] ──→ impulso_izquierda (350ms) ─→ moverce_izquierda
         └─→ [no ve pelota] ──→ sigue oscilando (pd=1)

moverce_izquierda
         ├─→ [ve pelota cerca y centrada] ──→ PATEANDO_pausa_inicial_arquero
         ├─→ [ve pelota a la derecha] ──→ moverce_derecha
         ├─→ [detecta línea blanca] ──→ impulso_derecha (350ms) ─→ moverce_derecha
         └─→ [no ve pelota] ──→ sigue oscilando (pd=1)

PATEANDO_pausa_inicial_arquero (200ms)
    └─→ PATEANDO_adelante_arquero (450ms)
        └─→ PATEANDO_pausa_arquero (1000ms)
            └─→ PATEANDO_atras_arquero (hasta línea blanca)
                └─→ avanzar_despues_de_patear (1000ms)
                    └─→ moverce_derecha
```

### 1.2 Lo positivo

- **Oscilación lateral con corrección giroscópica**: Las funciones `aiproporcional()` y `adproporcional()` usan el `error` del BNO055 para mantener la dirección mientras se mueve lateralmente. Esto es inteligente — el arquero compensa el drift mientras patrulla.
- **Velocidad adaptativa al ver pelota**: Cuando ve la pelota, `pd` sube a 1.5 (50% más rápido) para interceptar. Cuando no la ve, `pd` vuelve a 1.
- **Impulsos anti-trabado**: Los estados `impulso_izquierda`/`impulso_derecha` (350ms de movimiento forzado) resuelven el problema documentado en el código: "en los costados se traba con el blanco, porque cambia de moverce izquierda a moverce derecha erráticamente".
- **Secuencia de pateo + retorno**: Después de patear, retrocede hasta la línea y luego avanza 1 segundo para reposicionarse. Esto es defensivamente correcto.
- **`tolerancia_cercania = 140`**: Más alta que el delantero (50). El arquero reacciona a la pelota desde más lejos, lo cual tiene sentido para intercepción defensiva.

### 1.3 Problemas arquitectónicos

- **Todo el código del delantero está presente**: Los estados AVANCE_INICIO, GIRANDO, AVANZANDO, CENTRANDO, etc. están completos pero **nunca se alcanzan** desde el flujo del arquero (que empieza en `impulso_inicial`). Son ~500 líneas de código muerto.
- **Mismo archivo monolítico de 900 líneas**: Misma deuda técnica que el delantero.

---

## 2. BUGS CRÍTICOS

### BUG 1 — Gap mortal en la lógica de Yp del arquero

```cpp
case moverce_derecha:
    if (haypelota) {
        if ((Xp <= tolerancia_cercania) && (abs(Yp) <= 3))  // patear
        { ... estado = PATEANDO_pausa_inicial_arquero; }

        if (abs(Yp) >= 5)  // moverse hacia la pelota
        { ... }
        else
        { parar(); }  // ← TRAMPA: si 3 < abs(Yp) < 5, el robot se PARA
    }
```

Cuando `abs(Yp)` está entre 3 y 5 (zona de "casi centrado pero no lo suficiente"), el robot **se para y no hace nada**. No patea (necesita <=3), no se mueve hacia la pelota (necesita >=5), y no cambia de estado. Se queda congelado mirando la pelota.

**Solución**: Unificar los umbrales — o usar <=5 para patear, o >=3 para moverse:
```cpp
if ((Xp <= tolerancia_cercania) && (abs(Yp) <= 5)) {
    estado = PATEANDO_pausa_inicial_arquero;
} else if (abs(Yp) > 5) {
    // moverse hacia la pelota
}
```

---

### BUG 2 — PATEANDO_atras_arquero sin timeout (igual que delantero)

```cpp
case PATEANDO_atras_arquero:
    // retrocede hasta línea blanca
    if ((s1 >= blanco1) or (s2 >= blanco2) or (s3 >= blanco3)) { ... }
    // ¡SIN TIMEOUT!
break;
```

Si los sensores de línea fallan, el robot retrocede para siempre hasta salir de la cancha.

**Solución**: Agregar timeout de 3000ms máximo.

---

### BUG 3 — Protocolo UART idéntico al delantero (sin sincronización robusta)

Mismo problema exacto que el delantero: desincronización por byte perdido, valores que coinciden con headers. Ver análisis completo en `analisis-definitivo-delantero.md`, BUG 1.

---

### BUG 4 — `currentYaw` usado RAW en lugar de `error` normalizado

En CENTRANDO_horario y CENTRANDO_antihorario:
```cpp
if ((millis() - millis_inicio_estado >= 5000) && ((currentYaw <= 10) or (currentYaw >= 350)))
```

Y en las decisiones de línea blanca:
```cpp
if ((currentYaw <= 90) or (currentYaw >= 270))
```

Estos usan `currentYaw` (valor absoluto 0-360 del BNO055) en vez de `error` (valor normalizado ±180 relativo al heading inicial). Si el robot se enciende apuntando a 90° respecto al norte magnético, `currentYaw <= 10` nunca va a ser true cuando el robot mira al frente.

**Esto solo funciona si el robot se enciende apuntando al norte magnético** (o muy cerca). En competencia, la orientación en la cancha es arbitraria.

El delantero tenía este tema resuelto mejor usando `abs(error) <= 1`.

**Solución**: Reemplazar todas las comparaciones de `currentYaw` por comparaciones de `error`:
```cpp
// En vez de: (currentYaw <= 10) or (currentYaw >= 350)
if (abs(error) <= 10)  // ←  "estoy mirando al frente"

// En vez de: (currentYaw <= 90) or (currentYaw >= 270)
if (abs(error) <= 90)  // ← "estoy mirando hacia el lado correcto"
```

---

### BUG 5 — `Ycontrincante` no existe en el arquero

El delantero usa `Ycontrincante = Yam` para referirse al arco al que hay que patear. El arquero usa `Yam` directamente en algunos lugares:
```cpp
if (ARCO_CONTRINCANTE && haypelota && (abs(Yp - Yam) <= tolerancia_centrado))
```

Pero `ARCO_CONTRINCANTE = hayarco_amarillo` está hardcodeado. Si el equipo cambia de lado en la cancha y el arco al que debe patear es el azul, hay que cambiar código en múltiples lugares.

**Solución**: Crear una variable unificada al inicio del loop:
```cpp
bool hayArcoObjetivo = hayarco_amarillo; // cambiar acá según el lado
float YarcoObjetivo = Yam;
```

---

### BUG 6 — Conflicto zirconLib (idéntico al delantero)

`InitializeZircon()` configura pines que no coinciden con los `#define` del programa. Ver análisis del delantero, BUG 4.

---

## 3. PROBLEMAS DE CONFIABILIDAD

### R1 — Arquero no tiene timeout global de oscilación

Los estados `moverce_derecha` y `moverce_izquierda` no tienen timeout. Si la pelota nunca aparece, el robot oscila lateralmente para siempre sin hacer nada más.

No es necesariamente un bug (un arquero *debería* oscilar esperando), pero si pierde comunicación con la OpenMV, se queda oscilando sin saber que la pelota está ahí.

**Sugerencia**: Agregar un chequeo periódico de que la comunicación UART está viva. Si no recibe paquetes por X segundos, intentar un comportamiento alternativo.

---

### R2 — Sensores de línea: s3 no se chequea en oscilación del arquero

```cpp
case moverce_derecha:
    if (s1 >= blanco1 or s2 >= blanco2)  // ← ¿y s3?
    { ... }
```

Solo se chequean `s1` y `s2` pero no `s3` (sensor derecho). Si el robot toca la línea por la derecha, no la detecta y sale de la cancha.

**Solución**: Agregar `or s3 >= blanco3` a los chequeos de los estados del arquero.

---

### R3 — `avanzar_patear()` del arquero no tiene rampa (a diferencia del delantero)

```cpp
// ARQUERO:
void avanzar_patear() {
    analogWrite(PWM1, patadM1);  // velocidad fija máxima inmediata
    analogWrite(PWM2, patadM2);
}
```

El delantero tiene rampa de aceleración progresiva, pero el arquero arranca a velocidad máxima. Esto puede hacer que las ruedas patinen y pierda tracción en el despeje.

**Nota**: Esto podría ser intencional (despeje rápido), pero vale la pena testear si la rampa mejora la efectividad del despeje.

---

### R4 — `avanzar()` sin corrección giroscópica (mismo que delantero)

Cuando el arquero usa `avanzar()` en el estado `avanzar_despues_de_patear`, va sin corrección de heading. Puede desviarse al volver a su posición.

---

### R5 — Pausa de 1000ms en PATEANDO_pausa_arquero

```cpp
case PATEANDO_pausa_arquero:
    parar();
    if (millis() - millis_inicio_estado >= 1000)  // 1 SEGUNDO parado
```

Después de patear, el robot se queda parado 1 segundo completo antes de retroceder. Esto es tiempo en que el rival puede recuperar la pelota y atacar al arco desprotegido.

**Sugerencia**: Reducir a 200-400ms.

---

### R6 — Naming inconsistente entre estados del arquero y del delantero

Estados del arquero: `impulso_inicial`, `moverce_derecha`, `impulso_izquierda` (minúsculas, con typos)
Estados del delantero: `AVANCE_INICIO`, `IMPULSO_INICIAL_GIRANDO`, `CENTRANDO_horario` (MAYÚSCULAS)

Esto dificulta saber qué estados pertenecen a cada rol.

---

## 4. COMPARACIÓN ARQUERO vs DELANTERO

| Aspecto | Delantero | Arquero | Observación |
|---------|-----------|---------|-------------|
| Estado inicial | `AVANCE_INICIO` | `impulso_inicial` | OK |
| tolerancia_cercania | 50 | 140 | Arquero reacciona de más lejos |
| Rampa de pateo | Sí (progresiva) | No (velocidad fija) | Posible mejora arquero |
| Corrección giro al avanzar | No (pero calculada) | Sí (en oscilación lateral) | Delantero debería usarla |
| Uso de `currentYaw` raw | Usa `error` normalizado | Mezcla ambos | **Bug en arquero** |
| Chequeo s3 en oscilación | N/A | Falta | **Bug en arquero** |
| Gap en umbral Yp | N/A | Sí (3 < Yp < 5) | **Bug en arquero** |
| Código del otro rol incluido | Sí (~200 líneas de arquero) | Sí (~500 líneas de delantero) | Código muerto |
| Variable `Ycontrincante` | Sí | No (usa `Yam` directo) | Inconsistencia |
| `velocidadActualPateo` | Bug (no se resetea) | No aplica (sin rampa) | Bug solo en delantero |

---

## 5. OPORTUNIDADES DE MEJORA DE DESEMPEÑO

### D1 — Posicionamiento con arco propio

El arquero oscila ciegamente entre las líneas blancas. Podría usar la cámara para detectar su propio arco y mantenerse centrado. Si ve el arco azul (propio) muy a la izquierda, sabe que está desplazado y puede corregir.

### D2 — Predicción de trayectoria de pelota

Actualmente el arquero solo reacciona a la posición Y de la pelota. Si guardara 2-3 frames de posición, podría predecir hacia dónde va la pelota y anticipar el movimiento lateral.

```cpp
// Pseudocódigo simple:
float Yp_anterior = 0;
float velocidad_pelota_Y = Yp - Yp_anterior;
float Yp_predicho = Yp + velocidad_pelota_Y * 3; // 3 frames adelante
Yp_anterior = Yp;
// Moverse hacia Yp_predicho en vez de Yp
```

### D3 — Despeje direccional

Actualmente el arquero siempre despeja recto (adelante). Si la pelota viene por un costado, podría girar ligeramente antes de patear para despejar hacia la banda en vez de al centro (donde puede quedar para el rival).

### D4 — Velocidad lateral diferenciada por zona

Cerca del centro del arco, el arquero podría moverse más lento y controlado. Cerca de los postes (cuando detecta líneas laterales), podría moverse más rápido para cubrir más terreno.

### D5 — Modo "emergencia" cuando pelota está muy cerca y centrada

Si `Xp < 30` (pelota a menos de 30cm, prácticamente encima), saltear la pausa de 200ms y patear inmediatamente.

---

## 6. RESUMEN DE PRIORIDADES

| # | Problema | Impacto | Esfuerzo | Categoría |
|---|---------|---------|----------|----------|
| 1 | `currentYaw` raw en vez de `error` normalizado | Crítico | Bajo | Bug |
| 2 | Gap 3 < Yp < 5 congela el robot | Crítico | Trivial | Bug |
| 3 | Protocolo UART sin sincronización | Crítico | Medio | Confiabilidad |
| 4 | s3 no chequeado en oscilación | Alto | Trivial | Confiabilidad |
| 5 | PATEANDO_atras_arquero sin timeout | Alto | Trivial | Confiabilidad |
| 6 | Conflicto zirconLib vs pines propios | Alto | Bajo | Bug |
| 7 | Pausa de 1000ms post-pateo | Alto | Trivial | Desempeño |
| 8 | Código del delantero muerto (~500 líneas) | Medio | Medio | Arquitectura |
| 9 | Arco contrincante hardcodeado a amarillo | Medio | Bajo | Flexibilidad |
| 10 | Naming inconsistente + typos | Bajo | Bajo | Legibilidad |
| 11 | Sin posicionamiento por arco propio | Medio | Alto | Desempeño |
| 12 | Sin predicción de trayectoria | Medio | Medio | Desempeño |

---

## 7. BUGS COMPARTIDOS CON DELANTERO (resolver en módulo común)

Estos bugs están presentes en ambos programas y debieran resolverse una sola vez en módulos compartidos:

1. **Protocolo UART sin sincronización** → resolver en `comunicacion.h`
2. **Conflicto zirconLib** → resolver en `config.h` / `motores.h`
3. **`avanzar()` sin corrección giroscópica** → resolver en `motores.h`
4. **Chequeo de línea duplicado ~15 veces** → resolver en `sensores.h`
5. **Código de motor inline repetido** → resolver en `motores.h`
6. **Constantes mágicas** → resolver en `config.h`
7. **`START_BYTE 0xAA` con punto y coma extra** → eliminar

---

## 8. PROPUESTA: ESTRUCTURA UNIFICADA PARA 2026

```
software/
├── shared/                    # Módulos compartidos entre ambos robots
│   ├── config.h               # Pines, constantes, selección ROBOT1/ROBOT2
│   ├── motores.h / .cpp       # Primitivas: avanzar, girar, orbitar, patear
│   ├── sensores.h / .cpp      # Lectura línea, giróscopo, heading con corrección
│   └── comunicacion.h / .cpp  # Protocolo UART robusto con OpenMV
├── robot-delantero/
│   └── delantero.ino          # Solo máquina de estados del delantero
├── robot-arquero/
│   └── arquero.ino            # Solo máquina de estados del arquero
└── vision/
    └── main.py                # OpenMV con protocolo mejorado
```

Con esta estructura:
- Cada `.ino` tendría ~200-300 líneas (solo la lógica de comportamiento)
- Un fix en `comunicacion.h` se aplica a ambos robots automáticamente
- Cambiar de robot es cambiar qué `.ino` se compila
- Los alumnos pueden trabajar en paralelo sin conflictos
