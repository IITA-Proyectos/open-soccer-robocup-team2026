# 📋 Cuestionario de datos del equipo — RoboCup Incheon 2026

> **Para:** Virginia (y quien tenga las facturas / specs a mano)
> **Equipo:** IITA Low Battery Messi · RoboCupJunior Soccer **Open** · Incheon 2026
> **Por qué:** estos son los **únicos datos que faltan** para cerrar el póster, el TDP, el video y la entrevista
> que se entregan a los jueces. El firmware, los diagramas y los textos ya están listos — solo falta esta info
> que **solo ustedes tienen** (facturas, nombres, medidas, fotos).
>
> **Cómo completarlo:** escribí la respuesta en la línea `→ ______` o marcá `[x]`. Si algo no lo sabés, dejalo en
> blanco y escribí "no sé" — es mejor un dato vacío que uno inventado. **Prioridad:** 🔴 P0 = bloquea la entrega ·
> 🟡 P1 = sube el puntaje. No hace falta saber de robótica para responder casi todo.

---

## 1. Identidad del equipo  🔴 P0

✅ **Esta sección YA está completa** (nombre, región, roster y mentores confirmados por Gustavo el 2026-06-05 y cargados en los 16 documentos). **No hace falta que la completes.** Para referencia, quedó así:

- **Equipo:** IITA Low Battery Messi · Salta, Argentina.
- **Clasificación:** campeones de la **final nacional de la Roboliga Argentina 2025** (organizada por la UAI), clasificatoria a la RoboCup.
- **Compiten (viajan):** **María Virginia Viollaz** (18 — visión/estrategia; campeona nacional 2022 en Rescue Line + mundial 2023 en Eindhoven) y **Elías Cordero** (18 — electrónica/mecánica).
- **Acompañan (viajan):** **Enzo Juárez Velázquez** (coach) y **Cecilia Budeguer** (mentora).
- **Mentor (no viaja):** **Gustavo Viollaz**.

> ✅ **Confirmado por Gustavo (2026-06-05):** son **campeones inaugurales de la categoría Soccer** (primera edición en Argentina) y el coach es **Enzo Juárez Velázquez** (@enzzo195). Identidad 100% cerrada — nada que confirmar acá.

---

## 2. Costos del robot (BOM)  🔴 P0

> El "tiempo y costo de desarrollo" es un **elemento obligatorio** del póster, y los costos reales suben la nota
> del TDP (de "bien" a "excelente"). **Solo necesitamos el precio de lo que ustedes compraron** — con la factura
> alcanza. Si no tienen el ticket, un precio aproximado de mercado sirve (aclaren que es estimado).
>
> 👉 Hay una **planilla detallada lista para completar** en `docs/competencia/BOM-COSTOS-TEMPLATE.md` (15 filas con
> los componentes ya cargados). Pueden llenar **esa planilla directamente**, o responder acá abajo lo principal:

Precio **por unidad** (poné ARS o USD, el que tengan; aclaren cuál):

| Componente | Cant. | Precio unitario | ¿Nuevo o reusado del robot 2025? |
|---|---|---|---|
| Cámara OpenMV N6 | 2 | __________ | [ ] nuevo / [ ] reusado |
| Sensor de odometría SparkFun OTOS | 2 | __________ | [ ] nuevo / [ ] reusado |
| Teensy 4.0 (placas TOP y DOWN) | 2 | __________ | [ ] nuevo / [ ] reusado |
| Teensy 4.1 (placa CENTRAL) | 1 | __________ | [ ] nuevo / [ ] reusado |
| Placa Zircon (Robomov) | 1 | __________ | [ ] nuevo / [ ] reusado |
| Sensor ToF VL53L7CX | 4 | __________ | [ ] nuevo / [ ] reusado |
| Batería LiPo | 1–2 | __________ | [ ] nuevo / [ ] reusado |
| Motores | 3 | __________ | [ ] nuevo / [ ] reusado |
| Ruedas omni | 3 | __________ | [ ] nuevo / [ ] reusado |
| IMU BNO055 | 1–2 | __________ | [ ] nuevo / [ ] reusado |
| (Otros que recuerden) | __ | __________ | [ ] nuevo / [ ] reusado |

