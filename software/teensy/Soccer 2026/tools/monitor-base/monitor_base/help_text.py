"""help_text.py — Texto de AYUDA del monitor (fuente única).

GUIDE: la guía/README que se muestra con `python -m monitor_base --readme`, dentro
de la app (panel Ayuda) y se vuelca a README.md. PANEL_HELP: ayuda CONTEXTUAL por
vista (el botón "?" del shell muestra la del panel activo). Una sola fuente para
no duplicar/derivar.
"""
from __future__ import annotations

GUIDE = """\
============================================================================
  MONITOR Y CALIBRACIÓN DEL ROBOT — IITA Soccer Open
============================================================================

UNA sola aplicación para MONITOREAR y CALIBRAR todo el robot (placa SUPERIOR/TOP
y placa BASE/DOWN). Se conecta sola al Teensy que encuentre, detecta qué placa es
por los datos, y si movés el USB de una placa a la otra cambia sola — sin perder
la historia de la otra.

----------------------------------------------------------------------------
CÓMO SE CORRE
----------------------------------------------------------------------------
  python -m monitor_base                 ← y nada más: autodetecta el puerto y la
                                           placa, abre la consola. (lo normal)
  python -m monitor_base --sim           ← sin robot (datos simulados, para probar)
  python -m monitor_base --port COM7     ← forzar un puerto
  python -m monitor_base --replay x.jsonl← reproducir una grabación
  python -m monitor_base --readme        ← muestra esta guía y sale
  python -m monitor_base --list-ports    ← lista los COM y cuál parece el Teensy

  Requisitos: pip install pyserial  (para el robot real; el sim no lo necesita).

----------------------------------------------------------------------------
HOT-SWAP DE PLACA (lo importante)
----------------------------------------------------------------------------
Con la app abierta podés DESCONECTAR el USB de la placa superior y CONECTARLO a la
placa base: la app lo detecta, reconecta sola y muestra las vistas de la placa base
(y deja calibrar). La historia/estelas de la placa superior NO se pierden — quedan
en sus vistas tal como estaban. Volvés a enchufar el TOP y seguís donde ibas.

La barra de arriba dice a qué placa estás conectado (TOP / BASE) y el estado del
enlace (verde = llegando datos, gris = esperando).

----------------------------------------------------------------------------
NAVEGACIÓN
----------------------------------------------------------------------------
Menú lateral, agrupado por placa:
  PLACA SUPERIOR (TOP) — Cancha · Polar · ToF 360 · Salud · Cámara · Timeline · Config ToF
  PLACA BASE (DOWN)    — Base (anillo + calibración) · Arquero (línea + OTOS)
  GENERAL              — Logs · Ayuda
Se cambia de vista sin reconectar. El botón "?" de arriba abre la ayuda de la vista
que estés mirando.

----------------------------------------------------------------------------
LAS VISTAS — placa SUPERIOR (TOP)
----------------------------------------------------------------------------
• CANCHA — el robot ubicado en la cancha (pose que el TOP manda a CENTRAL) con
  orientación + estela, y la pelota con estela. Opción "Comparar con OTOS" (cyan)
  para chequear cuán confiable es la pose. La app NO recalcula la pose: muestra la
  del snapshot; solo agrega la estela. La DISTANCIA de la pelota está sin calibrar
  (homografía pendiente, TASK-022): la dirección es fiable, la distancia no.
• POLAR — vista cenital robot-céntrica: el cono de cada ToF con la PROFUNDIDAD de
  cada zona, los conos de las cámaras, pelota/arcos. El reparto de columnas dentro
  del cono es aproximado (FOV/azimut sin confirmar por firmware).
• ToF 360 — las 4 grillas de zonas lado a lado (izq | frente | der | atrás) +
  cámaras arriba. "Arriba en la grilla ≠ adelante del robot" hasta que el firmware
  exponga el azimut-por-zona.
• SALUD — semáforo por sensor (verde OK / amarillo revisar / rojo falla / gris sin
  dato) + grilla de zonas + botones de config (apagar cámara/BNO/ToF, posición de
  ToF, guardar a EEPROM).
• CÁMARA — compara cámara frontal vs trasera vs fusión; avisa "FUSIÓN SOSPECHOSA"
  cuando discrepan mucho (la pelota fantasma del promedio).
• TIMELINE — caja-negra en vivo del snapshot a CENTRAL (heading válido, pelota,
  árbitro, obstáculo…) en el tiempo + tablero de "flapping".
• CONFIG ToF — acomodar los ToF en su lugar (posición, rotar, espejar) y vetar
  zonas (las de arriba ven por encima de la pared). Modelo pared+cancha que sugiere
  qué filas apagar. Guarda a .json y baja a la placa (lo que el firmware ya soporta;
  el resto queda listo para cuando el firmware lo agregue).

----------------------------------------------------------------------------
LAS VISTAS — placa BASE (DOWN)  [calibración]
----------------------------------------------------------------------------
• BASE — el anillo de 32 sensores de línea en su geometría real, la línea detectada,
  la odometría OTOS, y la CALIBRACIÓN: capturar verde/blanco, sensibilidad global y
  por sensor, habilitar/deshabilitar sensores, guardar a EEPROM. Acá se calibra la
  línea contra el verde del campo y el blanco real, en el lugar.
• ARQUERO — medidor de cross-track + anillo trasero + estela de OTOS izq/der, para
  probar en banco que el seguidor de línea avanza derecho.

----------------------------------------------------------------------------
LOGS
----------------------------------------------------------------------------
Historial de eventos: conexión/hot-swap, comandos enviados, y ANOMALÍAS que la app
detecta sola (heading perdido, snapshot inválido, cambios de árbitro, pelota
fantasma) + un latido de datos. Filtros por nivel, exportar a .txt, y la franja del
último frame. Botón ⏺ grabar (arriba) guarda toda la sesión a .jsonl para replay.

----------------------------------------------------------------------------
NOTAS HONESTAS
----------------------------------------------------------------------------
- Es herramienta de monitoreo/calibración; NO toca el binario de competencia.
- Lo que dependa de la homografía de cámara sin calibrar (posición absoluta de
  pelota/arcos en la cancha) se muestra como ESTIMADO hasta cerrar TASK-022.
- El FOV de los ToF y el azimut-por-zona los está agregando el firmware en paralelo;
  hasta entonces los conos/orientación del polar son aproximados.
============================================================================
"""


