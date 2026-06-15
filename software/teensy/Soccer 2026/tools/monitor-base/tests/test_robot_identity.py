"""Tests de la identidad por-robot (anti-cruce R1/R2): registro por N° de serie,
config keyed por serial, y el guard de escrituras a EEPROM. PURO (sin Tk/serial)."""
from monitor_base.robot_registry import identify, load_registry
from monitor_base.safety import is_destructive_write
from monitor_base.tof_layout import config_path_for_serial, default_config_path


# ── robot_registry.identify ─────────────────────────────────────────────────
def test_identify_known_serial():
    rid = identify("19810740")
    assert rid.known
    assert rid.robot == 1
    assert rid.board == "top"
    assert "Robot 1" in rid.name


def test_identify_unknown_serial_keeps_serial_visible():
    rid = identify("ZZZ999")
    assert not rid.known
    assert rid.serial == "ZZZ999"
    assert "ZZZ999" in rid.name        # el serial queda visible para nombrarlo


def test_identify_none_serial():
    rid = identify(None)
    assert not rid.known
    assert rid.serial is None
    assert rid.robot is None


def test_identify_extra_overrides_and_extends():
    extra = {"AAA111": {"robot": 2, "board": "top", "name": "Robot 2 · TOP"}}
    rid = identify("AAA111", extra)
    assert rid.known and rid.robot == 2 and rid.board == "top"


def test_load_registry_missing_file_is_empty():
    assert load_registry("/no/existe/registry.json") == {}


# ── config keyed por serial (R1 y R2 NO comparten archivo) ──────────────────
def test_config_path_per_serial_is_distinct():
    p1 = config_path_for_serial("19810740")
    p2 = config_path_for_serial("22223333")
    assert p1 != p2
    assert "19810740" in p1
    assert p1.endswith(".json")


def test_config_path_none_falls_back_to_legacy():
    assert config_path_for_serial(None) == default_config_path()


def test_config_path_sanitizes_serial():
    p = config_path_for_serial("AB/CD:EF")
    fname = p.replace("\\", "/").split("/")[-1]
    assert fname == "tof_layout_AB_CD_EF.json"


# ── safety.is_destructive_write (guard de la Opción 2) ──────────────────────
def test_destructive_write_saves():
    assert is_destructive_write("CFG SAVE")
    assert is_destructive_write("CAL SAVE")
    assert is_destructive_write("IMU SAVE")
    assert is_destructive_write("cfg save")        # case-insensitive


def test_destructive_write_cfg_reset_yes_otos_reset_no():
    assert is_destructive_write("CFG RESET")       # borra config → confirmar
    assert not is_destructive_write("OTOS RESET")  # re-cero transitorio → no


def test_destructive_write_transient_commands_no():
    for c in ("STREAM ON", "PING", "TOF 0 POS FRONT", "SENSOR 3 OFF", "CAL AUTO ON"):
        assert not is_destructive_write(c)
