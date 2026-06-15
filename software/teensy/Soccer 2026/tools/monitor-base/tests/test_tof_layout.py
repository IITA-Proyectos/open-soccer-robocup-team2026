"""Tests de tof_layout.py — config de ToF lado app (orientación, veto, modelo de
pared, persistencia, comandos a firmware). Puro, sin GUI."""
from __future__ import annotations

from monitor_base.tof_layout import (
    GRID_W, N_ZONES, FwCommand, TofLayout, WallField, load_or_default,
    zone_source_map,
)


# ── Permutación de zonas (rotación + espejo) ────────────────────────────────
def test_source_map_identity():
    assert zone_source_map(0, "none") == list(range(N_ZONES))


def test_source_map_180_reverses():
    # Rotar 180° una grilla 4×4: la celda de display i sale de la cruda 15-i.
    assert zone_source_map(180, "none") == list(range(N_ZONES - 1, -1, -1))


def test_source_map_90_is_permutation_and_cycles():
    p = zone_source_map(90, "none")
    assert sorted(p) == list(range(N_ZONES))          # es una permutación
    # rotar 90 cuatro veces = identidad (componer la permutación 4 veces)
    cur = list(range(N_ZONES))
    for _ in range(4):
        cur = [cur[p[i]] for i in range(N_ZONES)]
    assert cur == list(range(N_ZONES))


def test_flip_h_is_own_inverse():
    p = zone_source_map(0, "h")
    assert [p[p[i]] for i in range(N_ZONES)] == list(range(N_ZONES))


# ── oriented_cells ──────────────────────────────────────────────────────────
def test_oriented_cells_180_reverses_values():
    cfg = TofLayout()
    cfg.rotation_deg[0] = 180
    raw = list(range(N_ZONES))            # 0..15 como "distancias"
    out = cfg.oriented_cells(raw, 0)
    assert out == list(range(N_ZONES - 1, -1, -1))


def test_default_left_sensor_is_180():
    # idx 3 = izquierdo, viene montado 180° por defecto (espejo de zones.py).
    cfg = TofLayout()
    assert cfg.rotation_deg[3] == 180


# ── Veto de zonas ───────────────────────────────────────────────────────────
def test_toggle_zone():
    cfg = TofLayout()
    assert cfg.zone_is_enabled(0, 5) is True
    assert cfg.toggle_zone(0, 5) is False
    assert cfg.zone_is_enabled(0, 5) is False
    assert cfg.toggle_zone(0, 5) is True
    assert cfg.zone_is_enabled(0, 5) is True


# ── Modelo de pared / cancha ────────────────────────────────────────────────
def test_suggest_vetoed_rows_top_rows_over_wall():
    # Pared baja, ToF mirando al frente (tilt 0): las 2 filas de arriba ven por
    # encima de la pared al llegar a la pared de referencia → se sugieren vetar.
    cfg = TofLayout(wall=WallField(
        wall_height_mm=140, mount_height_mm=90, tilt_down_deg=0,
        vfov_deg=60, field_width_mm=1820, field_height_mm=2430))
    assert cfg.suggest_vetoed_rows() == [0, 1]


def test_tilt_down_keeps_more_rows():
    # Si el ToF apunta MÁS hacia abajo, menos filas ven por encima de la pared.
    cfg = TofLayout(wall=WallField(
        wall_height_mm=140, mount_height_mm=90, tilt_down_deg=20,
        vfov_deg=60, field_width_mm=1820, field_height_mm=2430))
    vetoed = cfg.suggest_vetoed_rows()
    assert 3 not in vetoed and 2 not in vetoed     # las de abajo nunca


def test_apply_row_veto_disables_those_rows_all_sensors():
    cfg = TofLayout()
    cfg.apply_row_veto([0, 1])
    for idx in range(4):
        for z in range(N_ZONES):
            row = z // GRID_W
            assert cfg.zone_is_enabled(idx, z) == (row not in (0, 1))


# ── Persistencia ────────────────────────────────────────────────────────────
def test_roundtrip_dict():
    cfg = TofLayout()
    cfg.position[2] = "LEFT"
    cfg.flip[1] = "v"
    cfg.toggle_zone(0, 3)
    cfg.wall.wall_height_mm = 200
    back = TofLayout.from_dict(cfg.to_dict())
    assert back.position[2] == "LEFT"
    assert back.flip[1] == "v"
    assert back.zone_is_enabled(0, 3) is False
    assert back.wall.wall_height_mm == 200


def test_save_load_file(tmp_path):
    cfg = TofLayout()
    cfg.rotation_deg[0] = 90
    cfg.apply_row_veto([0])
    p = tmp_path / "tof.json"
    cfg.save(str(p))
    back = TofLayout.load(str(p))
    assert back.rotation_deg[0] == 90
    assert back.zone_is_enabled(0, 0) is False     # fila 0 vetada


def test_load_or_default_missing_returns_default(tmp_path):
    cfg = load_or_default(str(tmp_path / "no-existe.json"))
    assert isinstance(cfg, TofLayout)
    assert cfg.zone_is_enabled(0, 0) is True


# ── Comandos a firmware ─────────────────────────────────────────────────────
def test_firmware_commands_marks_supported():
    cfg = TofLayout()
    cfg.position[0] = "RIGHT"
    cfg.rotation_deg[0] = 90        # ROT = pendiente firmware
    cfg.toggle_zone(0, 5)           # ZONE OFF = pendiente firmware
    cmds = cfg.to_firmware_commands()
    by_text = {c.text: c for c in cmds}
    assert by_text["TOF 0 POS RIGHT"].supported_now is True
    assert by_text["CFG SAVE"].supported_now is True
    assert by_text["TOF 0 ROT 90"].supported_now is False
    assert by_text["TOF 0 ZONE 5 OFF"].supported_now is False
    # Todos los comandos son FwCommand con texto no vacío.
    assert all(isinstance(c, FwCommand) and c.text for c in cmds)
