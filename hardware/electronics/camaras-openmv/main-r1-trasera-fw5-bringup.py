from pyb import UART
import sensor, image, time
import math
import pyb

uart = UART(3, 19200)

led_rojo = pyb.LED(1)
led_verde = pyb.LED(2)
led_azul = pyb.LED(3)

led_verde.on()
time.sleep(0.4)
led_verde.off()
time.sleep(0.4)
led_verde.on()
time.sleep(0.4)
led_verde.off()

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.VGA)

sensor.set_hmirror(True)
sensor.set_vflip(True)

# ----- Color/brillo -----
# ===== VALORES DE BRING-UP POCA LUZ (2026-07-23, pedido Gustavo) ===============
# OJO: NO son los de competencia. Los de cancha eran gain 12.041200 dB / exp 100328 us.
# Para volver a competencia, poner esos dos valores de nuevo.
#
# >>> MEDIDO EN LA PLACA el 2026-07-23 (readback real, no teoria) <<<
#     Se pidio  gain 20 dB     -> el sensor aplico 24.08 dB   (CLAMPEADO al TECHO)
#     Se pidio  exp  200000 us -> el sensor aplico  8248 us   (CLAMPEADO, 24x menos)
#
# GANANCIA: TECHO REAL = 24.08 dB (= 16x lineal). Confirmado en la placa y en el
#   driver (drivers/sensors/pag7936.c:152-157, MAX_AGAIN=16384; clamp en :755).
#   Pedir mas de 24.08 no hace nada y NO tira error. Ya estamos en el maximo fisico.
#
# EXPOSICION: TECHO REAL = 8248 us (8.2 ms) a VGA. Corresponde a ~121 fps de periodo
#   de frame: la exposicion NO puede superar el tiempo de un frame. Por eso 200 ms
#   se recorto a 8.2 ms.
#   >>> CONSECUENCIA IMPORTANTE PARA EL EQUIPO: el valor de "calibracion" de
#   competencia (exposure_us=100328) TAMPOCO se aplicaba NUNCA — tambien caia a
#   ~8 ms. Los 4 scripts de las camaras tienen ese numero y es ficcion. <<<
#
# CON AMBOS EN EL TECHO, el unico lever que queda es LUZ EN LA ESCENA (o bajar
# el frame rate del sensor para permitir exposiciones mas largas — a investigar).
GAIN_DB     = 24.0        # dB. TECHO del PAG7936 (24.08). Pedir mas no sirve.
EXPOSURE_US = 8248        # us. TECHO medido a VGA. Pedir mas se recorta en silencio.

# ===== MODO DE CALIBRACION ====================================================
# AUTO_CALIB = True  -> deja los lazos automaticos correr 2 s, LEE lo que el sensor
#                       encontro, CONGELA esos valores e imprime las 3 lineas listas
#                       para pegar. Los valores son alcanzables POR CONSTRUCCION.
#              False -> usa GAIN_DB / EXPOSURE_US / RGB_GAIN_DB de arriba.
#
# POR QUE IMPORTA: los 4 scripts de produccion del equipo hacen algo distinto y
# peor — dejan correr el auto 2 s y despues DESCARTAN lo que encontro para meter
# constantes a mano (gain 12.041200 / exp 100328). Por eso el 100328 sobrevivio
# meses sin que nadie notara que el sensor lo recortaba a ~8 ms: nunca se comparo
# lo pedido contra lo aplicado. Con AUTO_CALIB=True ese error es IMPOSIBLE, porque
# lo que se congela es lo que el sensor ya logro.
AUTO_CALIB   = True
RGB_GAIN_DB  = (2.803574, 0.000000, 5.535230)   # solo se usa si AUTO_CALIB = False
# OJO: en el PAG7936 set_auto_whitebal IGNORA los rgb_gain_db por completo
# (pag7936.c:806-809 solo hace stats_enabled = enable; return 0). Esos 6 decimales
# NUNCA se aplicaron en esta camara. Se deja la linea por compatibilidad con las
# otras 3 camaras, que pueden tener otro sensor donde SI funciona.

