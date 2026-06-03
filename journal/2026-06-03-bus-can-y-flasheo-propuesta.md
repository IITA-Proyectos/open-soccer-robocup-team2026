---
title: "Propuesta de bus general CAN + flasheo de firmware por CAN"
date: 2026-06-03
author: "Claude (Anthropic - Claude Opus 4.8 1M)"
requested-by: "Gustavo Viollaz (@gviollaz)"
ai-assisted: true
ai-tool: "Claude Code (Opus 4.8 1M, Anthropic)"
status: final
tags: [journal, comunicacion, can, flasheo, firmware, telemetria, propuesta]
---

# Journal — bus CAN general + flasheo por CAN (propuesta)

## Qué se hizo

A pedido de Gustavo, estudio profundo de **cómo comunicar las 3 placas** (abajo,
central, arriba) con un **bus general**, sumando **4 cámaras OpenMV**, **LiDAR
opcional** y **telemetría RF a una PC**, priorizando **simple, confiable y
potente**. Luego, esquema de **flasheo de firmware por el mismo bus**.

Resultado documentado en
[`docs/decisions/2026-06-03-bus-can-general-y-flasheo-por-can.md`](../docs/decisions/2026-06-03-bus-can-general-y-flasheo-por-can.md)
(**status: propuesta, post-Incheon, NO validada en banco**).

## Conclusiones principales

1. **Bus = CAN 2.0B clásico @ 1 Mbps** (FlexCAN nativo de las Teensy + transceiver
   SN65HVD230). Gana a RS-485 (CAN trae arbitraje/CRC/reintento por HW; RS-485 lo
   programás vos), a UART (no escala en pines: ya casi sin serials libres), a
   SPI/I²C (no son multidrop reales/robustos), a 10BASE-T1S (potente pero
   ecosistema inmaduro en Teensy, stack TCP/IP de más) y a Ethernet/EtherCAT
   (voluminoso para un robot de cilindro ~22 cm).
2. **Las OpenMV hablan CAN nativo (≤1 Mbps)** ⇒ cada cámara es un nodo más que
   publica **coordenadas, no frames**. La imagen va por USB/WiFi (debug).
3. **LiDAR** en UART dedicado; al bus solo el obstáculo derivado.
4. **RF = herramienta de banco**, NO de partido: el reglamento RCJ 2026 (1.3.1 /
   3.2) permite solo robot↔robot 2.4 GHz ≤100 mW y prohíbe remote control. ⇒
   bridge ESP32-S3 desconectable; el robot debe ser autónomo con RF apagada.
   5 GHz solo sirve en el banco (en partido es ilegal).
5. **Flasheo por CAN** vía **FlasherX**: la app Teensy se auto-reprograma desde el
   bus. Protocolo propio con gate de mantenimiento, transferencia por bloques con
   `BLOCK_ACK/NAK`, e **integridad en 3 capas** (CRC frame HW + CRC16 bloque +
   CRC32 imagen). **USB siempre como recovery**; A/B dual-bank queda para después.
   El ESP32-S3 puede actuar de **hub de programación** (laptop→WiFi→bridge→CAN→
   Teensy) ⇒ flasheo inalámbrico de todo el robot.

## Lo que NO cambia

El transporte que **CORRE** sigue siendo UART (`proto.h`). CAN es **aditivo** y
preserva las **capas fail-safe 0–6** del diseño vivo (2026-05-18). Migración por
fases F0–F6, cada una reversible y con paridad contra UART antes de cortar nada.

## Verificaciones externas (jun-2026)

- Teensy 4.x: 3 controladores CAN, **CAN3 = CAN-FD** (5 Mbps con osc 24 MHz, 8
  Mbps a 80 MHz); transceiver externo obligatorio.
- OpenMV (H7/RT1062/N6): Arduino Interface Library expone **CAN/UART/SPI/I²C**
  (CAN ≤1 Mbps, SPI ≤60 Mb/s).
- **FlasherX** (joepasquariello): auto-flasheo de Teensy LC/3.x/4.x desde
  cualquier stream (USB/UART/SD/…); CAN se puentea con glue. Hay fork Ethernet.
- **STM32** (si algún día se rediseña): bootloader ROM con flasheo por **CAN/UART/
  USB/I²C/SPI nativo** (pin BOOT0) — ventaja real frente a Teensy en este punto.
- RoboCup Jr Soccer Rules 2026: reglas 1.3.1 (robot↔robot 2.4 GHz ≤100 mW) y 3.2
  (no remote control, autónomo).

## Próximos pasos sugeridos

- F0: sketch PoC CAN entre 2 Teensy con ruido de motores (no bloqueante).
- Si se aprueba, abrir TASKs de migración F1–F5 en `team-tasks/`.

## Addendum (mismo día) — extensión inter-robot del gateway

A pedido de Gustavo, análisis de si los **2 robots** pueden compartir datos por
el **mismo gateway** del bus CAN. Documentado en el doc del bus **§7-bis**.
Conclusiones:

- **CAN no cruza entre robots** (cableado) ⇒ inter-robot es wireless sí o sí; el
  gateway es un **puente selectivo CAN↔aire**, no un tunel transparente. Se
  publica un `TeamShare` curado (pelota, pose, rol, intención) y se **inyecta en
  el CAN del otro robot** como `0x230` ⇒ el compañero aparece como un CAN frame.
- Los **3 roles** del gateway (inter-robot / telemetría / flasheo) son
  **temporalmente exclusivos por reglamento** (solo inter-robot es de partido) ⇒
  **un mismo ESP32-S3 puede hacer los tres** con un **gate de modo** manejado por
  el `match_running` del árbitro (telemetría/flasheo hard-OFF en partido).
- **NO** usar la placa COMM (C6) para esto: es el módulo árbitro obligatorio
  (= Opción C ya rechazada). El gateway es un **ESP32-S3 separado**.
- **No cambia** la decisión `2026-05-17` (Opción A: inter-robot diferido); §7-bis
  es la arquitectura concreta de la **Opción B** para post-Incheon.
