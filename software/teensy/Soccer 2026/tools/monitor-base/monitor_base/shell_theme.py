"""shell_theme.py — Paleta y estilo "industrial" del monitor unificado.

Un solo lugar para los colores y el theming ttk, para que todos los paneles se
vean consistentes (consola de control oscura, sobria, robusta).
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk

# Paleta (consola industrial oscura).
BG = "#0e1116"          # fondo app
BG2 = "#141a22"         # paneles / barras
PANEL = "#0b0e13"       # canvas / áreas de dato
CARD = "#161d27"        # tarjetas
FG = "#cdd6e0"          # texto
MUTED = "#7d8896"       # texto secundario
LINE = "#222b36"        # bordes / divisores
ACCENT = "#39d3a0"      # acento (verde-cian industrial)
ACCENT_DK = "#18634c"
OK = "#2bd27a"
WARN = "#e8b13a"
BAD = "#e0533a"
INFO = "#5aa9e6"
SEL = "#1d2a36"         # item de nav seleccionado

MONO = ("Consolas", 10)
MONO_SM = ("Consolas", 9)
UI = ("Segoe UI", 10)
UI_B = ("Segoe UI", 10, "bold")
TITLE = ("Segoe UI Semibold", 13)


def apply_theme(root: tk.Tk) -> None:
    """Aplica el tema oscuro a los widgets ttk + el fondo del root."""
    root.configure(bg=BG)
    style = ttk.Style(root)
    try:
        style.theme_use("clam")     # el más maleable para re-colorear
    except tk.TclError:
        pass
    style.configure(".", background=BG, foreground=FG, fieldbackground=BG2,
                    bordercolor=LINE, font=UI)
    style.configure("TFrame", background=BG)
    style.configure("Card.TFrame", background=CARD)
    style.configure("Bar.TFrame", background=BG2)
    style.configure("TLabel", background=BG, foreground=FG)
    style.configure("Muted.TLabel", background=BG, foreground=MUTED)
    style.configure("Bar.TLabel", background=BG2, foreground=FG)
    style.configure("Title.TLabel", background=BG2, foreground=FG, font=TITLE)
    style.configure("TButton", background=CARD, foreground=FG, bordercolor=LINE,
                    focuscolor=ACCENT, padding=4)
    style.map("TButton", background=[("active", SEL)], foreground=[("active", ACCENT)])
    style.configure("Nav.TButton", background=BG2, foreground=FG, anchor="w",
                    padding=(12, 8), font=UI)
    style.map("Nav.TButton", background=[("active", SEL)])
    style.configure("NavActive.TButton", background=SEL, foreground=ACCENT,
                    anchor="w", padding=(12, 8), font=UI_B)
    style.configure("TCheckbutton", background=BG, foreground=FG)
    style.map("TCheckbutton", background=[("active", BG)])
    style.configure("TLabelframe", background=BG, foreground=MUTED, bordercolor=LINE)
    style.configure("TLabelframe.Label", background=BG, foreground=MUTED)
    style.configure("TEntry", fieldbackground=PANEL, foreground=FG, bordercolor=LINE)
    style.configure("TMenubutton", background=CARD, foreground=FG)
