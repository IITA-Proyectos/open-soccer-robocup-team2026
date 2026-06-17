"""Smoke headless del panel de calibración de la CENTRAL.

Construye el panel con un PanelContext fake (cuyo send() acumula los comandos),
renderiza un frame con ccfg sin crashear, y verifica que un "Aplicar" genera los
comandos SET correctos. Se saltea si no hay display (CI headless). El test PURO de
generación de comandos NO depende de Tk: vive aparte para correr siempre.

Patrón de headless: igual que tests/test_gui_health_smoke.py.
"""
import pytest

from monitor_base.protocol_central import CalibConfig, parse_obj_central


def _v1_obj(**over):
    obj = {
        "central": 1, "v": 1, "seq": 7, "t_ms": 100,
        "fsm": {"role": "ATK", "state": "IDLE", "match": 0},
        "cmd": {"vx": 0, "vy": 0, "w": 0, "spin": 0},
        "pwm": [0, 0, 0],
        "top": {"rxB": 0, "fr": 0, "crc": 0, "rsy": 0, "badsz": 0,
                "fresh": 1, "age": 0},
        "down": {"rx": 0, "crc": 0, "lost": 0, "rsy": 0, "badsch": 0,
                 "line_fresh": 1, "valid": 1, "ev": 0, "age": 0},
        "otos": {"fresh": 1, "hdg": 0.0},
        "snap": {"ball_vis": 0, "ball_x": 0, "ball_y": 0, "goal_own_ang": 0.0,
                 "goal_own_dist": 0, "referee": 0, "flags": 0},
        "loop": {"max_us": 0, "ema_us": 0},
    }
    for k, v in over.items():
        if isinstance(v, dict) and isinstance(obj.get(k), dict):
            obj[k].update(v)
        else:
            obj[k] = v
    return obj


class _FakeCtx:
    """PanelContext mínimo: send() acumula, log() no hace nada."""
    def __init__(self):
        self.sent = []
        self.is_sim = False
        self.config_path = None

    def send(self, cmd):
        self.sent.append(cmd)

    def log(self, level, msg):
        pass


def _make_panel():
    """Crea root oculto + panel. Saltea si no hay display."""
    tk = pytest.importorskip("tkinter")
    try:
        root = tk.Tk()
    except tk.TclError:
        pytest.skip("sin display (headless)")
    root.withdraw()
    from monitor_base.panel_central_calib import CentralCalibPanel
    ctx = _FakeCtx()
    panel = CentralCalibPanel(root, ctx)
    return root, panel, ctx


def test_panel_constructs_and_renders_ccfg():
    root, panel, ctx = _make_panel()
    try:
        f = parse_obj_central(_v1_obj(ccfg={
            "min_pwm": [70, 70, 107], "eff": [100, 100, 115],
            "fwd_l": 120, "fwd_r": 118, "gkp": 1500, "gki": 10, "gkd": 300,
        }))
        panel.render(f)
        root.update_idletasks()
        # Los campos se refrescaron con los valores del robot.
        assert panel.fields["min2"].var.get() == "107"
        assert panel.fields["eff2"].var.get() == "115"
        assert panel.fields["gkp"].var.get() == "1500"
    finally:
        try:
            root.destroy()
        except Exception:
            pass


def test_panel_renders_frame_without_ccfg():
    """Un frame sin ccfg no debe crashear (firmware sin calibración)."""
    root, panel, ctx = _make_panel()
    try:
        panel.render(parse_obj_central(_v1_obj()))
        root.update_idletasks()
    finally:
        try:
            root.destroy()
        except Exception:
            pass


def test_apply_all_generates_set_commands():
    root, panel, ctx = _make_panel()
    try:
        # Cargar valores conocidos vía un frame con ccfg.
        f = parse_obj_central(_v1_obj(ccfg={
            "min_pwm": [70, 71, 107], "eff": [100, 101, 115],
            "fwd_l": 120, "fwd_r": 118, "gkp": 1500, "gki": 10, "gkd": 300,
        }))
        panel.render(f)
        ctx.sent.clear()
        panel._on_apply_all()
        sent = ctx.sent
        assert "SET MINPWM 0 70" in sent
        assert "SET MINPWM 1 71" in sent
        assert "SET MINPWM 2 107" in sent
        assert "SET EFF 0 100" in sent
        assert "SET EFF 2 115" in sent
        assert "SET FWDL 120" in sent
        assert "SET FWDR 118" in sent
        assert "SET GKP 1500" in sent
        assert "SET GKI 10" in sent
        assert "SET GKD 300" in sent
        assert "SAVE" not in sent   # Aplicar NO persiste
    finally:
        try:
            root.destroy()
        except Exception:
            pass


def test_get_and_save_buttons_send_commands():
    root, panel, ctx = _make_panel()
    try:
        panel._on_get()
        assert ctx.sent == ["GET"]
        ctx.sent.clear()
        panel.render(parse_obj_central(_v1_obj(ccfg={
            "min_pwm": [70, 70, 107], "eff": [100, 100, 115],
            "fwd_l": 120, "fwd_r": 118, "gkp": 1500, "gki": 10, "gkd": 300,
        })))
        ctx.sent.clear()
        panel._on_save()
        # SAVE va último, precedido de los SET (persistir lo que está en pantalla).
        assert ctx.sent[-1] == "SAVE"
        assert any(c.startswith("SET ") for c in ctx.sent)
    finally:
        try:
            root.destroy()
        except Exception:
            pass


def test_out_of_range_value_is_clamped_before_send():
    root, panel, ctx = _make_panel()
    try:
        # Piso PWM por encima del máximo (149) → clamp a 149 en el comando.
        panel.fields["min0"].var.set("999")
        cmd = panel.fields["min0"].command()
        assert cmd == "SET MINPWM 0 149"
        # Eficiencia por debajo del mínimo (50) → clamp a 50.
        panel.fields["eff0"].var.set("0")
        assert panel.fields["eff0"].command() == "SET EFF 0 50"
        # Ganancia por encima del máximo (32000) → clamp.
        panel.fields["gkp"].var.set("99999")
        assert panel.fields["gkp"].command() == "SET GKP 32000"
        # Campo vacío → sin comando (None).
        panel.fields["gkd"].var.set("")
        assert panel.fields["gkd"].command() is None
    finally:
        try:
            root.destroy()
        except Exception:
            pass
