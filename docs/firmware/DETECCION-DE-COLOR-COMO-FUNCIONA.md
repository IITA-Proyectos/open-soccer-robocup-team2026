---
title: "Cómo funciona la detección de color (pelota y arcos) y la selección de arco a atacar"
date: 2026-06-21
status: vivo (explicativo)
audiencia: "Equipo IITA — Virginia, Elías, alumnos nuevos 2027"
author: "Claude Opus 4.8 (Anthropic), vía Claude Code"
requested-by: "Gustavo Viollaz (@gviollaz)"
tipo: documento-explicativo
---

# Cómo el robot VE colores (y decide a qué arco patear)

> **En una frase.** Las cámaras no "entienden" la cancha: buscan **manchas de un color** (la pelota
> naranja, el arco amarillo, el arco azul), miden **dónde** están y mandan esas posiciones por un cable.
> Quién es "el arco rival" NO lo decide la cámara — lo decide la placa de arriba (TOP) mirando **cuál arco
> tiene el robot adelante**. Este documento explica las dos cosas, paso a paso.

Este doc **junta en un solo lugar** algo que hoy está repartido en el código y en varios docs (ver §7).
No reemplaza a esas fuentes: las explica.

---

## 0. El recorrido completo (mapa mental)

```
  CÁMARA FRONTAL ─┐  ve 3 colores (naranja/amarillo/azul)         ┌─ goal_opp_* (arco RIVAL → atacar)
                  ├─► manda POSICIÓN de cada color ─► PLACA TOP ──►┤
  CÁMARA TRASERA ─┘  por UART (no decide roles)     (fusiona +     └─ goal_own_* (arco PROPIO → defender)
                                                     decide polaridad)        │
                                                                         WorldSnapshot
                                                                              │
                                                                      PLACA CENTRAL (delantero/arquero)
                                                                      usa pelota + arco rival ya resueltos
```

Tres capas, tres responsabilidades:
1. **Cámara** (OpenMV N6): *"veo una mancha naranja en tal lugar, una amarilla en tal otro"*. Nada más.
2. **TOP** (Teensy de arriba): junta las dos cámaras y **decide cuál arco es el rival**.
3. **CENTRAL** (Teensy del medio): recibe todo ya masticado y juega (apunta el pateo al arco rival).

---

## 1. Cómo la cámara DETECTA un color (la pelota naranja es el ejemplo)

Todo esto vive en **`hardware/electronics/camaras-openmv/main.py`** (el mismo programa corre en las 2
cámaras). El mecanismo es **idéntico** para la pelota (naranja) y para los dos arcos (amarillo y azul):
solo cambian los números del color.

### 1.1 Primero: la cámara "se fija la vista" (exposición y balance)

Una cámara, si la dejás en automático, va cambiando solita el brillo y el color según la luz. Eso **rompe**
la detección: un naranja con una luz se ve distinto con otra. Por eso, al arrancar, el programa:
- prende los automáticos unos segundos (para no arrancar con la imagen negra),
- y después los **APAGA y los congela** con valores fijos (`main.py:34-40`):
  `set_auto_whitebal(False, …)`, `set_auto_gain(False, …)`, `set_auto_exposure(False, …)`.

> **Por qué importa:** con la vista congelada, "naranja" siempre se ve igual → la detección es estable.
> Esto hay que **rehacerlo en Incheon** bajo la luz del estadio (ver §6).

### 1.2 El "color" se define en espacio LAB (no en RGB)

Cada color se define con **6 números** llamados *threshold LAB* — `(Lmin, Lmax, Amin, Amax, Bmin, Bmax)`:
- **L** = qué tan claro/oscuro (0=negro, 100=blanco).
- **A** = del verde (−) al rojo (+).
- **B** = del azul (−) al amarillo (+).

Se usa LAB y no RGB porque **separa el color del brillo**: una pelota naranja a la sombra o al sol cambia su
brillo (L) pero mantiene su "naranjés" (A y B). Eso la hace más robusta.

Ejemplo (cómo se distinguen amarillo y azul): **el arco amarillo tiene B muy positivo** (tira al amarillo)
y **el azul tiene B muy negativo** (tira al azul). Ese canal B es el que los separa.

### 1.3 `find_blobs`: buscar la mancha más grande de ese color

El corazón es esta línea (`main.py:125`):

```python
naranja_blobs = img.find_blobs([naranja_threshold], roi=roi,
                               pixels_threshold=7, area_threshold=7, merge=True)
```

- `find_blobs` recorre la imagen y agrupa los píxeles que caen dentro del threshold en **"blobs"** (manchas).
- `merge=True`: si la mancha quedó partida en pedacitos, los une.
- `pixels_threshold`: **tamaño mínimo** para que cuente como mancha. La **pelota es chica → 7 píxeles**;
  los **arcos son grandes → 600** (`main.py:126-127`). Eso filtra ruido (un reflejito naranja de 3 píxeles
  no es la pelota).
- De todas las manchas de ese color, se queda con **la más grande** (`procesar_blob`, `main.py:107-113`):
  `largest_blob = max(blobs, key=lambda b: b.pixels())`. La idea: la pelota real es la mancha naranja más
  grande del cuadro.

