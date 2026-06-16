"""Tests del tooltip de ayuda contextual (sin mainloop de Tk)."""
import tkinter as tk

from monitor_base.tooltip import Tooltip, attach_tooltip
from monitor_base.tooltips_text import TOOLTIPS, tip


def test_tip_known_and_unknown():
    assert tip("CAL CARPET")               # comando conocido → texto
    assert tip("base.sens_global")         # clave de vista → texto
    assert tip("noexiste") == ""           # desconocido → "" (no rompe el call site)


def test_tooltips_cover_cryptic_controls():
    # Los controles que un usuario nuevo NO entiende (el pedido de Gustavo).
    for k in ("CAL AUTO ON", "CAL AUTO OFF", "CAL LOAD", "OTOS RESET", "CFG RESET",
              "TOF POS", "tofset.rot", "tofset.flip", "tofset.push", "logs.autoscroll",
              "BNO L ON", "US ON", "field.otos"):
        assert TOOLTIPS.get(k), f"falta tooltip para {k}"


def test_tooltips_are_short():
    # Tooltips cortos para que entren en el globo sin tapar la pantalla.
    long = {k: len(v) for k, v in TOOLTIPS.items() if len(v) > 160}
    assert not long, f"tooltips demasiado largos: {long}"


def _root():
    r = tk.Tk(); r.withdraw(); return r


def test_attach_returns_tooltip_for_text():
    r = _root()
    b = tk.Button(r, text="Auto ON")
    tp = attach_tooltip(b, "arranca la auto-calibración")
    assert isinstance(tp, Tooltip)
    r.destroy()


def test_attach_empty_text_is_noop():
    r = _root()
    b = tk.Button(r, text="x")
    assert attach_tooltip(b, "") is None
    assert attach_tooltip(b, "   ") is None      # solo espacios = sin tooltip
    r.destroy()


def test_show_and_hide_no_crash():
    r = _root()
    b = tk.Button(r, text="CAL CARPET"); b.pack(); r.update_idletasks()
    tp = attach_tooltip(b, "captura el color del piso verde")
    tp._show()        # root oculto: puede o no crear el globo, pero NO debe romper
    tp._hide()        # tras ocultar, siempre limpio
    assert tp._tip is None
    r.destroy()
