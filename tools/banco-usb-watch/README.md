# banco-usb-watch

Vigía de banco para el **USB flojo de la CENTRAL** (conector que cortó >5 veces
el 2026-06-12 y bloqueó la verificación post-flasheo).

`watch_panel.py` espera a que el puerto COM **reaparezca** y después lee el panel
serial de la CENTRAL buscando un marcador (`snap_fresh=Y` por defecto) + el
estado de la FSM. **Solo lee; no manda nada al robot.**

```
python tools/banco-usb-watch/watch_panel.py
```

- exit 0 → snapshot fresco; imprime el panel (`state=…`, `match=…`, `hdg=…`).
- exit 1 → el puerto nunca volvió (USB sigue muerto → revisar conector/cable).
- exit 2 → puerto OK pero sin snapshot (TOP no levantó / sin batería).

**Para qué sirve:** tras un `pio run -e <env> -t upload`, confirmar el marcador
único del binario (p.ej. `state=GK_SIMPLE_WAIT` de la v6 del arquero strafe) sin
creerle al SUCCESS de pio (regla: el SUCCESS de upload NO garantiza que el
binario llegó). Cambiar `MARKER` en el script para cazar otro estado.

> Pendiente de hardware relacionado: el conector USB de la CENTRAL (y el de la
> DOWN) quedaron flagueados como flojos — ver journal 2026-06-12.