**Antes de buscar**, el programa tapa de negro las **dos esquinas de arriba** (`enmascarar_esquinas`,
`main.py:94-104`) y recorta un poco el borde inferior (`roi`, `main.py:123`). Por qué: ahí suele aparecer
**lo de afuera de la cancha** (público, paredes) que da falsos positivos.

### 1.4 De "píxeles" a "posición real" (homografía)

La mancha está en coordenadas de **píxel** (dónde cae en la foto). Pero al robot le sirve saber **dónde está
en centímetros** (izquierda/derecha, adelante). Eso lo hace `transformarcoordenadas` (`main.py:71-91`) con
una **matriz de homografía** `H` (calibrada por cámara) + una corrección por la altura de la cámara. El
resultado se recorta a ±100 y se le suma 100 → queda un número **0..200** (para que viaje en 1 byte sin
signos).

### 1.5 El mensaje que manda la cámara

Arma un paquete y lo manda por UART al TOP (`main.py:145-150`):

```
[201, Xp, Yp,  202, Xam, Yam,  203, Xaz, Yaz,  CRC8, 254]
  │    └pelota┘    └─amarillo─┘    └──azul──┘     │     └ fin de paquete
  └ "viene pelota"  "viene amarillo" "viene azul"  └ chequeo de errores
```

- `201/202/203` son **etiquetas de color** (cabeceras): 201=pelota naranja, 202=arco amarillo, 203=arco azul.
- Si un color **no se ve**, manda el centinela **255** en su lugar (sentinel = "no visible").
- `CRC8` es un número de control para detectar si el mensaje llegó corrupto; `254` marca el fin.

> **Clave conceptual:** la cámara manda **los TRES colores que ve, crudos**. NO dice "este es el arco
> rival". Solo dice "vi amarillo acá y azul allá". La decisión de roles es de la capa de arriba.

### 1.6 Los LED de diagnóstico (mirar sin computadora)

La cámara prende un LED por color detectado (`main.py:129-139`): **rojo = ve naranja (pelota)**,
**verde = ve amarillo**, **azul = ve azul**. Sirve para verificar en el banco de un vistazo, sin la PC.

---

## 2. Cómo se decide A QUÉ ARCO atacar (selección de arco)

Esto **NO pasa en la cámara ni en la CENTRAL**: pasa en la placa **TOP**, en un módulo llamado
**`goal_polarity`** (`software/teensy/Soccer 2026/src/shared/goal_polarity.{h,cpp}`).

### 2.1 La regla: "el arco que tengo adelante es el del rival"

El robot arranca **mirando a la cancha** (de frente al arco contrario). Entonces:
- el arco que ve **adelante** (ángulo `|θ| < 90°`) = **RIVAL** (al que hay que hacer gol),
- el de **atrás** = **PROPIO** (el que defendemos).

Eso es todo: no mira el color para decidir el rol, mira **la posición** (`goal_polarity.cpp`). Si ve los dos
arcos adelante (situación confusa), devuelve `UNKNOWN` y no decide con datos contradictorios.

### 2.2 El "latch": decidir una vez y no cambiar de opinión

Para que un frame malo no le haga creer que cambió de arco, usa un **latch anti-rebote**: necesita **30
lecturas seguidas iguales** (~0,3 segundos) antes de **congelar** la decisión para todo el resto de la
mitad. Una vez fijada, no se mueve aunque parpadee la visión.

### 2.3 El fail-safe (si nunca lo ve)

Si nunca logra confirmar (no ve arcos al arrancar), usa un **valor por defecto**: amarillo = rival
(`YELLOW_IS_OPP`). Es una red de seguridad: degrada, no rompe.

### 2.4 Cómo le llega esto a la CENTRAL

El TOP arma el `WorldSnapshot` y mete el arco **ya etiquetado por ROL**, no por color:
- `goal_opp_*` = arco **rival** (ángulo + distancia + visible),
- `goal_own_*` = arco **propio**.

La CENTRAL (delantero y arquero) **solo lee esos campos** y juega. Nunca pregunta "¿de qué color es?". Por
eso, en los programas mix de la CENTRAL ya **no hay lógica de color** (se sacó en los commits del 2026-06-21):
el delantero apunta a `goal_opp`, el arquero también lo usa para el despeje dirigido.

> Para el detalle de esta capa y la historia (2025 hardcodeaba "amarillo", 2026 lo automatizó), ver los dos
> documentos de análisis en `research/in-progress/` listados en §7.

---

## 3. Resumen en una tabla (qué hace cada capa con el color)

| Capa | Qué hace con el color | Qué NO hace |
|---|---|---|
| **Cámara N6** | Detecta 3 colores (naranja/amarillo/azul) por threshold LAB + `find_blobs`, manda la posición de cada uno | NO decide cuál arco es rival; NO entiende "cancha" |
| **TOP** | Junta las 2 cámaras y **decide rol**: el arco al frente = rival (`goal_polarity` + latch + fail-safe) | NO re-detecta color crudo (lo recibe de la cámara) |
| **CENTRAL** | Usa `goal_opp` (rival) para apuntar el pateo; usa la pelota para perseguirla | NUNCA pregunta por color (amarillo/azul) |