# ===== FRAMERATE: la palanca para subir el TECHO de exposicion ================
# El techo de exposicion NO es del silicio: es el tiempo de trama.
#   reset() deja framerate=120 (pag7936.c:541) -> 1000000/120 = 8333 us (:645)
#   -> clamp (8333-80)/8 = 1031 lineas -> x8 = 8248 us (:786, :801). Da exacto.
# Bajando el framerate sube el techo:
#     fps | techo expo |  x luz
#     120 |   8248 us  |  1.00x   (default, hoy)
#      60 |  16584 us  |  2.01x
#      50 |  19920 us  |  2.42x
#      30 |  33248 us  |  4.03x
#      15 |  66584 us  |  8.07x
#
# COSTO: BLUR. A 30 fps son 33 ms de obturacion: un robot a 1 m/s arrastra la
# pelota ~35 px sobre un blob de ~26 px -> la pelota se vuelve una raya y se
# rompen area y centroide de find_blobs. Para BANCO sirve; para JUGAR, no.
#
# TRAMPAS (las 3 estan resueltas abajo, no cambiar el orden):
#  1. configure() NO re-aplica la exposicion pedida: re-clampea la ACTUAL contra
#     el frame_time nuevo. Hay que fijar el framerate ANTES y volver a setear la
#     exposicion DESPUES.
#  2. sensor.reset() borra el framerate (vuelve a 120).
#  3. sensor.get_framerate() LANZA excepcion, no sirve de readback. El unico
#     readback valido es get_exposure_us().
#  NUNCA poner 0: omv_csi.c:645 divide por el.
SET_FRAMERATE = 0      # 0 = no tocar (120 fps default). Probar 60 / 30 en banco.
# ==============================================================================

if SET_FRAMERATE > 0:
    try:
        sensor.set_framerate(SET_FRAMERATE)
        print("framerate fijado en %d fps (techo de exposicion sube)" % SET_FRAMERATE)
    except Exception as e:
        print("set_framerate(%d) fallo -> %s (sigo a 120 fps)" % (SET_FRAMERATE, e))
# ==============================================================================

if AUTO_CALIB:
    # --- 1) Dejar que los tres lazos automaticos encuentren el punto ---
    sensor.set_auto_whitebal(True)
    sensor.set_auto_gain(True)
    sensor.set_auto_exposure(True)
    sensor.skip_frames(time=2000)          # 2 s: el AE necesita medir varios frames

    # --- 2) LEER lo que encontraron (esto es lo que los scripts viejos tiraban) ---
    _exp  = sensor.get_exposure_us()
    _gain = sensor.get_gain_db()
    _rgb  = sensor.get_rgb_gain_db()

    # --- 3) Congelar EXACTAMENTE eso ---
    sensor.set_auto_exposure(False, exposure_us=_exp)
    sensor.set_auto_gain(False, gain_db=_gain)
    sensor.set_auto_whitebal(False, rgb_gain_db=_rgb)
    sensor.skip_frames(time=500)

    GAIN_DB     = _gain
    EXPOSURE_US = _exp

    print("=" * 60)
    print("AUTO_CALIB — copiar estas 3 lineas si estos valores sirven:")
    print("sensor.set_auto_gain(False, gain_db=%f)" % _gain)
    print("sensor.set_auto_exposure(False, exposure_us=%d)" % _exp)
    print("sensor.set_auto_whitebal(False, rgb_gain_db=(%f, %f, %f))"
          % (_rgb[0], _rgb[1], _rgb[2]))
    print("=" * 60)
else:
    # White balance y ganancia en AUTO primero (esto es lo que evita la imagen negra).
    sensor.set_auto_whitebal(True)
    sensor.set_auto_gain(True)
    sensor.skip_frames(time=2000)

    sensor.set_auto_whitebal(False, rgb_gain_db=RGB_GAIN_DB)
    sensor.set_auto_gain(False, gain_db=GAIN_DB)
    sensor.set_auto_exposure(False, exposure_us=EXPOSURE_US)
    sensor.skip_frames(time=2000)

