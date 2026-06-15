"""panel_help.py — Panel de AYUDA: muestra la guía completa dentro de la app.

board="any": no depende de ninguna placa; render(f) no hace nada (contenido
estático). El mismo texto que `--readme` (help_text.GUIDE).
"""
from __future__ import annotations

import tkinter as tk
from tkinter import ttk

from .help_text import GUIDE
from .panel import Panel
from .shell_theme import ACCENT, BG, FG, MONO_SM, PANEL


class HelpPanel(Panel):
    title = "Ayuda"
    key = "ayuda"
    icon = "?"
    board = "any"

    def build(self, parent: tk.Frame) -> None:
        tk.Label(parent, text="GUÍA DE LA APLICACIÓN", bg=BG, fg=ACCENT,
                 font=("Segoe UI Semibold", 13), anchor="w", padx=10, pady=8).pack(fill="x")
        wrap = tk.Frame(parent, bg=BG)
        wrap.pack(fill="both", expand=True, padx=10, pady=(0, 10))
        wrap.rowconfigure(0, weight=1); wrap.columnconfigure(0, weight=1)
        txt = tk.Text(wrap, bg=PANEL, fg=FG, font=MONO_SM, relief="flat", wrap="word",
                      padx=12, pady=10, highlightthickness=0)
        txt.grid(row=0, column=0, sticky="nsew")
        sb = ttk.Scrollbar(wrap, command=txt.yview)
        sb.grid(row=0, column=1, sticky="ns")
        txt.configure(yscrollcommand=sb.set)
        txt.insert("1.0", GUIDE)
        txt.configure(state="disabled")

    def render(self, f) -> None:  # estático
        pass
