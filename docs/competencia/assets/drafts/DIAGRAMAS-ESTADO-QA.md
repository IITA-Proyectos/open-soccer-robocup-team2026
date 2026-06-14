---
title: "Estado QA de los diagramas (drafts) — catalogo de 21"
date: 2026-06-13
author: "Claude Opus 4.8 (Anthropic), via Claude Code"
requested-by: "Gustavo Viollaz (@gviollaz)"
status: vivo
tipo: indice-asset
---

# Estado QA de los diagramas del robot

> Estos son **drafts** (zona de borrador de la skill `rcj-diagramas-poster`). El equipo
> decide cuales suben al A1 y con que numero `Fig.N`. Cada figura debe pasar render + mirada
> (legible a 1,5 m, cuerpo >=24 pt) y llevar caption con `CC BY 4.0` + Fuente antes de imprimir.

## Hechos a mano y revisados — listos como draft

| ID | Archivo | Que muestra |
|----|---------|-------------|
| D1 | `fig_topologia_3placas` | Topologia 3 placas + COMM: que cuelga de cada placa + enlaces UART + bus de emergencia |
| D4 | `fig_fusion_snapshot` | Sensor crudo &#8594; campo del WorldSnapshot (prueba de "CENTRAL recibe ubicacion, no crudo") |
| D5 | `fig_electrico_distribucion` | Arbol de distribucion de potencia + brownout OTOS + panel de honestidad |
| D10 | `fig_layout_cobertura` | Vista superior: cobertura camaras/ToF/ultrasonido + zonas ciegas + ruedas + anillo |

Ya existian (de sesiones previas): `fig_funcionamiento_2modulos` (D2), `fig_capas_abstraccion`
(D3), `fig2_dataflow` (D7), `fig8_test_growth` (D17), `fig9_otos_error` (D18),
`fig_madurez_escalera` (D19), `fig_proceso_constructivo_timeline` (D20), `fig_roadmap_fases` (D21).

## Auto-generados 2026-06-13 (workflow) — PRIMERA PASADA, requieren pulido

> Generados por un workflow multi-agente y auditados por un revisor visual independiente.
> **Ninguno paso QA limpio**: son borradores correctos en CONTENIDO (verificado contra codigo
> vivo) pero con defectos de MAQUETADO. Punch-list por figura (alta = bloqueante para A1):

| ID | Archivo | Alta | Que corregir (lo principal) |
|----|---------|:----:|------------------------------|
| D6 | `fig_presupuesto_potencia` | 0 | Alinear el origen de todas las barras a una base comun; `*` a "500 mA (spec)"; reservar rojo solo al cap termico |
| D8 | `fig_presupuesto_temporal` | 2 | La cadena de freno (corazon) esta ilegible (cuerpo <28 px) y con solapes; agrandar pildoras + separar; rojo solo para el freno |
| D9 | `fig_frame_uart_errores` | 2 | Texto sale del borde derecho (ejemplo panel A); caja roja de resync solapa la fila de estados; bajar densidad |
| D11 | `fig_pila_placas_lateral` | 3 | Caption se sale del lienzo y FALTA `CC BY 4.0`/Fuente; etiquetas "XP1 CENTRAL/DOWN" desbordan su caja |
| D12 | `fig_fsm_dual` | 2 | Las notas ATK y GK se solapan (ilegibles); caption cortado y sin `CC BY`/Fuente |
| D13 | `fig_localizacion_fusion` | 0 | Densidad alta en columna central; recortar a titulo + 2 lineas; jerarquizar el rojo |
| D14 | `fig_pipeline_vision` | 2 | Caption cortado a media palabra y sin `CC BY`/Fuente |
| D15 | `fig_arbol_fallos` | 0 | Sobrecarga (>>7 unidades) + cuerpo chico + rojo masivo; reducir a matriz 6&#215;3, colorear por placa |
| D16 | `fig_r1_vs_r2` | 2 | Texto del panel GAP desborda; cuerpo chico; recortar detalle de pines a la TDP |

**Patron sistematico a arreglar en todas:** (1) caption SIEMPRE dentro del lienzo (x &lt;= 1380)
con `CC BY 4.0` + Fuente; (2) ningun texto/caja pasa el borde derecho; (3) cuerpo >= 28 px
(si no entra, CORTAR texto, no achicar); (4) rojo SOLO para emergencia/seguridad; (5) sin solapes.

> ⚠️ Posible **solapamiento con otra sesion Claude** que tambien genera figuras
> (`fig_arbol_salud`, `fig_zonas_tof_4x4` via `gen_arbol_salud.py`/`gen_zonas_tof.py`):
> `fig_arbol_salud` puede pisarse con D15 (`fig_arbol_fallos`) y `fig_zonas_tof_4x4` con la
> cobertura de D10/D14. Reconciliar antes de elegir cual va al A1.
