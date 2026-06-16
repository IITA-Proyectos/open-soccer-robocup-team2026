"""tooltip.py — Ayuda contextual: un globo que aparece al posar el cursor sobre un
control unos instantes (sin clickear) y explica PARA QUÉ SIRVE.

Pensado para usuarios nuevos que no saben qué hace "Auto ON", "CAL CARPET",
"OTOS RESET", "CFG RESET", etc. Pasás el mouse por encima, esperás ~0,7 s sin
clickear, y aparece la explicación. Se va al sacar el cursor o al clickear.

Uso:
    from .tooltip import attach_tooltip
    btn = ttk.Button(parent, text="Auto ON", command=...)
    attach_tooltip(btn, "Arranca la auto-calibración: el robot aprende solo el "
                        "verde/blanco mientras lo movés sobre la línea.")

Sin dependencias raras (solo tkinter). La lógica de mostrar/ocultar es chica y
host-testeable (construir + _show/_hide sin loop).
"""
from __future__ import annotations

import tkinter as tk
from typing import Optional

HOVER_MS = 700        # cuánto hay que quedarse quieto sobre el control para que salga
_BG = "#1b2430"
_FG = "#dfe7ef"
_BORDER = "#3a4a5a"


class Tooltip:
    """Globo de ayuda para UN widget. Se arma con attach_tooltip()."""

    def __init__(self, widget: tk.Widget, text: str, delay: int = HOVER_MS):
        self.widget = widget
        self.text = text
        self.delay = delay
        self._after: Optional[str] = None
        self._tip: Optional[tk.Toplevel] = None
        widget.bind("<Enter>", self._schedule, add="+")
        widget.bind("<Leave>", self._hide, add="+")
        widget.bind("<ButtonPress>", self._hide, add="+")   # al accionar, ocultar

    # ── interno ──────────────────────────────────────────────────────────────
    def _schedule(self, _evt=None) -> None:
        self._cancel()
        try:
            self._after = self.widget.after(self.delay, self._show)
        except Exception:  # noqa: BLE001 — widget ya destruido
            self._after = None

    def _show(self) -> None:
        self._after = None
        if self._tip is not None or not self.text:
            return
        try:
            x = self.widget.winfo_rootx() + 14
            y = self.widget.winfo_rooty() + self.widget.winfo_height() + 6
        except Exception:  # noqa: BLE001
            return
        self._tip = tw = tk.Toplevel(self.widget)
        tw.wm_overrideredirect(True)          # sin barra de título
        try:
            tw.wm_attributes("-topmost", True)
        except Exception:  # noqa: BLE001 — algunas plataformas no lo soportan
            pass
        tw.wm_geometry(f"+{int(x)}+{int(y)}")
        tk.Label(tw, text=self.text, justify="left", bg=_BG, fg=_FG,
                 relief="solid", borderwidth=1, padx=8, pady=5,
                 font=("Segoe UI", 9), wraplength=320,
                 highlightbackground=_BORDER, highlightthickness=1).pack()

    def _hide(self, _evt=None) -> None:
        self._cancel()
        if self._tip is not None:
            try:
                self._tip.destroy()
            except Exception:  # noqa: BLE001
                pass
            self._tip = None

    def _cancel(self) -> None:
        if self._after is not None:
            try:
                self.widget.after_cancel(self._after)
            except Exception:  # noqa: BLE001
                pass
            self._after = None


def attach_tooltip(widget: tk.Widget, text: str, delay: int = HOVER_MS) -> Optional[Tooltip]:
    """Engancha un tooltip a `widget`. Si text está vacío, no hace nada (devuelve
    None) — así se puede llamar siempre sin if en el sitio de llamada."""
    text = (text or "").strip()
    if not text:
        return None
    return Tooltip(widget, text, delay)
