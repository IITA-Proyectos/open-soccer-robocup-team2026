import sensor
import time

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)

# 1) Dejar que el auto encuentre un punto de partida razonable
sensor.set_auto_whitebal(True)
sensor.set_auto_gain(True)
sensor.skip_frames(time=2000)

# 2) CONGELAR exposicion y WB. Si no, el AEC te deshace la ganancia.
exp = sensor.get_exposure_us()
rgb = sensor.get_rgb_gain_db()
sensor.set_auto_exposure(False, exposure_us=exp)
sensor.set_auto_whitebal(False, rgb_gain_db=rgb)
sensor.set_auto_gain(False)
sensor.skip_frames(time=500)

print("== punto de partida ==")
print("gain = %.3f dB   exp = %d us" % (sensor.get_gain_db(), sensor.get_exposure_us()))

# 3) Barrido en dB ABSOLUTOS (no multiplicativo)
GANANCIAS = [0.0, 6.0, 12.0, 18.0, 24.0, 30.0, 36.0]

clock = time.clock()
i = 0
t0 = time.ticks_ms()

while True:
    clock.tick()
    img = sensor.snapshot()

    if time.ticks_diff(time.ticks_ms(), t0) > 2000:
        t0 = time.ticks_ms()
        pedido = GANANCIAS[i % len(GANANCIAS)]
        sensor.set_auto_gain(False, gain_db=pedido)
        sensor.skip_frames(time=200)
        real = sensor.get_gain_db()
        print("pedido %6.2f dB -> real %6.2f dB   %s" % (
            pedido, real,
            "OK" if abs(real - pedido) < 1.0 else "<<< CLAMPEADO o IGNORADO"))
        i += 1