# ----- READBACK: que aplico REALMENTE el sensor (no lo que le pedimos) --------
# Critico: set_auto_gain NO tira excepcion si el sensor no puede aplicar el valor
# (py_csi_ng.c:544-549 solo hace un printf). Y la exposicion se clampea al periodo
# de frame. Si pedido != real, el sensor recorto y hay que saberlo.
print("=" * 52)
try:
    g_real = sensor.get_gain_db()
    print("ganancia : pedido %.2f dB -> real %.2f dB  %s"
          % (GAIN_DB, g_real, "OK" if abs(g_real - GAIN_DB) < 1.0 else "<<< CLAMPEADO"))
except Exception as e:
    print("ganancia : get_gain_db() fallo ->", e)
try:
    e_real = sensor.get_exposure_us()
    print("exposicion: pedido %d us -> real %d us  %s"
          % (EXPOSURE_US, e_real,
             "OK" if abs(e_real - EXPOSURE_US) < EXPOSURE_US * 0.1 else "<<< CLAMPEADO"))
except Exception as e:
    print("exposicion: get_exposure_us() fallo ->", e)
print("=" * 52)
# ------------------------------------------------------------------------------

clock = time.clock()
_dbg_n = 0        # contador para no inundar la consola con el print de debug

h = 82.5
r = 13.5/(2*math.pi)

naranja_threshold = (30, 61, 39, 70, 20, 50)
amarillo_threshold = (40, 65, 0, 20, 10, 30)
azul_threshold = (10, 30, 0, 30, -35, -10)

# ----- Máscara de esquinas superiores (triángulos) — tus valores de antes -----
MASK_TL_W = 220   # triángulo sup-IZQUIERDO: ancho desde la esquina (px)
MASK_TL_H = 50    # triángulo sup-IZQUIERDO: alto  desde el borde de arriba (px)
MASK_TR_W = 220   # triángulo sup-DERECHO:   ancho desde la esquina (px)
MASK_TR_H = 50    # triángulo sup-DERECHO:   alto  desde el borde de arriba (px)
MASK_COLOR = (0, 0, 0)   # negro: fuera de TODOS los thresholds LAB

SENTINEL_CODED = 255
COORD_OFFSET = 100
HEADER1, HEADER2, HEADER3 = 201, 202, 203
END_BYTE = 254

# ----- Corrección del eje Y - cámara R1_TRASERA -----
# La Y se vuelve más sensible cuanto más lejos está la pelota (perspectiva).
# Esta corrección (cúbica por el origen) endereza la Y: f(0)=0, no toca la
# pelota pegada al robot y corrige cada vez más lejos.
# Se aplica DESPUÉS del ajuste de altura Y = y*(h-r)/h.
# Ajustada a la tabla r1_trasera medida EN VIVO (5..90 cm):
#   leído:  6 14 28 39 49 56 61 67 74 78
#   real :  5 10 20 30 40 50 60 70 80 90
# RMSE 1.12 cm. Rango con datos: leído ~0..78 (real 0..90). Más allá NO extrapolar.
# f(L) = A*L^3 + B*L^2 + C*L   (sin término constante -> arranca en 0)
CORR_Y_A =  0.000090394
CORR_Y_B = -0.000886305
CORR_Y_C =  0.672463621

CORR_Y_MIN = 0.0
CORR_Y_MAX = 78.0


def corregir_y(Y_hr):
    # Entrada: Y ya ajustada por altura (y*(h-r)/h). Salida: Y corregida en cm.
    # Horner: ((A*L + B)*L + C)*L  ==  A*L^3 + B*L^2 + C*L
    Yc = ((CORR_Y_A * Y_hr + CORR_Y_B) * Y_hr + CORR_Y_C) * Y_hr
    return Yc


