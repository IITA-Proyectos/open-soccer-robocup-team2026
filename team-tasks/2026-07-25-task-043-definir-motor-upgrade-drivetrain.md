---
id: TASK-043
title: "Definir el motor del upgrade de tracción (modelo · caja · acople de eje · encoder)"
date_created: 2026-07-25
assigned: [gviollaz]
priority: P0
status: pending
estimated_hours: 6
tags: [hardware, mecanica, motores, compras, roboliga-2026, robocup-2027]
blocks: [compra-motores, rediseno-chasis-v2, firmware-bldc]
roadmap_id: HW-010
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Claude Opus 5, Anthropic)"
---

# TASK-043 — Definir el motor del upgrade de tracción

## Resumen

Antes de comprar los motores del robot de **Roboliga 2026** (el mismo que iría a **RoboCup 2027
Alemania**) hay que cerrar **cuatro decisiones**: (1) qué modelo exacto, (2) con caja reductora o
sin caja, (3) qué tipo de eje/acople, (4) con encoder integrado o sin encoder.

## Contexto

- Los motores actuales son **DC brushed tipo "TT", nominal 5 V, alimentados a 7,4 V** → se degradan
  arriba de ~70 % de duty. El upgrade a brushless está en el roadmap como **HW-010** desde
  2026-03-20 (`research/backlog/investigar-motores-brushless-encoders.md`), pero ese research
  **solo tiene preguntas, ninguna respuesta**: no hay modelo, tensión, RPM ni torque definidos.
- Candidato en evaluación: **Nanotec DF45** (BLDC plano, 45 mm, 24 V), el que usan equipos de
  **RoboCup Small Size League** (TIGERs Mannheim; y "The A-Team", ex-RoboJackets de Georgia Tech).
- ⚠️ **Sin estas 4 definiciones no se puede pedir cotización**, porque cada una cambia el part
  number y algunas implican **pedido especial** (plazo largo + posible mínimo de compra).

> **Nota de método.** Todo lo marcado ✅ abajo está verificado contra **fuente primaria**
> (datasheet de fábrica / reglamento oficial). Lo marcado ❓ está **sin verificar** y es
> justamente lo que esta TASK tiene que cerrar. No dar por cierto lo ❓.

---

## Decisión 1 — ¿Qué MODELO exacto?

**Estado: ABIERTA.**

| Opción | Par rated / pico | Velocidad rated | Potencia | Nota |
|---|---|---|---|---|
| **DF45M**024053-A2 | 0,084 / 0,25 N·m | 5.260 rpm | 50 W | el que se estaba por pedir |
| **DF45L**024048-A2 | **0,13 / 0,39 N·m** | 4.840 rpm | 65 W | **el que usa TIGERs**; único tamaño con opción de encoder |
| DF45S… | — | — | 30 W | descartado (menos par) |

- ✅ **El código `DF45-K1V38786` NO existe en ningún catálogo público.** No coincide con el formato
  de part number de catálogo (`DF45M024053-A2`) ni con los números de plano internos (`03000184`
  para el M, `03000168` para el L). Es probablemente un código OEM/de configuración. **No se puede
  girar plata contra ese código**: hay que pedirle a Nanotec el part number de catálogo equivalente.
- ✅ **"6250 rpm" no existe en ninguna fuente.** Los números reales son: no-load 6.700 (M) / 6.100 (L);
  rated 5.260 (M) / 4.840 (L). **No poner 6250 en el TDP.**
- ✅ **Cuidado con el par que publican los distribuidores:** Farnell/Newark titulan el M como
  "0.25 N-m", pero el datasheet dice `Torque - Rated / Peak: 0,084 / 0,25`. **0,25 es el PICO.**
  Quien dimensione con 0,25 como continuo se equivoca por 3×.

**Cómo se cierra:** confirmar con Nanotec el part number de catálogo, y decidir M vs L en función
de las decisiones 2 y 4 (el L es el único que admite encoder integrado).

---

## Decisión 2 — ¿CON caja reductora o SIN caja (direct drive)?

**Estado: hay una recomendación técnica fuerte, falta la decisión formal del equipo.**

**Recomendación: SIN caja (direct drive).** Fundamento (cálculos sobre datos primarios):