---

## 4. Los colores que detecta hoy y sus números

| Color | Etiqueta | Para qué | Tamaño mínimo |
|---|---|---|---|
| Naranja | 201 | la **pelota** | 7 píxeles (es chica) |
| Amarillo | 202 | un **arco** | 600 píxeles (es grande) |
| Azul | 203 | el otro **arco** | 600 píxeles |

> ⚠️ **OJO — hay dos juegos de umbrales LAB en el repo y NO coinciden** (esto hay que ordenarlo):
> - En el código vivo `camaras-openmv/main.py:47-49`:
>   `naranja=(30,61,39,70,20,50)`, `amarillo=(40,65,0,20,10,30)`, `azul=(10,30,0,30,-35,-10)`.
> - En el doc de calibración `docs/firmware/CALIBRACION-VISION-N6.md` (marcado "calibrado 2026-06-09"):
>   `naranja=(21,67,18,79,-32,127)`, `amarillo=(17,70,-27,14,38,111)`, `azul=(4,38,-13,57,-64,-4)`.
>
> El doc dice que `main.py` es "la fuente única", pero `main.py` tiene los OTROS valores. **El repo ya
> registra esta deuda** (`docs/FUENTES-DE-VERDAD.md`: *"hasta 3 sets de thresholds LAB divergentes — cerrar
> en banco"*; el tercero está en los `cam-*-n6.py` de los packs, **deprecados**). **Antes de Incheon hay
> que decidir cuál es el bueno y dejar uno solo** (lo confirma el banco mirando qué detecta bien). En
> cualquier caso, los LAB **se recalibran sí o sí en Incheon** bajo la luz del venue.

---

## 5. ¿Cómo verificar que "ve" bien? (banco)

> Regla del repo: esto lo cierra el **equipo** en hardware; Claude no marca TASK de banco como hecha.

1. **LEDs:** poné la pelota → debe prender el **rojo**; un arco amarillo → **verde**; azul → **azul**.
2. **Recuadro:** en el OpenMV IDE, el recuadro de color debe rodear **solo el objeto**, sin agarrar fondo.
3. **El TOP cuenta paquetes** de las dos cámaras (contadores `cameras_packets_front/back`).
4. **Roles:** encendé mirando un arco → el TOP debe fijar la polaridad correcta (`goal_opp` apunta al de
   adelante) en < 1 s y no cambiarla.

Procedimiento completo de calibración: [`CALIBRACION-VISION-N6.md`](CALIBRACION-VISION-N6.md).

---

## 6. Lo que hay que rehacer en Incheon

- **Recalibrar los 3 umbrales LAB** bajo la luz del estadio (los LAB dependen de la iluminación). Kit:
  `calib-lab-n6.py`. ~15 min por cámara, repetible.
- **Fijar la exposición** para esa luz (`BRING_UP=False`).
- **Confirmar la premisa de arranque** de `goal_polarity`: el robot debe **arrancar mirando a la cancha**
  para que fije bien el rol del arco. Si lo colocan girado, puede fijar al revés.

---

## 7. Dónde está cada cosa en el repo (las fuentes)

**Detección de color (cámara):**
- Código vivo (la verdad): `hardware/electronics/camaras-openmv/main.py`
- Cómo calibrar (procedimiento + umbrales): `docs/firmware/CALIBRACION-VISION-N6.md`
- Funcionalidad explicada: `hardware/electronics/cameraFront-pack/02-funcionalidad.md` (y `cameraBack-pack/`)
- Contrato del mensaje cámara→TOP: `docs/firmware/CONTRATO-DATOS-CAMARAS.md`
- Homografía (píxel→cm): `docs/firmware/CALIBRACION-HOMOGRAFIA-XY-N6.md`
- Skill operativa: `.claude/skills/openmv-n6-camara-vision-robocup/SKILL.md`
  *(ojo: la skill vieja `openmv-vision-tuning` quedó superada — describe H7 + pelota IR + arcos cyan/magenta,
  que NO es lo de hoy. Lo vivo es N6 + pelota naranja + arcos amarillo/azul.)*

**Selección de arco (rol propio/rival):**
- Código (con comentarios explicativos): `software/teensy/Soccer 2026/src/shared/goal_polarity.{h,cpp}`
- Análisis corto: `research/in-progress/2026-06-21-deteccion-arco-propuesta-concreta.md`
- Análisis exhaustivo (cómo funciona + historia 2025→2026): `research/in-progress/2026-06-21-deteccion-arco-ataque-mix-delantero-arquero.md`
- Contrato del snapshot (goal_opp/goal_own): `software/teensy/Soccer 2026/src/shared/types.h`

---

*Documento explicativo (consolida fuentes existentes). Apoyo de Claude; atribución según `AI-INSTRUCTIONS.md`.
Los valores y rutas se verificaron leyendo el código real al 2026-06-21.*