# ===== SHIM DE COMPATIBILIDAD DE FIRMWARE (agregado 2026-07-23) =====
# Este script tiene que correr en DOS firmwares distintos que conviven en el equipo:
#   - OpenMV 4.8.1  (cámaras r1-frontal, r2-frontal, r2-trasera)
#   - OpenMV 5.0.0  (cámara r1-trasera, OpenMV N6 / sensor PAG7936)
#
# En 4.8.1 el blob es una CLASE: sus campos son MÉTODOS  -> b.cx()  b.rect()
#   (modules/py_image.c:4396-4426, py_blob_locals_dict_table)
# En 5.0.0 el blob es un ATTRTUPLE: sus campos son ATRIBUTOS -> b.cx  b.rect
#   (modules/py_image.c:3376 blob_fields[] y :3473 mp_obj_new_attrtuple)
# Si en 5.0.0 escribís b.cx() explota con "TypeError: 'int' object isn't callable".
#
# _bv() resuelve las dos formas con una sola línea de código: si el campo es
# llamable (4.8.1) lo llama; si ya es el valor (5.0.0) lo devuelve tal cual.
# 'callable' existe en las dos versiones de MicroPython (v1.24 y v1.28).
def _bv(v):
    return v() if callable(v) else v
# OJO para el resto de los scripts del equipo: NUNCA indexar el blob por número
# (blob[4]). En 4.8.1 el índice 4 es 'pixels' y en 5.0.0 es 'cx' -> devuelve el
# número equivocado SIN tirar error. Siempre por nombre + _bv().
# ===================================================================


# ===== REALCE DE BRILLO — SOLO PARA LA PANTALLA ==============================
# Se aplica AL FINAL del loop, DESPUES de find_blobs, asi la deteccion sigue
# trabajando sobre los valores ORIGINALES y los thresholds LAB no se tocan.
#
# HONESTIDAD: esto NO agrega informacion. La camara ya esta con ganancia y
# exposicion en el TECHO fisico (24.08 dB / 8248 us, medido). Aclarar por
# software solo re-mapea los valores que ya hay, y amplifica el ruido igual que
# la señal. Sirve para VER mejor en el IDE, no para detectar mejor.
# La unica forma real de mejorar la deteccion es MAS LUZ EN LA ESCENA.
DISPLAY_BOOST  = True     # False = ver la imagen cruda, tal cual la capta el sensor
BOOST_GAMMA    = 2.0      # >1 ACLARA. MEDIDO en banco 2026-07-23: con 0.6 la imagen
                          # se oscurecio (L_ver 14 < L_esc 19) -> la convencion es
                          # gamma>1 = mas claro. Subir a 2.5/3.0 si falta.
BOOST_CONTRAST = 1.2
BOOST_BRIGHT   = 0.10

BOOST_AFECTA_DETECCION = False   # EXPERIMENTAL. True = realza ANTES de find_blobs.
# Con True la deteccion podria empezar a andar con poca luz (los thresholds piden
# L 30..61 y hoy la escena da 20), PERO gamma es NO LINEAL: tambien corre los
# canales A y B, y los thresholds fueron calibrados sin este realce. Si se activa,
# hay que RE-CALIBRAR los thresholds. Default False = no tocar lo que ya anda.

# ===== SONDA LAB DEL CENTRO — la herramienta que faltaba =====================
# POR QUE: comparar el L MEDIO DE LA ESCENA contra el threshold de la PELOTA es
# invalido. La media de un escritorio oscuro no dice nada de los pixeles de la
# pelota. Y lo que separa el naranja del verde de cancha NO es L: es el canal A
# (el threshold pide A entre 39 y 70 = fuertemente rojo; el verde tiene A < 0).
#
# USO: apunta la camara para que la PELOTA llene el cuadrito del centro y mira
# el print. Te dice el L/A/B REAL de la pelota, con [OK] o [FUERA] por canal
# contra naranja_threshold. Con eso se ajusta el threshold CON DATOS.
PROBE_LAB  = True
PROBE_SIZE = 60           # lado del cuadrito de sondeo, en px
# ==============================================================================

_boost_ok = None          # None = sin probar, True = anda, False = no existe la API


def aclarar(img):
    # Devuelve True si aplico el realce. Shim: prueba la API y se acuerda.
    global _boost_ok
    if _boost_ok is False:
        return False
    try:
        img.gamma_corr(gamma=BOOST_GAMMA, contrast=BOOST_CONTRAST,
                       brightness=BOOST_BRIGHT)
        _boost_ok = True
        return True
    except Exception as e:
        if _boost_ok is None:
            print("aclarar: gamma_corr no disponible (%s) -> sigo sin realce" % e)
        _boost_ok = False
        return False
