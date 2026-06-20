*R1 — BNO (heading) se cuelga: causa encontrada* 🛠️

Enzo, diagnóstico de banco de hoy (R1, placa TOP). Resumen:

*El problema:* el heading del BNO de R1 se congela ("dice OK pero no cambia al girar").

*NO es:* (todo probado en banco)
❌ el chip BNO (anda perfecto si los ToF no rangean — NO lo cambien)
❌ el cristal / oscilador
❌ la frecuencia de lectura (20 y 100 Hz, igual)
❌ el bus I2C compartido (escaneé los 2 buses: están AISLADOS, los ToF solo aparecen en Wire, el BNO primario solo en Wire2)
❌ la batería (con batería independiente a la TOP, sigue igual)
❌ un solo ToF con cable malo (probé de a uno: el #0 Y el #1, cada uno solo, lo cuelgan)

*SÍ es:* 🎯
✅ *El RANGEO de cualquier ToF cuelga al BNO.* Encenderlos (LP) no pasa nada; cuando RANGEAN (pulsos del láser VCSEL) se cuelga la fusión del BNO. Basta UN ToF rangeando.
➡️ Es *acople ELÉCTRICO en la placa* (masa / 3V3 / EMI compartidos entre los ToF y el BNO), NO el bus ni la batería.

*Qué revisar (conexiones):*
1. La *masa (GND)* del BNO vs los ToF: que la corriente pulsada del láser NO pase por el retorno de masa del BNO. Ideal: masa de los ToF separada de la del BNO (star-ground).
2. El *3V3 del BNO*: ¿comparte rail con los ToF? Poner *desacoplo fuerte pegado al BNO* (100nF + 10µF), o darle un LDO/ferrite propio aislado del rail de los ToF.
3. Cables de los ToF que corran pegados a las líneas del BNO (EMI).

*Bonus:* tu repaso de soldaduras arregló el 2º BNO (volvió a responder). 👍

*Cómo verificar el fix:* girar el robot con el firmware de competencia y mirar si el heading sigue el giro (si queda en 0.0 girando = sigue el bug; si se mueve = arreglado).

*Mitigación para Incheon si no llega el HW:* usar el heading del OTOS (placa DOWN) en vez del BNO, que no sufre esto.

Detalle completo + cómo reproducir: en el repo, journal 2026-06-20 + TASK-223.
