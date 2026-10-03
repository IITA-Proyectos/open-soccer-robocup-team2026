# TASK-124 — Banco: arqueromix #10 (empuje con la cámara trasera) + TOP con prioridad delantera

- **Placa:** CENTRAL + TOP (R2, arquero)
- **Asignado:** equipo (banco)
- **Prioridad:** P1 — conducta nueva sin probar; si no se entiende ahora, vuelve en el Nacional de noviembre.
- **Estado:** abierta
- **Build (se flashean JUNTOS):**
  - TOP: `pio run -e top_robot2_pri_frontprio -t upload`
  - CENTRAL: `pio run -e central_robot2_arqueromix_empuje -t upload`
  - Escape: TOP `top_robot2_pri` + CENTRAL `central_robot2_arqueromix_recto_cortaretro` (#9).
- **Journal:** `journal/2026-10-03-arqueromix-empuje-trasera-prioridad-delantera.md`

## Qué cambia

1. **TOP:** si las DOS cámaras ven "pelota", manda la DELANTERA (antes promediaba → pelota fantasma en el medio).
2. **CENTRAL (arquero):** pelota ATRÁS (|ángulo| > 90° = la vio la trasera) + el TOP NO ve el arco PROPIO →
   retrocede empujándola con la cola HASTA LA LÍNEA → sale como el homing (avanza, se acomoda, espera).
   Corta si aparece el arco propio, si la pelota pasa al frente, si DOWN no está fresco o a los 3 s.

## Cómo validar (en orden)

0. **`pio run` de los dos envs** en la PC del taller (en la sesión cloud no se pudo: registry bloqueado).
   Anotar FLASH/RAM. Y que `central_robot2_arqueromix_recto_cortaretro` y `top_robot2_pri` den el MISMO
   md5 de `.hex` que antes de este commit (deberían: sin los flags el preprocesado es idéntico).
1. **Prioridad delantera (TOP, monitor-base vista Cámara):** pelota real adelante + algo naranja atrás
   (buzo/tapa). Antes: la "pelota" fusionada caía en el medio. Ahora: tiene que quedar ADELANTE, en la
   posición de la delantera. Sacar la pelota de adelante → la fusionada pasa a la de atrás.
2. **¿La trasera ve el arco propio desde el arco?** Arquero en su lugar: mirar en el monitor si
   `goal_own` aparece visible. **Si el arco propio se ve SIEMPRE desde la posición del arquero, el
   empuje NUNCA se dispara cerca del arco** (por diseño: no empuja hacia su arco). Anotar el resultado:
   decide si la condición "sin arco" tiene que ser otra (p.ej. sólo si la pelota NO está entre el
   robot y el arco).
3. **Empuje:** sin arco propio a la vista, pelota quieta ~20 cm detrás del robot → al GO debe
   retroceder empujándola, PARAR en la línea y avanzar para despegarse. Medir: ¿se pasa de la línea?
   ¿pierde la pelota de costado? El empuje va a la MISMA potencia que el despeje (rampa → 191): mirar si por
   la inercia **se pasa de la línea** al frenar (si pasa → agregar freno activo, como `frenar_patada`) y si
   se desvía de costado (el trim del pateo NO se aplica hacia atrás).
4. **Cortes:** (a) poner la pelota adelante durante el empuje → tiene que cortar y seguirla;
   (b) levantar el robot (DOWN "lifted") → no debe seguir retrocediendo a ciegas.
5. **Loop:** con la pelota quieta en la línea detrás, ver si "bombea" (empuja, avanza, empuja…).
   Perilla `AMIX_T_EMPUJE_COOLDOWN` (2000 ms) en `amix_config.h`.

## Criterio de cierre

Pasos 0-5 hechos y anotados en el journal, con la decisión de qué hacer con el paso 2. Lo cierra
quien tenga el robot en la mano (no Claude).