# ==============================================================================


# ----- DEBUG / CAPTURA -----
DEBUG_PRINT = True    # imprime la coord. de la pelota ya corregida (para verificar)
RECAL_PRINT = False   # ponelo True cuando recalibres: imprime (cx,cy) crudos de la pelota


def crc8(data):
    c = 0
    for b in data:
        c ^= b
    return c & 0xFF


def transformarcoordenadas(u, v):

    # Camara delantera robot 1
    H = [[ 9.75710406e-01, -1.68298485e-02, -3.10158606e+02],
         [ 4.96930331e-02, -7.50443823e-01,  3.19229639e+02],
         [ 5.35796634e-03,  7.17775346e-02,  1.00000000e+00]]
    denominator = H[2][0]*u + H[2][1]*v + H[2][2]
    if abs(denominator) < 1e-6:
        return SENTINEL_CODED, SENTINEL_CODED
    x = (H[0][0]*u + H[0][1]*v + H[0][2]) / denominator
    y = (H[1][0]*u + H[1][1]*v + H[1][2]) / denominator
    X = x * (h - r) / h
    Y = y * (h - r) / h
    # ---- Corrección del eje Y (después del ajuste de altura) ----
    Y = corregir_y(Y)
    # X queda intacta: ese eje ya anda bien.
    X = max(-100, min(100, X))
    Y = max(-100, min(100, Y))
    X_coded = max(0, min(200, round(X) + COORD_OFFSET))
    Y_coded = max(0, min(200, round(Y) + COORD_OFFSET))
    return X_coded, Y_coded


def decodificar_coordenada(x_coded, y_coded):
    if x_coded == SENTINEL_CODED or y_coded == SENTINEL_CODED:
        return None, None
    return x_coded - COORD_OFFSET, y_coded - COORD_OFFSET


def enmascarar_esquinas(img):
    # Pinta de negro las dos esquinas triangulares de arriba (relleno por filas).
    w = img.width()
    for y in range(0, MASK_TL_H):
        span = int(MASK_TL_W * (1 - y / MASK_TL_H))
        if span > 0:
            # CAMBIO fw5.0.0: draw_line exige los 4 puntos en UNA TUPLA.
            # 4.8.1 aceptaba x0,y0,x1,y1 sueltos (py_helper_consume_array);
            # 5.0.0 usa py_helper_get_array_min_n -> con un int tira
            # "TypeError: object 'int' isn't a tuple or list" (este era el crash).
            # La tupla de 4 funciona en LAS DOS versiones.
            img.draw_line((0, y, span, y), color=MASK_COLOR)
    for y in range(0, MASK_TR_H):
        span = int(MASK_TR_W * (1 - y / MASK_TR_H))
        if span > 0:
            # CAMBIO fw5.0.0: idem arriba, los 4 puntos en una tupla.
            img.draw_line((w - span, y, w, y), color=MASK_COLOR)


def procesar_blob(blobs, dibujar_color):
    if not blobs:
        return SENTINEL_CODED, SENTINEL_CODED
    # CAMBIO fw5.0.0: b.pixels() -> _bv(b.pixels)  (en 5.0.0 'pixels' es atributo)
    largest_blob = max(blobs, key=lambda b: _bv(b.pixels))
    # CAMBIO fw5.0.0: .rect() -> _bv(.rect). En 5.0.0 'rect' YA ES la tupla (x,y,w,h);
    # ponerle paréntesis da "TypeError: 'tuple' object isn't callable".
    # NO pasar el blob crudo a draw_rectangle: anda en 5.0.0 pero rompe en 4.8.1.
    img.draw_rectangle(_bv(largest_blob.rect), color=dibujar_color)
    # CAMBIO fw5.0.0: dos cambios en una línea:
    #   1) cx/cy son atributos en 5.0.0 -> _bv()
    #   2) draw_cross exige tupla de EXACTAMENTE 2 (mp_obj_get_array_fixed_n n=2);
    #      los enteros sueltos ya no se aceptan. La tupla sirve en 4.8.1 también.
    img.draw_cross((_bv(largest_blob.cx), _bv(largest_blob.cy)), color=(255, 255, 255))
    # CAMBIO fw5.0.0: mismos accesores para las coordenadas que se envían.
    return transformarcoordenadas(_bv(largest_blob.cx), _bv(largest_blob.cy))