# Ayuda CONTEXTUAL por vista (clave = panel.key). El botón "?" del shell muestra la
# de la vista activa. Texto corto, "qué estoy viendo y cómo se lee".
PANEL_HELP = {
    "cancha":
        "VISTA DE CANCHA\n\n"
        "El robot (triángulo verde) está ubicado en la cancha con la POSE que el TOP "
        "manda a CENTRAL (snapshot my_x/y/heading). La app NO recalcula: muestra ese "
        "dato y le suma la ESTELA del recorrido.\n\n"
        "• Estela verde = recorrido del robot.\n"
        "• Pelota naranja + estela = posición de la pelota (relativa, ubicada con la "
        "pose). Su DIRECCIÓN es fiable; la DISTANCIA está sin calibrar (TASK-022).\n"
        "• Checkbox 'Comparar con OTOS' = dibuja en cyan la pose de la odometría para "
        "ver cuán confiable es la del snapshot (compará la FORMA, no el número).\n"
        "• Si dice 'sin odometría' es porque esa placa no tiene OTOS (R2).",
    "polar":
        "VISTA POLAR (cenital, robot-céntrica)\n\n"
        "El robot al centro, frente hacia arriba.\n"
        "• Cada ToF dibuja su CONO en la dirección donde está montado; los puntos de "
        "colores dentro del cono son la PROFUNDIDAD de cada zona (rojo cerca, verde "
        "lejos).\n"
        "• Conos punteados = cámaras (frontal arriba, trasera abajo) con pelota/arcos.\n"
        "• Flecha desde la pelota = su velocidad.\n"
        "OJO: el FOV real y el ángulo exacto de cada zona los expondrá el firmware; el "
        "reparto de columnas hoy es aproximado.",
    "tof360":
        "TIRA 360 DE LOS ToF\n\n"
        "Las 4 grillas 4×4 de los ToF, una al lado de la otra en orden físico: "
        "IZQUIERDO | FRONTAL | DERECHO | TRASERO. Cada celda = una zona; color por "
        "distancia (rojo cerca, verde lejos, gris sin lectura) con el valor en mm.\n\n"
        "Arriba del panel frontal/trasero está lo que ve cada cámara (pelota/arcos).\n"
        "AVISO: las 4 grillas están alineadas ENTRE SÍ, pero 'arriba en la grilla' NO "
        "es necesariamente 'adelante del robot' hasta que el firmware exponga el "
        "azimut-por-zona.",
    "salud":
        "SALUD POR SENSOR\n\n"
        "Un semáforo por sensor: VERDE ok, AMARILLO revisar, ROJO falla, GRIS sin dato. "
        "Sirve para ver de un vistazo qué sensor anda y cuál miente.\n\n"
        "A la derecha, las zonas de cada ToF (mismo color por distancia).\n\n"
        "CONFIG (escribe a la placa): apagar/prender cámara, BNO, ultrasonido, ToF; "
        "fijar la POSICIÓN de cada ToF; y guardar/recargar/resetear la config en EEPROM. "
        "Estos botones MANDAN comandos al robot (en simulador no envían nada).",
    "cam":
        "FUSIÓN DE CÁMARA (anti-pelota-fantasma)\n\n"
        "Compara lo que ve la cámara FRONTAL vs la TRASERA vs el resultado FUSIONADO: "
        "pelota y arcos de cada una.\n\n"
        "Cuando la pelota de las dos cámaras discrepa mucho, el promedio fusionado "
        "'teletransporta' la pelota (pelota fantasma) y la app marca 'FUSIÓN "
        "SOSPECHOSA' en rojo. Útil para diagnosticar ese bug de visión.",
    "timeline":
        "TIMELINE / CAJA-NEGRA DEL SNAPSHOT\n\n"
        "Tira temporal de las señales que el TOP manda a CENTRAL: snapshot válido, "
        "pelota visible, heading válido, arcos, obstáculo mínimo, árbitro.\n\n"
        "Sirve para cazar PARPADEOS (flapping) de visibilidad y CAÍDAS del heading en "
        "el tiempo. El tablero de 'flapping' cuenta cuántas veces cambió cada señal.",
    "tofcfg":
        "CONFIGURAR LOS ToF\n\n"
        "Acomodar cada ToF y vetar zonas, desde la app.\n"
        "• Por cada ToF: su POSICIÓN (frente/der/atrás/izq), ROTAR y ESPEJAR (corregir "
        "el montaje), y on/off.\n"
        "• CLICK en una zona = vetarla (las filas de arriba suelen ver por encima de la "
        "pared → detecciones falsas de afuera).\n"
        "• Panel PARED+CANCHA: cargás altura de pared y tamaño de cancha y sugiere qué "
        "filas apagar.\n"
        "• Guardar/Cargar .json. 'Bajar a la placa' manda lo que el firmware ya entiende "
        "(POS/ON-OFF) y deja listo el resto (rotar/espejar/vetar zona) para cuando el "
        "firmware lo soporte.",
    "base":
        "PLACA BASE — anillo de línea + CALIBRACIÓN\n\n"
        "Los 32 sensores de línea en su geometría real del PCB; cada uno coloreado por "
        "su lectura, resaltado si ve blanco, en rojo si está muerto/pegado.\n"
        "• Flecha = línea detectada (ángulo/penetración). Panel OTOS = odometría.\n"
        "• CALIBRACIÓN (escribe a la placa): poné el robot sobre el VERDE y capturá "
        "carpet; sobre la LÍNEA y capturá blanco; ajustá la sensibilidad global o por "
        "sensor; deshabilitá un sensor que molesta; guardá en EEPROM. Así se calibra la "
        "línea contra el campo real, en el lugar.",
    "arquero":
        "PLACA BASE — vista ARQUERO (línea + OTOS)\n\n"
        "Para probar en banco el seguidor de línea del arquero: un medidor grande de "
        "CROSS-TRACK (mm a la línea, objetivo 0), el arco trasero del anillo resaltado, "
        "y la estela de la trayectoria con cada OTOS izq/der por separado (para ver el "
        "diferencial / que avance derecho).",
    "logs":
        "LOGS\n\n"
        "Historial de eventos del monitor: conexión/hot-swap de placa, comandos "
        "enviados, y ANOMALÍAS detectadas solas (heading perdido, snapshot inválido, "
        "cambios de árbitro, pelota fantasma) + un latido de datos.\n\n"
        "Filtrá por nivel (info/ok/warn/bad/cmd), pausá el autoscroll, exportá a .txt. "
        "La franja de arriba muestra el último frame recibido. Para guardar TODA la "
        "sesión y reproducirla después, usá ⏺ grabar (barra superior).",
    "ayuda":
        "AYUDA\n\nLa guía completa de la app. La misma que ves con "
        "'python -m monitor_base --readme' y en README.md del repo.",
}


def panel_help(key: str) -> str:
    return PANEL_HELP.get(key, "Sin ayuda para esta vista todavía.")
