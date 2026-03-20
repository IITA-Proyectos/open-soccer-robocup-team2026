---
title: "Flujo de Trabajo para Desarrollo de Software"
date: 2026-03-20
author: "Claude (Anthropic) bajo supervisión de Gustavo Viollaz"
ai-assisted: true
status: final
tags: [workflow, desarrollo, testing, organizacion]
---

# Flujo de Trabajo para Desarrollo de Software

## Principio fundamental

> **Nunca se modifica el código que funciona en los robots hasta que el código nuevo esté probado físicamente.**

El código que actualmente corre en los robots está en `software/robot-delantero/` y `software/robot-arquero/`. Ese código **no se toca** hasta que una mejora haya sido probada en el robot real y verificada por el equipo.

---

## Estructura de carpetas

```
software/
├── robot-delantero/           # 🟢 PRODUCCIÓN - lo que está en el robot HOY
│   └── definitivo-delantero     #    NO TOCAR (hasta promoción)
├── robot-arquero/             # 🟢 PRODUCCIÓN - lo que está en el robot HOY
│   └── definitivo-arquero_6-9-2026
├── staging/                   # 🟡 PENDIENTE DE PRUEBA - probar el próximo viernes
│   ├── README.md               #    Lista de QUÉ probar y CÓMO
│   ├── delantero/              #    Versiones nuevas del delantero a probar
│   ├── arquero/                #    Versiones nuevas del arquero a probar
│   ├── shared/                 #    Módulos compartidos nuevos
│   └── vision/                 #    Cambios en OpenMV a probar
├── shared/                    # 🟢 PRODUCCIÓN - módulos compartidos probados
├── vision/                    # 🟢 PRODUCCIÓN - código OpenMV actual
├── libraries/                 # Librerías (zirconLib, etc.)
└── communication/             # Documentación de protocolos
```

### Colores
- 🟢 **PRODUCCIÓN** (`robot-delantero/`, `robot-arquero/`, `shared/`, `vision/`): Lo que está en los robots. Solo se modifica después de prueba exitosa.
- 🟡 **STAGING** (`staging/`): Código nuevo listo para probar en el robot. Es la "bandeja de entrada" del próximo viernes.

---

## Ciclo de trabajo semanal

### Durante la semana (lunes a jueves)

1. **Claude + Gustavo** preparan mejoras y las ponen en `staging/`
2. Cada cambio en staging tiene:
   - El archivo `.ino` o `.py` listo para compilar/subir
   - Un entry en `staging/README.md` explicando qué cambio es, por qué se hizo, y cómo probarlo
3. Se pueden preparar múltiples cosas para probar (fixes de bugs, mejoras de desempeño, etc.)

### El viernes (sesión con robots)

1. **Abrir `staging/README.md`** — acá está la lista de todo lo que hay que probar
2. **Para cada item**:
   - Descargar el archivo de `staging/` al robot
   - Compilar y subir
   - Ejecutar el protocolo de prueba descrito
   - **Anotar resultado** en el README (funciona / no funciona / parcial + observaciones)
3. **Al final del viernes**, lo que funcionó se "promueve":
   - Se copia de `staging/` a la carpeta de producción correspondiente
   - Se actualiza la documentación
   - Se limpia staging

### Promoción: de staging a producción

Cuando un cambio pasa la prueba en el robot:

```
staging/delantero/delantero-v2-uart-fix.ino
    │
    │  PRUEBA EXITOSA ✅
    │
    ▼
software/robot-delantero/delantero-v2-uart-fix.ino  (nueva versión de producción)
```

El archivo anterior de producción no se borra — queda en el historial de Git. Si algo falla, siempre se puede volver atrás.

---

## Cómo nombrar archivos en staging

Usar nombres descriptivos que indiquen qué cambió:

```
staging/delantero/
├── fix-uart-sincronizacion.ino        # Fix específico del bug UART
├── fix-rampa-pateo-reset.ino          # Fix del bug de velocidadActualPateo
├── mejora-avanzar-con-giroscopo.ino   # Mejora: avanzar recto usando BNO055
└── refactor-modular-v1.ino            # Refactoring completo (más riesgoso)

 staging/arquero/
├── fix-currentyaw-a-error.ino         # Fix del bug de currentYaw raw
├── fix-gap-yp-3-5.ino                 # Fix del gap que congela al robot
└── fix-s3-oscilacion.ino              # Fix: agregar chequeo s3
```

---

## Formato del README de staging

El archivo `staging/README.md` es la **hoja de ruta del viernes**. Formato:

```markdown
# Staging — Pendiente de prueba

Última actualización: YYYY-MM-DD

## Prioridad 1 (probar primero)

### [DELANTERO] Fix sincronización UART
- **Archivo**: `delantero/fix-uart-sincronizacion.ino`
- **Bug que resuelve**: La comunicación con OpenMV se desincroniza y pierde la pelota
- **Qué cambió**: Se agregó descarte de bytes basura con peek() antes de leer
- **Cómo probar**:
  1. Subir al robot delantero
  2. Encender con la pelota visible
  3. Verificar que el LED indica detección estable (no parpadea)
  4. Mover la pelota y verificar que la sigue sin perderla
  5. Tapar la cámara 3 segundos y destapar — debe re-detectar inmediato
- **Resultado**: ⬜ Pendiente | ✅ Funciona | ❌ No funciona | 🟡 Parcial
- **Observaciones**: _(completar después de probar)_

## Prioridad 2
...
```

---

## Reglas importantes

1. **Nunca modificar directamente los archivos en `robot-delantero/` o `robot-arquero/`** sin prueba previa en staging.
2. **Un cambio por archivo en staging**. No mezclar múltiples fixes en un solo archivo — si uno falla, no sabés cuál.
3. **Si un cambio es riesgoso** (refactoring grande, cambio de arquitectura), marcarlo claramente y probarlo último.
4. **Si algo falla en el robot**, anotar exactamente qué pasó en las observaciones. Esa info vale oro para el siguiente intento.
5. **Antes de ir al viernes, hacer Pull** del repo para tener la última versión de staging.
6. **Después del viernes, hacer ACP** (Add, Commit, Push) con los resultados anotados.

---

## Estrategia de prueba: de menor a mayor riesgo

Orden recomendado para probar en el viernes:

1. **Fixes triviales** (una línea cambiada, bajo riesgo): timeout faltante, s3 faltante, gap Yp
2. **Fixes de comunicación** (medio riesgo): sincronización UART, validación de headers
3. **Mejoras de desempeño** (medio riesgo): avanzar con giróscopo, velocidad adaptativa
4. **Refactoring modular** (alto riesgo): código reorganizado — mismo comportamiento, distinta estructura

Si un fix de prioridad alta falla, **no seguir probando cosas que dependen de ese fix**.

---

## Historial de sesiones de prueba

Después de cada viernes, mover los resultados a `testing/results/`:

```
testing/results/
└── 2026-03-28-sesion-pruebas.md    # Resultados del viernes 28/3
```

Formato:
```markdown
# Sesión de pruebas — 2026-03-28

## Probado
| Item | Robot | Resultado | Observaciones |
|------|-------|-----------|---------------|
| Fix UART sync | Delantero | ✅ | Detección estable, no parpadea |
| Fix gap Yp | Arquero | ❌ | Sigue congelando, revisar umbral |

## Promovido a producción
- Fix UART sync → robot-delantero/delantero.ino

## Pendiente para próxima sesión
- Revisar fix gap Yp con umbral 4 en vez de 5
```