| Configuración (rueda Ø58) | Fuerza por rueda |
|---|---|
| DF45M directo — continuo | 2,90 N |
| DF45M directo — pico | 8,62 N |
| **Límite de patinado** (robot 2 kg, μ≈0,7, 3 ruedas) | **≈ 4,6 N** |

- El continuo queda **debajo** del patinado y el pico ~1,9× **arriba**: dimensionamiento correcto
  **sin reductor**. Con i=4 el pico sería 7× el límite del piso; con i=9, 15×. Es par que el piso
  no puede transmitir → **desperdicio**.
- ⚠️ **La inercia reflejada crece con i²** (motor de rotor externo): i=7 con rueda chica agrega
  **+2,75 kg de masa aparente** — más que el robot entero. La reducción **empeora** la aceleración.
- ✅ **El planetario Nanotec GP42 NO ENTRA físicamente**: requiere ~103 mm de radio antes de llegar
  a la rueda, y hay ~90 mm disponibles en un robot de 18 cm. Además su rpm de entrada **nominal**
  es 3.000, por debajo de las 5.260 del motor → el reductor pasaría a ser el techo del régimen.
- ✅ **Precedente:** TIGERs Mannheim monta los DF45L en **direct drive** (sin reductor) con ruedas
  de Ø62 mm en un robot de **Ø178 × 148 mm, 2,62 kg** — prácticamente el cilindro de 18 cm de RCJ.

> ⚠️ **Esto SUPERA a `docs/internal/robot-v2/robot-v2-documento-maestro.md:143`**, que proponía
> "reducción por correa dentada corta" como primera línea. Ese doc está marcado como HISTÓRICO
> (mar-2026) y dejaba la reducción explícitamente abierta. **Pero su `:149` SIGUE VIGENTE y es
> correcto:** la rueda **NO puede colgar del eje del motor** (ver Decisión 3).

**Cómo se cierra:** decisión de Gustavo, registrada en las Notas de esta TASK. Si se elige direct
drive, actualizar el doc V2 marcando el `:143` como superado.

---

## Decisión 3 — ¿Qué TIPO DE EJE y qué ACOPLE?

**Estado: ABIERTA — es la decisión que más condiciona el pedido (puede volverlo pedido especial).**

### Lo que trae de fábrica (✅ verificado en datasheet)

| Dato | Valor |
|---|---|
| Diámetro | **Ø4 mm** (tolerancia −0,005/−0,01 → entra deslizante en agujero de 4) |
| Forma | **cilíndrico LISO** — sin plano D, sin rosca, sin chavetero |
| Montaje | buje de centrado Ø16 + **3× M3** (prof. mín. 3 mm) a 120° |
| **Límite del rodamiento** | **28 N radial** (medido a 10 mm del apoyo) · **10 N axial** |

**NO viene con eje de 3 mm.** El estándar es Ø4 liso.

### El problema del eje liso

Un eje **liso** transmite el par **solo por fricción** (prisionero contra superficie redonda). En un
omni-3 que **invierte el sentido de cada rueda decenas de veces por segundo**, un prisionero sobre
eje redondo **termina aflojándose y patinando** → se pierde la referencia y la calibración.
Un **plano D** da arrastre positivo (el prisionero apoya sobre la cara plana).

### ⚠️ El plano D es una MODIFICACIÓN, no una opción de catálogo

- ✅ Nanotec ofrece "Flat on shaft (D-shaft)" como **modificación de eje** (servicio), con
  **formulario propio** (`Form-shaft-modification.pdf`). También ofrece eje más corto, más fino,
  agujero transversal y chavetero.
- ⇒ **Implica pedido especial**: suma **plazo de entrega** y posiblemente **mínimo de compra**.
  Para un pedido de 1–4 unidades esto puede ser bloqueante. **Hay que preguntarlo explícitamente.**
- ❓ **SIN VERIFICAR:** si existe una variante de catálogo con eje D de 4 o 5 mm ya hecha; si el
  ancho/profundidad del plano es configurable; el sobrecosto y el plazo; si hay MOQ.

### La restricción mecánica que NO cambia con el tipo de eje

