---
id: TASK-025
title: "Avast (SSL-scanning) bloquea PlatformIO — configurar excepción en cada máquina"
date_created: 2026-05-18
assigned: [gviollaz, mariaviollaz, elias]
priority: P1
status: pending
estimated_hours: 0.5
blocks: [correr suite de tests host-native, instalar libs/plataformas PlatformIO, TASK-023]
tags: [infra, entorno, platformio, testing, antivirus, ssl]
---

# TASK-025 — Avast bloquea PlatformIO (SSL MITM)

## Resumen

PlatformIO no puede bajar plataformas ni librerías del registry (`pio test`,
`pio pkg install` fallan con `HTTPClientError`). La causa **NO es falta de
red**: es **Avast Antivirus interceptando TLS** (SSL-scanning / MITM). Cada
máquina del equipo que tenga Avast necesita una excepción configurada — si no,
nadie puede instalar libs nuevas (p.ej. al activar OTOS/ToF de TASK-023) ni
correr el TDD sin depender de pre-cachear todo offline.

Complementa `docs/firmware/SETUP-ENTORNO-BUILD-WINDOWS.md` (que da el
workaround de cache offline pero no diagnosticaba la causa raíz).

## Diagnóstico (confirmado 2026-05-18)

- `git push` y `WebFetch` funcionan → hay red.
- `curl https://api.registry.platformio.org` → error 35
  (`CRYPT_E_NO_REVOCATION_CHECK`, schannel).
- `curl --ssl-no-revoke ...` → **HTTP 200**. Confirma que el problema es la
  verificación de revocación del certificado, no la conectividad.
- Variable de entorno presente: `SSLKEYLOGFILE=\\.\aswMonFltProxy\...` →
  `aswMonFlt` = driver de Avast. Avast reemplaza los certificados HTTPS por
  los suyos; esos certs no tienen endpoint de revocación comprobable →
  Windows schannel corta el handshake.
- PlatformIO 6.1.x usa `truststore` (almacén de certificados de Windows =
  schannel) → hereda el problema. `git` usa OpenSSL → no lo tiene (por eso
  los push de toda la sesión funcionaron).

## Pasos concretos (cada máquina con Avast)

### Opción A — Excepción de URLs (recomendada, no baja protección global)

1. Avast → ☰ Menú → Configuración → General → **Excepciones**.
2. Agregar excepción con estas URLs:
   - `*platformio.org*`
   - `https://dl.platformio.org/*`
   - `https://api.registry.platformio.org/*`
   - `https://collector.platformio.org/*`
3. Guardar. Reiniciar la terminal.

### Opción B — Desactivar HTTPS scanning (más amplio, menos protección)

1. Avast → ☰ Menú → Configuración → Protección → Protecciones básicas.
2. Pestaña "Protección web" (Web Shield).
3. Destildar "Habilitar análisis HTTPS".

### Opción C — Solo si A/B no son posibles (decisión del coach)

`pio settings set enable_proxy_strict_ssl No` — debilita la verificación TLS
de PlatformIO en esa máquina. Aceptable solo en red de lab confiable. NO es
la preferida porque afecta todas las descargas de PlatformIO. Requiere
autorización explícita (debilita seguridad).

## Criterio de cierre

- [ ] En la máquina de Gustavo: excepción aplicada, `pio test -e test_native`
      corre las suites sin `HTTPClientError`.
- [ ] Documentado en `journal/` el resultado de correr la suite host-native.
- [ ] Virginia y Elías replican la excepción en sus máquinas (anotar acá
      cuándo cada uno lo hizo).
- [x] Nota agregada a `docs/firmware/SETUP-ENTORNO-BUILD-WINDOWS.md` (callout
      de causa raíz + troubleshooting corregido) para que el relevo 2027 no
      pierda horas con esto.

## Notas / decisiones

- Unity fue vendoreado por git en `software/teensy/Soccer 2026/lib/Unity`
  (clon tag v2.6.0) como mitigación parcial, pero PlatformIO con
  `test_framework = unity` igual resuelve del registry → no alcanza por sí
  solo. Con la excepción de Avast deja de importar. Evaluar si conviene
  mantener o quitar el vendoring una vez resuelto el SSL (NO commiteado al
  repo por ahora — queda local en la máquina de Gustavo).
- El doc del equipo `SETUP-ENTORNO-BUILD-WINDOWS.md` (TASK-023) sigue siendo
  válido y útil para cachear; esta TASK le agrega la causa raíz + un fix que
  permite a PlatformIO funcionar CON red (no solo offline-cacheado).

## Cambios de estado

- 2026-05-18: creada (renumerada desde un TASK-013 local que colisionó con el
  TASK-013 del equipo al reconciliar la divergencia git). Diagnóstico hecho
  durante la sesión de análisis de firmware. Causa raíz: Avast MITM SSL.
