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
    cfg.rotation_deg[0] = 90        # ROT = informativo (se pliega en ZONEMASK)
    cfg.toggle_zone(0, 5)           # veto una zona → se refleja en ZONEMASK
    cmds = cfg.to_firmware_commands()
    by_text = {c.text: c for c in cmds}
    assert by_text["TOF 0 POS RIGHT"].supported_now is True
    assert by_text["CFG SAVE"].supported_now is True
    # ROT ya NO va al firmware (la orientación se pliega en la máscara cruda): informativo.
    rot = [c for c in cmds if c.text.startswith("TOF 0 ROT 90")]
    assert rot and rot[0].supported_now is False
    # ZONEMASK ahora SÍ lo entiende el firmware y refleja la zona vetada.
    zm = [c for c in cmds if c.text.startswith("TOF 0 ZONEMASK")]
    assert len(zm) == 1 and zm[0].supported_now is True
    val = int(zm[0].text.split()[3], 16)
    assert bin(val).count("1") == 15        # exactamente una zona vetada → 15 bits en 1
    # Todos los comandos son FwCommand con texto no vacío.
    assert all(isinstance(c, FwCommand) and c.text for c in cmds)


def test_firmware_commands_firmware_owns_rotation(monkeypatch):
    # Modo FIRMWARE-DUEÑO (placa con -DTOP_ENABLE_TOF_ROT): la app manda ROT/FLIP REALES
    # (supported_now=True) y la ZONEMASK en marco CANÓNICO (sin plegar); el firmware la rota
    # canónico→crudo con la misma convención (tof_zone_mask_orient). Es el único acople
    # app↔firmware. Default (False) ya está cubierto por test_firmware_commands_marks_supported.
    import monitor_base.tof_layout as tl
    monkeypatch.setattr(tl, "FIRMWARE_OWNS_ROTATION", True)
    cfg = TofLayout()
    cfg.rotation_deg[0] = 90
    for z in (0, 1, 2, 3):          # veto la fila SUPERIOR del display (zonas 0..3)
        cfg.zone_enabled[0][z] = False
    cmds = cfg.to_firmware_commands()
    by_text = {c.text: c for c in cmds}
    # ROT/FLIP ahora SÍ van al firmware (reales, no informativos).
    assert by_text["TOF 0 ROT 90"].supported_now is True
    assert by_text["TOF 0 FLIP NONE"].supported_now is True
    # ZONEMASK en marco CANÓNICO (sin plegar): fila superior vetada → bits 0..3 en 0 → 0xFFF0.
    # (El firmware la rota; no la pliega la app.)
    zm = [c for c in cmds if c.text.startswith("TOF 0 ZONEMASK")]
    assert len(zm) == 1 and zm[0].supported_now is True
    assert int(zm[0].text.split()[3], 16) == 0xFFF0


# ── Máscara CRUDA: paridad orientación+veto (regla del diseño: la app es la fuente
#    de verdad de la composición; el firmware solo aplica el bit crudo) ──────────
def test_raw_zone_mask_default_all_on():
    assert TofLayout().raw_zone_mask(0) == 0xFFFF        # nada vetado → todas activas (no-op)


def test_raw_zone_mask_top_row_no_rotation():
    # Sin rotación display == crudo: vetar la fila superior (display 0..3) anula las
    # zonas CRUDAS 0..3 → bits 0..3 en 0 → 0xFFF0.
    cfg = TofLayout()
    cfg.rotation_deg[0] = 0
    for z in (0, 1, 2, 3):
        cfg.zone_enabled[0][z] = False
    assert cfg.raw_zone_mask(0) == 0xFFF0


def test_raw_zone_mask_top_row_180_maps_to_bottom_raw():
    # CASO CLAVE (ToF izquierdo montado a 180°): vetar la fila SUPERIOR en PANTALLA
    # debe anular las zonas CRUDAS 12..15 (180°: display z ← crudo 15-z), NO las 0..3.
    # Es lo que hace que "anular zonas superiores" funcione en el sensor rotado.
    cfg = TofLayout()
    cfg.rotation_deg[0] = 180
    for z in (0, 1, 2, 3):
        cfg.zone_enabled[0][z] = False
    assert cfg.raw_zone_mask(0) == 0x0FFF                # crudas 12..15 anuladas


# ── Veto INICIAL recomendado: SÓLO la 2ª fila desde abajo (pedido Gustavo) ──────
def test_default_veto_solo_fila_2_activa_en_display():
    # 4×4: fila 2 (zonas display 8..11) ACTIVA; filas 0,1,3 vetadas.
    cfg = TofLayout()
    cfg.apply_default_veto()
    for z in range(N_ZONES):
        r = z // GRID_W
        assert cfg.zone_is_enabled(0, z) == (r == 2), f"zona {z} (fila {r})"


def test_default_veto_raw_mask_rot0_es_0x0F00():
    # Sin rotación, display=crudo → la fila 2 (zonas 8..11) queda activa = 0x0F00.
    cfg = TofLayout()
    cfg.rotation_deg = {i: 0 for i in range(4)}          # forzar rot 0 en todos (idx3 default=180)
    cfg.apply_default_veto()
    assert cfg.raw_zone_mask(0) == 0x0F00


def test_default_veto_raw_mask_sensor_180_mapea_a_fila_de_arriba():
    # ToF a 180° (ej. el izquierdo): la "2ª desde abajo" en PANTALLA cae en las zonas
    # CRUDAS 4..7 (display z ← crudo 15-z → disp 8..11 ← crudo 7..4) = 0x00F0.
    cfg = TofLayout()
    cfg.rotation_deg = {i: 0 for i in range(4)}
    cfg.rotation_deg[3] = 180
    cfg.apply_default_veto()
    assert cfg.raw_zone_mask(3) == 0x00F0


# ── Exportador del layout ESTÁTICO al firmware (hornear FIJO) ───────────────────
def test_flip_to_bits():
    from monitor_base.tof_layout import flip_to_bits
    assert flip_to_bits("none") == 0
    assert flip_to_bits("h") == 1
    assert flip_to_bits("v") == 2


def test_export_static_header_usa_bearing_correcto_sin_bug_derizq():
    # bearings por defecto: FRONT=0, BACK=180, RIGHT=270, LEFT=90 (convención HARDWARE,
    # NO el bug RIGHT=90/LEFT=270). rotación idx3=180 por default; flips 0.
    from monitor_base.tof_layout import export_static_layout_header
    hdr = export_static_layout_header({"TEENSY_ABC123": TofLayout()})
    assert '"TEENSY_ABC123"' in hdr
    assert "{ 0, 180, 270, 90 }" in hdr                  # bearings (der=270, izq=90 = correcto)
    assert "{ 0, 0, 0, 180 }" in hdr                     # rotaciones (izq a 180°)
    assert "TOF_STATIC_LAYOUT_COUNT = 1" in hdr
    assert "#pragma once" in hdr
