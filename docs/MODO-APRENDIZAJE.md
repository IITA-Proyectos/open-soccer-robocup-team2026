---
title: "Modo APRENDIZAJE — cómo se trabaja en este repo desde el cierre de Incheon"
date: 2026-07-23
author: "Claude (Anthropic — Claude Opus 4.8)"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: vivo
tipo: decision-de-frame
supersedes: "el frame de modo competencia (P0 = bloqueante para Incheon) de CLAUDE.md"
---

# Modo APRENDIZAJE

> **Lectura obligatoria al abrir una sesión en este repo, junto con
> [`ESTADO-ACTUAL.md`](ESTADO-ACTUAL.md) y [`FUENTES-DE-VERDAD.md`](FUENTES-DE-VERDAD.md).**
> Este doc reemplaza el frame de competencia. Si algo en otro doc dice
> "antes de Incheon" o "P0 = bloqueante para Incheon", ese texto es **historia**,
> no una instrucción vigente.

## 1. El hecho que cambia todo

**La competencia de Incheon TERMINÓ.** Se jugó del **30 de junio al 6 de julio de 2026**.

El repo entero está escrito en tiempo futuro respecto de ese torneo: cuentas
regresivas, "no tocar antes de Incheon", "P0 = bloqueante para Incheon", moratorias
para no romper el binario de competencia. **Todo ese frame está muerto** desde el
2026-07-23.

El journal corta el `2026-07-04` (`journal/2026-07-04-arqueromix-pateo-recto-y-corte-retro-checkpoint-9.md`).
Entre esa fecha y hoy no hay nada registrado: los días de torneo y el post-mortem
**no están en el repo**. Eso es una deuda a saldar, no un vacío a ignorar.

> ⚠️ **Resultados y crónica de Incheon: PENDIENTE DE CARGAR por Gustavo.**
> Este doc NO inventa qué pasó en cancha. Lo que sigue viene del reporte verbal del
> 2026-07-23 y está marcado como tal.

## 2. La ventana tiene fecha de vencimiento

**Decisión de Gustavo, 2026-07-23:** *"ahora hay que aprender TODO, es el momento de probar,
cambiar, mejorar; en un mes comenzamos a trabajar en modo competencia para noviembre."*

```
2026-07-23 ─────────── ventana de aprendizaje ─────────── ~2026-08-23 ──── modo competencia ────► Nacional
            probar · romper · medir · entender              CONGELAMIENTO      consolidar          noviembre
                       (1 mes)                                                 (~2 meses)
```

| Fase | Cuándo | Qué se permite |
|---|---|---|
| **Aprendizaje** | 2026-07-23 → **~2026-08-23** | Todo. Romper, reescribir, migrar, probar hipótesis. Amplitud > profundidad en un solo tema |
| **Congelamiento** | ~2026-08-23 | Se decide, cambio por cambio: **queda o se revierte**. Nada a medias |
| **Competencia** | ~2026-08-23 → noviembre | Vuelve el riesgo bajo, los cambios gateados y el "no romper lo que anda" |

> **"Aprender TODO" es una instrucción de AMPLITUD.** No es "arreglar las 3 cosas rotas".
> Es recorrer el sistema entero — línea, ToF, cámaras, motores, comunicación, control —
> y en cada subsistema saber qué hace, por qué, y dónde está el límite. Los 3 subsistemas
> rotos (§5) son el punto de entrada, no el alcance.

### Regla de salida (la que evita el desastre)

**Al ~2026-08-23, todo cambio abierto está en uno de dos estados: VALIDADO EN BANCO o REVERTIDO.**

No se entra a modo competencia con un subsistema a medio migrar. Un módulo reescrito y no
probado es **peor** que el módulo viejo con sus defectos conocidos: al viejo le conocés las
mañas, al nuevo no. Si el 23 de agosto algo no está validado, se revierte y se anota como
aprendizaje — no se arrastra.

Corolario práctico: **cuanto más grande el cambio, más temprano hay que arrancarlo.**
Reescribir la lectura de línea es de la semana 1, no de la semana 4.

### Línea base para volver

Tag **`incheon-2026-estado-final`** (en `908473d`) = el firmware tal como quedó al cierre del
torneo. Es el punto de retorno si la exploración rompe algo irrecuperable.

```bash
git diff incheon-2026-estado-final --stat
```

Antes de romper cualquier cosa, esa es la referencia de "cómo estaba".

## 3. Qué cambia

| | Modo COMPETENCIA (hasta 2026-07-06) | Modo APRENDIZAJE (desde 2026-07-23) |
|---|---|---|
| **Objetivo** | Que el robot compita | Que el equipo **entienda** por qué falló y lo deje andando bien |
| **Quién implementa** | Los alumnos (Virginia, Elías). Claude NO implementaba | **Gustavo**, investigando y aprendiendo. Claude implementa **con** él |
| **Riesgo aceptable** | Casi cero — no romper el binario que compite | **Alto.** Romper para entender está permitido y es deseable |
| **Ritmo** | Cambios chicos, gateados, byte-idénticos | Se puede reescribir un subsistema entero si eso enseña más |
| **Reloj** | Cuenta regresiva al torneo | Sin reloj. Manda la profundidad, no la fecha |
| **Criterio de éxito** | "Anda en cancha" | "Sé **por qué** anda, y puedo explicarlo" |
| **Documentación** | Mínima, para no frenar | **Es el producto.** Un fix sin explicación no cuenta |

### El cambio operativo más importante

