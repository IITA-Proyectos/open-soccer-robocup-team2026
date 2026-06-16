"""tooltips_text.py — Textos de los tooltips de ayuda contextual (hover).

Contenido (no mecanismo: el globo lo dibuja tooltip.py). Cada texto explica PARA
QUÉ SIRVE el control, en lenguaje de estudiante, y se VERIFICÓ contra el firmware
(auditoría 2026-06-15: 3 agentes leyeron paneles + parsers de comandos). <=140 char.

Clave:
  • comando (string que manda al robot) cuando el control manda algo — así el sitio
    de llamada pasa el comando que ya tiene (ej. tip("CAL CARPET")).
  • clave descriptiva "panel.control" para los controles de SOLO VISTA (no mandan
    nada): filtros de log, checkboxes, botones de la barra, etc.
"""
from __future__ import annotations

TOOLTIPS = {
    # ── Base (DOWN): calibración de la línea ────────────────────────────────
    "CAL CARPET": "Paso 1 de calibración: apoyá el robot sobre el VERDE (sin la "
                  "línea) y tocá. Aprende cómo se ve el piso. Después: paso 2 (BLANCO).",
    "CAL WHITE": "Paso 2: poné los sensores sobre la LÍNEA BLANCA y tocá. Aprende a "
                 "reconocer la línea. Hacelo después del paso 1 (VERDE).",
    "CAL SAVE": "Paso 3: guarda la calibración y los ajustes para que sobrevivan al "
                "apagado. Hacelo SIEMPRE al terminar, o perdés todo.",
    "CAL AUTO ON": "Calibración automática: tocá y paseá el robot por verde Y por "
                   "blanco; aprende solo. Cerrá con 'Auto OFF'. Alternativa a hacerlo a mano.",
    "CAL AUTO OFF": "Cierra la auto-calibración y la aplica. ¡Paseá el robot por verde "
                    "Y blanco ANTES! Si no, la rechaza. Luego tocá GUARDAR.",
    "CAL LOAD": "Recarga la calibración guardada en el robot y descarta los cambios "
                "sin guardar. Usalo para deshacer si arruinaste la calibración.",
    "OTOS RESET": "Pone en (0,0) la posición/odometría OTOS del robot. Usalo antes de "
                  "una prueba para medir desde cero. No afecta la línea.",
    "base.sens_global": "Ajusta de golpe qué tan fácil TODOS los sensores ven blanco. "
                        "+ = más estricto (menos falsos), − = más sensible. Acordate de GUARDAR.",
    "base.sens_persensor": "Ajusta la sensibilidad de UN solo sensor (el seleccionado). "
                           "Para emparejar uno que ve de más o de menos. Primero clickeá un sensor.",
    "base.sensor_toggle": "Apaga (o vuelve a prender) el sensor elegido. Apagá uno roto "
                          "o que da falsos. Se marca con X roja y deja de usarse para la línea.",
    "base.ring": "Click = ver detalles del sensor en el Inspector. Doble click = "
                 "prender/apagar ese sensor. El dibujo muestra qué sensores ven blanco en vivo.",
    "base.bars": "Click = seleccionar sensor. Doble click = prenderlo/apagarlo. Barra a "
                 "la derecha = ve blanco; izquierda = ve piso; el borde indica si separa bien.",

    # ── Arquero (DOWN) ──────────────────────────────────────────────────────
    "arquero.clear": "Borra el rastro dibujado de la trayectoria OTOS para observar de "
                     "nuevo. Solo afecta el dibujo, no toca el robot.",

    # ── Salud (TOP): config que escribe a la placa ──────────────────────────
    "CAM F ON": "Prende/apaga la cámara delantera del robot. Apagala solo para probar "
                "qué ve sin ella; dejala ON para jugar.",
    "CAM B ON": "Prende/apaga la cámara trasera del robot. Dejala ON para jugar; "
                "apagala solo para diagnosticar.",
    "BNO L ON": "Prende/apaga la brújula (giroscopio) PRINCIPAL que da el rumbo del "
                "robot. No la apagues en partido: el robot se desorienta.",
    "BNO R ON": "Prende/apaga la brújula (giroscopio) de RESPALDO. Tocala solo para "
                "diagnosticar cuál de las dos falla.",
    "US ON": "Prende/apaga el sensor de ultrasonido (mide distancia por eco). 'US' = "
             "ultrasonido. Dejalo ON salvo que estés probando.",
    "TOF ON": "Prende/apaga un sensor láser de distancia (ToF) que mide paredes. "
              "Apagalo solo si ese sensor da datos malos.",
    "TOF POS": "Indica hacia dónde mira ese láser (adelante/derecha/atrás/izq). El "
               "cambio se aplica recién tras Guardar EEPROM + reiniciar el robot.",
    "CFG SAVE": "Guarda la config actual en la memoria del robot (sobrevive al "
                "apagado). Tocalo después de dejar todo como lo querés.",
    "CFG LOAD": "Recarga la config guardada en el robot y descarta los cambios sin "
                "guardar. Útil para deshacer antes de Guardar EEPROM.",
    "CFG RESET": "Vuelve la config de sensores a fábrica (todo ON), solo en memoria. Si "
                 "después tocás Guardar EEPROM, perdés la config previa.",

    # ── Config ToF (TOP) ────────────────────────────────────────────────────
    "tofset.pos": "Indica hacia dónde mira ese láser. No viaja al robot al instante: se "
                  "baja con 'Bajar a la placa'. El efecto pleno es tras reiniciar.",
    "tofset.zonegrid": "Clic en un cuadradito veta/activa esa zona del láser. Hoy es SOLO "
                       "visual: el robot todavía no aplica el veto de zonas.",
    "tofset.rot": "Rota cómo se DIBUJA la grilla del láser para que matchee el montaje. "
                  "Solo visual en la app: el robot no aplica la rotación hoy.",
    "tofset.flip": "Espeja cómo se DIBUJA la grilla del láser (h/v). Solo visual en la "
                   "app: el robot no aplica el espejo hoy.",
    "tofset.onoff": "Prende/apaga ese láser. No viaja al instante: se baja con 'Bajar a "
                    "la placa'. Apagalo solo si el sensor da datos malos.",
    "tofset.fields": "Medidas de la cancha y del montaje del láser. Las usa el botón "
                     "'Sugerir' para calcular qué filas ven por encima de la pared.",
    "tofset.suggest": "Calcula y tacha las filas del láser que miran por encima de la "
                      "pared (ruido de afuera). Solo visual: el robot no aplica el veto hoy.",
    "tofset.resetzones": "Re-activa todas las zonas tachadas en el dibujo. Solo en la "
                         "app: no manda nada al robot.",
    "tofset.savejson": "Guarda la config de ToF en un archivo en TU compu (no en el "
                       "robot). Útil para no perder el trabajo del banco.",
    "tofset.loadjson": "Carga una config de ToF desde un archivo de tu compu. No la "
                       "manda al robot hasta tocar 'Bajar a la placa'.",
    "tofset.push": "Manda al robot posición + ON/OFF de cada láser y los graba. "
                   "Rotación/espejo/veto NO se bajan: el firmware aún no los soporta.",

    # ── Cancha (TOP) ────────────────────────────────────────────────────────
    "field.otos": "Dibuja en celeste la posición según la odometría de la base, encima "
                  "de la verde, para comparar. En R2 (sin OTOS) no muestra nada. Solo vista.",
    "field.clear": "Limpia los rastros dibujados del robot y la pelota para empezar el "
                   "recorrido de cero. No afecta al robot, es solo el dibujo.",

    # ── Logs ────────────────────────────────────────────────────────────────
    "logs.TODO": "Muestra todos los mensajes del log, sin filtrar. Para volver a la "
                 "vista completa. Solo afecta lo que ves, no al robot.",
    "logs.info": "Muestra solo los mensajes informativos (latidos de datos, eventos "
                 "normales). Filtro de vista, no afecta al robot.",
    "logs.ok": "Muestra solo los mensajes buenos/verdes (conexión OK, recuperaciones). "
               "Filtro de vista, no afecta al robot.",
    "logs.warn": "Muestra solo las advertencias (amarillo): cosas raras a revisar como "
                 "heading inválido o pelota fantasma. Filtro de vista.",
    "logs.bad": "Muestra solo los errores (rojo): fallos de enlace o envío. Mirá esto "
                "primero si algo no anda. Filtro de vista.",
    "logs.cmd": "Muestra solo los comandos que la app le envió al robot (→). Útil para "
                "auditar qué se mandó. Filtro de vista, no envía nada.",
    "logs.autoscroll": "Prende/apaga que el log siga solo lo último. Apagalo para leer "
                       "mensajes viejos sin que la lista se mueva. No afecta al robot.",
    "logs.clear": "Vacía la lista de mensajes del log de la app. No borra nada en el "
                  "robot. Útil para empezar limpio antes de una prueba.",
    "logs.export": "Guarda el log en un archivo .txt (con fecha y hora) para revisarlo "
                   "o compartirlo después. No afecta al robot.",

    # ── Barra superior (shell) ──────────────────────────────────────────────
    "shell.pause": "Congela la pantalla para mirar un instante. El robot sigue andando "
                   "igual; solo se pausa la app. Tocala de nuevo para reanudar.",
    "shell.record": "Guarda toda la telemetría en un archivo para revisarla o "
                    "reproducirla luego. No afecta al robot. Tocala de nuevo para detener.",
    "shell.help": "Abre la ayuda de la vista actual: qué muestra y cómo leerla (también "
                  "con F1). Cambia según la vista. No afecta al robot.",
}


def tip(key: str) -> str:
    """Texto del tooltip para `key` (un comando o una clave 'panel.control'), o '' si
    no hay (así el sitio de llamada puede pasar siempre attach_tooltip(w, tip(k)))."""
    return TOOLTIPS.get(key, "")