**2.1 — Costo total aproximado por robot:** → __________ (ARS o USD: ______)
**2.2 — Costo total de los 2 robots:** → __________
**2.3 — Tipo de cambio que usamos** (si mezclan ARS y USD): → 1 USD = ______ ARS
**2.4 — Tiempo de desarrollo:** ¿cuántas **horas/semanas** aprox. le dedicaron al robot 2026?
→ ______________ (ej.: "≈4 meses, ~3 personas, fines de semana")

---

## 3. Especificaciones mecánicas  🔴 P0 (motor/rueda/robot) · 🟡 P1 (batería)

> Esto permite que **otro equipo pueda replicar el robot** — es el "estándar de oro" que premia RoboCup en el TDP.
> Si lo saben de memoria, genial; si no, una medida con regla/balanza alcanza.

**3.1 — Motores** (modelo o link de compra si lo tienen):
→ Modelo / tipo: ______________________  ·  Voltaje: ______  ·  ¿Tienen encoder? [ ] sí / [ ] no

**3.2 — Ruedas omnidireccionales:**
→ Diámetro: ______ mm  ·  Material: ____________  ·  [ ] impresas en 3D / [ ] compradas

**3.3 — El robot armado:**
→ Diámetro: ______ mm (¿entra en el círculo reglamentario?)  ·  Peso: ______ g  ·  Altura: ______ mm

**3.4 — Batería** 🟡 P1:
→ Capacidad: ______ mAh  ·  Celdas: [ ] 2S (7.4 V) / [ ] otro: ____  ·  Marca: __________  ·  ¿Cuánto dura un partido con una carga? ______

---

## 4. Fotos a sacar  🔴 P0

> **El repo no tiene ni una foto del robot** — y sin fotos el póster pierde casi medio puntaje de esa sección.
> No hace falta nada profesional: **celular + buena luz + fondo liso (pared blanca o cartón claro)**. Manden los
> archivos y nosotros los ubicamos. Lista de las que necesitamos:

- [ ] **Robot de frente/3-4** (entero, sobre fondo liso) — *la foto principal*
- [ ] **Robot de costado** mostrando las **3 placas apiladas** (los "pisos")
- [ ] **Foto de cerca del anillo de sensores de línea** (la placa de abajo)
- [ ] **El equipo** (idealmente con el trofeo del Nacional 2025 si lo tienen)
- [ ] **La notebook con los tests en verde** (pantalla que diga `624 tests / 0 failures`) — *opcional, la sacamos nosotros*
- [ ] **(Opcional)** el robot jugando / en la cancha, para el video

→ ¿Quién las saca y para qué fecha? ______________________

---

## 5. CAD, video y cierre  🟡 P1

**5.1 — Archivos CAD/STL del chasis 2026.** ¿Existen? (diseño del armazón en Fusion/SolidWorks/Tinkercad, etc.)
- [ ] Sí, los podemos exportar (STL o STEP) → quién: ____________
- [ ] No hay CAD del chasis 2026
- [ ] Hay pero de un diseño viejo

**5.2 — Video (<3 min).** El guion ya está escrito (`VIDEO-GUION.md`). Falta **grabarlo** con subtítulos en inglés.
→ ¿Quién lo graba y para qué fecha aproximada? ______________________

**5.3 — ¿Algo más que el equipo quiera destacar?** (un logro, una anécdota técnica, un sponsor a agradecer)
→ ____________________________________________

---

## ✅ Cuando esté completo

Devuelvan este archivo (o las respuestas sueltas, como les quede cómodo) y yo:
1. Cargo los **costos** en el BOM, el póster y el TDP (totales + tiempo de desarrollo).
2. Cargo las **specs mecánicas** (motor/rueda/robot/batería) en el TDP/BOM.
3. Ubico las **fotos** en el póster/TDP con sus epígrafes.
4. Genero el **QR del repo** para el póster y el one-pager.

> **Nada de esto necesita que sepan programar** — es la info que solo ustedes tienen. ¡Gracias Virginia! 🤖⚽
