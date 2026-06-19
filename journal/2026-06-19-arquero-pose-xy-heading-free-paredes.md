# 2026-06-19 — Arquero: pose XY HEADING-FREE por paredes (TOP) + lateral acotado (CENTRAL)

## Contexto / por qué
La trilateración clásica del TOP (`localization.cpp`) daba pose XY inservible para el
arquero (ver journal 2026-06-18 + sesión de debug por serie). Dos causas de fondo:
1. **Heading inestable** (BNO se cae intermitente → `imu valid=0`) → la trilateración
   rota el mapa → pose coherente pero equivocada. Es la raíz (skill `fusion-pose-odometria-landmarks`).
2. **Convención de paredes espejada** (POS `RIGHT` mapeaba a bearing 90, pero
   `classify_wall` lee 90=WEST) → izq/der cruzados. Confirmado en banco: con los
   bearings laterales swapeados (der=270, izq=90) la X pasó de 515 (mal) a 320 (ok).
3. **Reducción de zonas contaminada por el piso y el "por encima de la pared"**: el
   reductor `tof_zone_masked_robust` promedia supervivientes y NO descarta el cúmulo
   alto (lecturas ~900-1050 que pasan por encima de la pared de 200 mm pero siguen
   < field_max 2430) → distancia de pared inflada.

Pedido de Gustavo: pose XY **sin heading**, asumiendo que el arquero mira al frente
(lo sostiene el heading-hold), usando ToF izq/der/atrás → (x,y) aproximado, útil en
la **zona de fondo**. NO es fusión con odometría (eso es 2027); es la fuente landmark
pura y mínima.

## Qué se hizo
- **`src/shared/keeper_xy_walls.h`** (módulo PURO, header-only, host-testeable):
  - `keeper_wall_dist_mm()`: reduce 16 zonas → distancia de pared por **mediana +
    recorte simétrico** (±35% de la mediana). Descarta piso (cúmulo bajo), por-encima
    (cúmulo alto) y picos espurios — sin necesidad de saber la rotación del sensor.
  - `keeper_xy_from_walls()`: Y de la pared propia (atrás); X de la pared lateral
    CERCANA + consistencia izq/der. Centrado/ambiguo (ambas leen ~lo mismo y la X no
    cierra) → `x_valid=false` (honesto). La conf la maneja la Y (eje confiable en el fondo).
- **`test/test_keeper_xy_walls/test_main.cpp`**: 12 tests Unity con **capturas REALES
  de banco** como fixtures (izq tocando, izq 50 cm, der con picos, atrás tocando, +
  casos de borde). **12/12 verde** (`run-host-tests.sh test_keeper_xy_walls`).
- **`src/top/localization_runtime.cpp`**: detrás de `-DTOP_KEEPER_XY_WALLS`, en el tick
  se reemplaza la trilateración por `compute_keeper_xy_pose()` (lee las 16 zonas crudas
  vía `sensors_tof_get_zone_mm`, reduce, arma `my_x/my_y`). El emisor del snapshot ya
  hace `conf = valid?70:0` → la CENTRAL lo consume **sin cambio de protocolo**. Default
  OFF → binario de competencia byte-idéntico.
- **`platformio.ini`**: env `top_robot2_arquero_xywalls` = `top_robot2_pri_hpredict` +
  `-DTOP_KEEPER_XY_WALLS`. Compila SUCCESS; `top_robot2_pri` (competencia) sin regresión.
- **`src/central/strategy.cpp`**: `GK_PATROL_XBOUND` (consumidor) — invierte el sentido
  de patrulla en el borde de la banda (±350 mm del centro CAPTURADO al arrancar) usando
  `world_model_get_my_x_mm()`, gateado por confianza ≥40. Sin pose confiable → rebote por
  línea (fallback de hoy). Zona central frente al arco, NO de arco a arco.

## Validación en vivo (banco, TOP flasheado con el env nuevo, lectura por serie COM15)
| Posición | Resultado | Veredicto |
|---|---|---|
| Esquina fondo-derecha (2 paredes cerca) | x=1496, y=313 | ✓ correcto |
| Alejándolo de la pared derecha | x 1496→1323 | ✓ trackea lateral |
| Centro lateral | x=910 (ambiguo→centro), y=378 | ✓ honesto (ambas laterales = piso → X inválida → default centro) |
| Fondo (varias) | y 313→489→378 | ✓ confiable |

- Con `hdg` paseando 0…−4.6°, **x/y no se movieron** → heading-free confirmado.
- **Artefacto encontrado:** el **cable USB del tether de debug** a la derecha del robot
  se leía como pared a ~30-40 cm → X falsa hacia la derecha. Sacándolo, el ToF derecho
  pasó a ver la pared lejana (~2200, "por encima") + piso. En partido NO hay cable
  (untethered) → no aplica; el test de motores del arquero va untethered.

## Límites conocidos (documentados, no son bugs)
- Pared a > ~1.3 m no se ve (rayo por encima/piso) → X solo confiable con pared CERCANA.
- Centro exacto (ambas laterales ~910) → X ambigua → default centro (Y sigue válida).
- Vale solo con el arquero **de frente**; durante el escape rota → la **congelación la
  decide la CENTRAL** (sabe el estado), pendiente si el banco lo pide.

## Pendiente
- **TASK-221**: validación HW del estimador + del arquero con motores (la cierra el equipo).
- `git pull` ya hecho en esta sesión (estábamos 1 atrás de ea4674f).
- Posible: congelar el consumo del XY en la CENTRAL durante el escape (si el banco lo pide).