Antes: Claude proponía → los alumnos probaban en banco días después → el resultado
volvía (o no) al repo. Por eso existía la regla *"Claude no cierra TASKs de hardware"*.

Ahora: **Gustavo tiene la placa en la mano durante la sesión.** El lazo se cierra en
minutos, no en días. Claude propone, Gustavo prueba, el resultado se registra en la
misma sesión.

**La regla no desaparece, se afila:** Claude sigue sin poder declarar que algo funciona
porque compila o porque los tests host pasan. Lo que cambia es que la evidencia de banco
ahora llega en la misma conversación. **Sigue siendo Gustavo el que dice "anda".**

## 3. Nueva escala de prioridad

La vieja escala estaba anclada al torneo. La nueva está anclada al **aprendizaje** y al
próximo hito doméstico.

- **P0 — Bloquea el aprendizaje.** Sin esto no se puede investigar nada más: el
  subsistema no da datos, no se puede medir, o se pierde información
  irrecuperablemente (ej. flashear sin backup).
- **P1 — Deuda que va a volver a morder.** Funciona a medias y nadie sabe por qué.
  Si no se entiende ahora, reaparece en el Nacional de noviembre.
- **P2 — Capitalizable a 2027.** Mejora real pero no bloquea entender lo de hoy.

`risk-no-fix` / `risk-fix` / `tiempo` **siguen siendo obligatorios**. Lo que cambia es
que `risk-fix` ya no incluye "rompe el binario de competencia" como veto automático.

## 4. Las 3 líneas de investigación abiertas

Reportadas verbalmente por Gustavo el 2026-07-23 como *"lo que no funcionó"*. Su
root-cause técnico se audita aparte; acá queda el registro de qué se investiga y por qué.

### (A) Cámaras traseras — no se supieron calibrar
Sospecha inicial de Gustavo: **el firmware de las cámaras no se actualizó.**
Plan acordado: actualizar el firmware de **la trasera del ROBOT1 primero**; si anda
bien, recién ahí las otras tres.
⚠️ **Precondición dura:** no se flashea nada sin haber respaldado antes el script
`main-r*.py` vivo, la calibración LAB, la matriz de homografía y el binario de
firmware actual. Ver [`RESPALDO-ANTES-DE-FLASHEAR.md`](RESPALDO-ANTES-DE-FLASHEAR.md)
cuando exista.

### (B) Sensores de línea / odometría de piso (placa DOWN) — sin ventana de calibración
Síntoma textual: **"pasaba de no detectar a detectar falsas líneas"**. O sea: no existe
un umbral intermedio que funcione. Hipótesis inicial de Gustavo: *"no sé si será que el
cambio se hacía por números enteros"*.
Pedido explícito: **un programa de calibración más simple.**

### (C) Sensores ToF — nunca anduvieron bien
Cuatro VL53L7CX en el TOP. Hay mucho trabajo previo documentado
(`docs/firmware/TOF-ZONAS-TOP-MONITOR-ANALISIS.md`, piloto 8×8, TASK-225 a TASK-228)
y ninguna conclusión de que sirvan hoy.

**Objetivo de las tres: dejarlas andando BIEN, entendiendo por qué.**

## 5. Rango de TASKs

Las TASKs de modo aprendizaje usan el rango **TASK-400 a TASK-499**, para que se
distingan de un vistazo de las de competencia.

Los rangos viejos siguen válidos y **no se renumeran**:
`001-099` generales · `100-199` CENTRAL · `200-299` TOP · `300-399` DOWN.

Las TASKs abiertas que quedaron de competencia **no se cierran de oficio**: se revisan
una por una y se marcan como *vigente en modo aprendizaje*, *histórica* o *muerta*.

## 6. Qué NO cambia

Estas reglas siguen intactas — el cambio de modo no las toca:

1. **Verificar antes de afirmar.** El código manda sobre la documentación. Si hay
   contradicción, se marca; no se elige en silencio ni se homogeneiza tapando.
2. **`SUCCESS` de una herramienta no prueba nada.** Que compile, que `pio` diga OK o que
   pasen los tests host **no es evidencia de que funcione en el robot**.
3. **Journal vivo.** Toda sesión deja entrada en `journal/YYYY-MM-DD-*.md`.
4. **Atribución correcta** en commits (ver [`AI-INSTRUCTIONS.md`](../AI-INSTRUCTIONS.md)).
5. **No tocar `legacy/`** ni `software/teensy/Soccer 2026/_archive/`.
6. **DRC + ERC antes de mandar a fabricar PCB.**
7. **Actualizar `FUENTES-DE-VERDAD.md` / `ESTADO-ACTUAL.md` en el mismo commit** que
   crea o supera un doc.

## 7. Horizonte

- **Nacional Argentina, noviembre 2026** — próximo hito real. Cosecha doméstica de lo
  que se aprenda ahora.
- **Mundial 2027** — Virginia transiciona a coach + entran alumnos nuevos.
  **El repo tiene que sobrevivirla.** Todo lo que se investigue ahora se escribe pensando
  en que lo lea alguien que no estuvo.

---

## Referencias

- [`ESTADO-ACTUAL.md`](ESTADO-ACTUAL.md) — snapshot vivo de qué corre hoy.
- [`FUENTES-DE-VERDAD.md`](FUENTES-DE-VERDAD.md) — qué doc es canónico por tema.
- `docs/BACKLOG-INCHEON.md` — **histórico.** Backlog del torneo; sus ítems P0/P1 se
  reevaluaron bajo la escala nueva.
- [`../CLAUDE.md`](../CLAUDE.md) — frame de sesión, apunta acá.