El límite de **28 N radiales a 10 mm** significa que **la rueda no puede ir colgada del eje del
motor**. Hace falta **eje propio soportado por rodamientos + acople** entre motor y eje.
Candidato de acople ya identificado: **cubo hexagonal Nexus 18037** (agujero Ø4 mm, hex 8 mm,
18 mm de largo) — pensado para la rueda omni de 38 mm.

### ⭐ Dependencia con la RUEDA — y por qué puede ser menor de lo que parece

**Planteo de Gustavo (2026-07-25):** *"en principio parecería lo mejor eje de 4 mm en D, pero
antes de confirmar hay que ver qué ruedas conseguimos"*. Correcto como orden de trabajo, con un
matiz técnico que conviene tener presente **antes** de pagar un pedido especial:

- **Si la rueda va montada sobre un eje intermedio soportado por rodamientos** (que es lo que la
  restricción de 28 N **obliga** a hacer), entonces el agujero del cubo de la rueda tiene que
  coincidir con **ese eje intermedio** —que lo elige/fabrica el equipo— y **NO con el eje del
  motor**. El eje del motor solo tiene que entrar en el **acople**. En ese caso la rueda **no
  condiciona** el tipo de eje del motor, y el plano D deja de ser urgente: el arrastre positivo se
  puede resolver en el **acople** (abrazadera de apriete / clamping hub, que aprieta por
  circunferencia y no depende de un plano).
- **Solo si se monta la rueda directamente sobre el eje del motor** (no recomendado: viola el
  límite de 28 N) el agujero del cubo tiene que ser exactamente el diámetro del eje del motor.

⇒ **Conclusión operativa:** primero definir **rueda + arquitectura del eje** (colgada vs soportada),
y recién ahí decidir si hace falta pagar el plano D. Puede que no haga falta.

❓ **SIN VERIFICAR:** qué ruedas omni se consiguen realmente en Argentina (modelos, diámetros,
diámetros de cubo disponibles, precio y plazo), y si conviene comprarlas acá o importarlas en el
mismo envío que los motores.

**Cómo se cierra:** decidir, en este orden —
1. qué rueda omni se consigue (diámetro, ancho, material de los rodillos, cubos disponibles);
2. arquitectura del eje: rueda sobre eje intermedio con rodamientos (recomendado) vs colgada;
3. recién entonces: eje liso Ø4 estándar + acople de apriete, **o** plano D como modificación
   (asumiendo sobrecosto y plazo).

⚠️ **Sobre el material de los rodillos:** la rueda Nexus de 58 mm trae rodillos de **Nylon+PE
(plástico duro = poca fricción)**, mala para tracción en un deporte de contacto; la de 38 mm trae
**TPR (gomoso)**, mejor agarre. Como todo el dimensionamiento del motor descansa en el límite de
patinado, **el material de los rodillos importa tanto como el diámetro** (medición de μ en TASK-044).

---

## Decisión 4 — ¿CON encoder integrado o SIN encoder?

**Estado: ABIERTA — pero hay un argumento fuerte a favor del encoder.**

- ✅ **El encoder integrado existe SOLO en el tamaño L.** Nanotec, textual: el encoder inductivo de
  2 canales y **1.024 CPR** está disponible *"in size L… (DF45L…-E)"*. **El M no lo tiene.**
- ✅ **Sin encoder, la realimentación son los 3 sensores Hall: 48 impulsos por vuelta** = un aviso
  cada 7,5° = **~3,8 mm de avance por flanco** en la rueda (direct drive, Ø58). Es **grosero** justo
  en el régimen lento del posicionamiento fino frente al arco.
- ✅ **El problema medido del robot NO es potencia, es precisión:** rango útil de duty **70→150 PWM**,
  **rotación mínima ~300°/s** (no sabe girar despacio parado), **deriva parásita ~80°/s** al
  desplazarse de costado. Meter 25× más potencia **no ataca eso**; medir la velocidad de cada rueda sí.
- ✅ **Los equipos de referencia le ponen encoder de alta resolución igual:** TIGERs usa un
  **iC-Haus de 23.040 ppr** sobre el DF45L. O sea: ir a BLDC sin encoder es cambiar un problema por otro.

❓ **SIN VERIFICAR:** part number exacto del `DF45L…-E`, su precio, si es artículo de catálogo o
pedido especial, cuánto suma en largo/peso, y qué señales entrega (A/B/index).
*(Sourcing en curso — ver Notas.)*