while(True):
    clock.tick()
    img = sensor.snapshot()

    # Tapar las esquinas triangulares de arriba (falsos positivos fuera de cancha).
    enmascarar_esquinas(img)

    # L que ve la DETECCION (antes de cualquier realce). Es el numero que hay que
    # comparar contra el threshold del naranja (L 30..61).
    _L_det = _bv(img.get_statistics().l_mean)

    # EXPERIMENTAL: realzar ANTES de detectar (default OFF). Ver nota en el bloque.
    if DISPLAY_BOOST and BOOST_AFECTA_DETECCION:
        aclarar(img)
        _L_det = _bv(img.get_statistics().l_mean)

    roi = (0, 0, img.width(), int(img.height() * 0.97))

    # SIN CAMBIOS: en 5.0.0 todos los parámetros de find_blobs salvo 'thresholds'
    # son KW_ONLY. Este código ya los pasa por nombre -> compatible con las dos.
    # REGLA para el futuro: nunca pasarlos posicionalmente.
    naranja_blobs = img.find_blobs([naranja_threshold], roi=roi, pixels_threshold=7, area_threshold=7, merge=True)
    azul_blobs = img.find_blobs([azul_threshold], roi=roi, pixels_threshold=600, area_threshold=600, merge=True)
    amarillo_blobs = img.find_blobs([amarillo_threshold], roi=roi, pixels_threshold=600, area_threshold=600, merge=True)

    led_rojo.off()
    if naranja_blobs:
        led_rojo.on()

    led_azul.off()
    if azul_blobs:
        led_azul.on()

    led_verde.off()
    if amarillo_blobs:
        led_verde.on()

    Xp, Yp = procesar_blob(naranja_blobs, (255, 0, 0))
    Xam, Yam = procesar_blob(amarillo_blobs, (0, 255, 0))
    Xaz, Yaz = procesar_blob(azul_blobs, (0, 0, 255))

    # CAMBIO fw5.0.0 (línea comentada, portada igual para que no explote el día
    # que la descomenten): los 4 puntos de draw_line van en una tupla.
    #img.draw_line((img.width() // 2, 0, img.width() // 2, img.height()), color=(255, 255, 255), thickness=1)

    data = [HEADER1, Xp, Yp,
            HEADER2, Xam, Yam,
            HEADER3, Xaz, Yaz]
    packet = bytearray(data + [crc8(data), END_BYTE])

    uart.write(packet)

    # ----- CAPTURA para recalibrar: (cx,cy) crudos de la pelota -----
    # Poné la pelota en una posición conocida (X,Y) y anotá el cx,cy que sale acá.
    # Repetí hasta cubrir todo el rango (incluí 70/80/90/100 cm y los costados).
    # Esos pares (cx,cy)->(X,Y) son los que cargás en pts_img / pts_fisicas del Colab.
    if RECAL_PRINT and naranja_blobs:
        # CAMBIO fw5.0.0: bb.pixels() -> _bv(bb.pixels)
        b = max(naranja_blobs, key=lambda bb: _bv(bb.pixels))
        # CAMBIO fw5.0.0: b.cx()/b.cy() -> _bv(b.cx)/_bv(b.cy)
        print("RECAL pelota -> cx=%d  cy=%d" % (_bv(b.cx), _bv(b.cy)))

    # ----- DEBUG: pelota corregida + BRILLO (para saber si la exposicion alcanza) --
    # El L medio es el diagnostico de "hay luz suficiente": el naranja pide L 30..61.
    # Si L_medio queda muy por debajo de 30, NO es que no detecte: es que esta oscuro.
    # Se imprime cada 10 frames para no inundar la consola.
    # ---- Cuadrito de la SONDA LAB, para saber donde poner la pelota ----
    if PROBE_LAB:
        img.draw_rectangle((img.width() // 2 - PROBE_SIZE // 2,
                            img.height() // 2 - PROBE_SIZE // 2,
                            PROBE_SIZE, PROBE_SIZE), color=(255, 255, 0))

    # ---- REALCE PARA LA PANTALLA: al FINAL, despues de detectar y de dibujar ----
    # El IDE muestra el framebuffer al terminar la iteracion, asi que tocarlo aca
    # cambia SOLO lo que se ve. find_blobs ya corrio sobre la imagen original.
    if DISPLAY_BOOST and not BOOST_AFECTA_DETECCION:
        aclarar(img)

    if DEBUG_PRINT:
        _dbg_n += 1
        if _dbg_n >= 10:
            _dbg_n = 0
            # CAMBIO fw5.0.0: get_statistics() devuelve un ATTRTUPLE igual que el blob,
            # asi que l_mean es ATRIBUTO, no metodo. Con .l_mean() explota con
            # "TypeError: 'int' object isn't callable". _bv() resuelve las 2 versiones.
            _L_ver = _bv(img.get_statistics().l_mean)   # lo que se VE en pantalla
            F = _bv(clock.fps)          # idem por las dudas: fps podria ser atributo
            Xp_real, Yp_real = decodificar_coordenada(Xp, Yp)
            # L_det = lo que decide la deteccion (comparar contra 30..61 del naranja)
            # L_ver = lo que se ve en el IDE. Si L_ver < L_det, el gamma esta INVERTIDO
            #         -> cambiar BOOST_GAMMA de 0.6 a 1.6.
            _est = "Pelota: NO DETECTADA" if Xp_real is None else \
                   ("Pelota -> X=%d cm  Y=%d cm" % (Xp_real, Yp_real))
            print("L_esc=%3d  L_ver=%3d  fps=%4.1f  |  %s"
                  % (_L_det, _L_ver, F, _est))

            # ---- SONDA LAB: lo que hay en el CUADRITO DEL CENTRO ----
            # Este es el numero que importa. Poner la pelota ahi adentro.
            if PROBE_LAB:
                _s = sensor.snapshot() if False else img   # usar el frame actual
                _pw = _s.width()
                _ph = _s.height()
                _pr = (_pw // 2 - PROBE_SIZE // 2, _ph // 2 - PROBE_SIZE // 2,
                       PROBE_SIZE, PROBE_SIZE)
                _st = _s.get_statistics(roi=_pr)
                _pl = _bv(_st.l_mean)
                _pa = _bv(_st.a_mean)
                _pb = _bv(_st.b_mean)
                _t = naranja_threshold      # (Lmin,Lmax, Amin,Amax, Bmin,Bmax)
                _ok = lambda v, lo, hi: "OK   " if (lo <= v <= hi) else "FUERA"
                print("   SONDA centro -> L=%4d [%s %d..%d]  A=%4d [%s %d..%d]  B=%4d [%s %d..%d]"
                      % (_pl, _ok(_pl, _t[0], _t[1]), _t[0], _t[1],
                         _pa, _ok(_pa, _t[2], _t[3]), _t[2], _t[3],
                         _pb, _ok(_pb, _t[4], _t[5]), _t[4], _t[5]))

    #print("Enviando:", list(packet))

    # Imprime las 3 líneas LISTAS PARA PEGAR como calibración fija:
    #g = sensor.get_gain_db()
    #rgb = sensor.get_rgb_gain_db()
    #e = sensor.get_exposure_us()
    #print("=========== COPIA ESTAS 3 LINEAS ===========")
    #print("sensor.set_auto_gain(False, gain_db=%f)" % g)
    #print("sensor.set_auto_whitebal(False, rgb_gain_db=(%f, %f, %f))" % (rgb[0], rgb[1], rgb[2]))
    #print("sensor.set_auto_exposure(False, exposure_us=%d)" % e)
    #print("============================================")