**Cómo se cierra:** decidir entre (a) `DF45L…-E` con encoder integrado, (b) `DF45L024048-A2` sin
encoder + encoder externo, o (c) sin encoder, aceptando control grosero a baja velocidad.

---

## Pasos concretos

1. **Pedir a Nanotec, por escrito y en un solo mail** (formulario del producto o +49 89 900686-898):
   - a qué part number de catálogo corresponde `DF45-K1V38786`;
   - part number, precio, MOQ y plazo de: `DF45L024048-A2` **y** la versión con encoder `DF45L…-E`;
   - si el **plano D (4 o 5 mm)** es modificación a pedido: sobrecosto, plazo y mínimo;
   - si envían a Argentina o tienen representante en LatAm;
   - **si tienen programa de apoyo a equipos de RoboCup** (patrocinan a TIGERs y Delft Mercurians).
2. **Cotizar el canal local**: Electrocomponentes S.A. (CABA, representante del catálogo Farnell),
   Solís 225 · (5411) 4375-3366.
2-bis. **Relevar qué RUEDAS OMNI se consiguen** (Argentina e importadas): modelo, diámetro, ancho,
   **material de los rodillos** (gomoso > plástico duro para tracción), **diámetros de cubo
   disponibles**, precio y plazo. Esto precede a la decisión del eje (ver Decisión 3).
3. **Medir el robot** (bloquea todo lo mecánico — ver TASK-044): diámetro, altura, peso, diámetro
   real de la rueda omni y radio centro→rueda.
4. **Confirmar el reglamento de Roboliga 2026** con la organización (ver TASK-044).
5. **Decidir las 4 preguntas** y registrar la decisión en las Notas de esta TASK.
6. **Comprar 1 (UNO) motor + 1 controlador** como prototipo, antes de comprar los 3–4.

## Criterio de cierre

- [ ] **Decisión 1** — modelo y part number de catálogo EXACTO, confirmado por escrito por Nanotec
      (o por el distribuidor), pegado en las Notas.
- [ ] **Decisión 2** — con caja / sin caja, decidida y justificada en las Notas. Si es sin caja,
      `robot-v2-documento-maestro.md:143` queda marcado como superado.
- [ ] **Rueda definida** (modelo, diámetro, material de rodillos, cubo) y arquitectura del eje
      elegida (rueda sobre eje soportado vs colgada) — **precede a la Decisión 3**.
- [ ] **Decisión 3** — tipo de eje (liso Ø4 vs plano D) + diseño de acople definido, con el
      sobrecosto y el plazo del plano D conocidos (no estimados).
- [ ] **Decisión 4** — con o sin encoder, con part number y precio de la variante elegida.
- [ ] Las 4 decisiones vuelcan a un BOM del drivetrain con precio y plazo reales.
- [ ] `research/backlog/investigar-motores-brushless-encoders.md` (HW-010) se mueve a
      `research/completed/` con estas respuestas, o se actualiza apuntando acá.

## Notas / decisiones

*(A completar por el equipo a medida que lleguen las respuestas.)*

- **2026-07-25** — Abierta. Investigación multi-agente con verificación adversarial (13 agentes).
  Correcciones a supuestos previos que conviene no repetir:
  - El equipo de referencia **no es de "liga mayor"**: es **Small Size League** (robots Ø180 mm),
    *la misma clase de tamaño* que RCJ Soccer. La referencia es **más** aplicable, no menos.
  - **24 V es LEGAL**: el tope de 15 V fue **eliminado del reglamento en 2024 justamente para
    permitir BLDC planos tipo SSL**. Hoy rige 48 V DC (regla 1.3.2), sin límite de peso.
  - ⚠️ **La liga se renombró a "Soccer VISION"** y su límite bajó a **18,0 × 18,0 cm**
    (los 22 cm quedaron para "Soccer INFRARED", ex-Lightweight). Barrer TDP/póster, que dicen
    "Soccer Open" en todos lados. **Verificar si el robot actual entra en 18 cm** (ver TASK-044).

## Cambios de estado

- 2026-07-25 — creada (`pending`), asignada a Gustavo. Bloquea la compra de motores.
