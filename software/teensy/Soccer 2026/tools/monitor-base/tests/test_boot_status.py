"""Tests del clasificador de estado de calibración al boot (boot_status.py)."""
from monitor_base.boot_status import (
    CALIB_CALIBRANDO,
    CALIB_EEPROM,
    CALIB_FAILSAFE,
    LINE_EEPROM,
    BootStatusTracker,
    classify_boot_line,
)


# ── classify_boot_line: una regla por test, con prints REALES del firmware ──

def test_calib_cargada_de_eeprom():
    assert classify_boot_line(
        "[DOWN] calib cargada de EEPROM (persistida)") == CALIB_EEPROM


def test_calib_persistida():
    # comm_central.cpp imprime "[DOWN] calib persistida en EEPROM"
    assert classify_boot_line(
        "[DOWN] calib persistida en EEPROM") == CALIB_EEPROM


def test_calib_cargada_eeprom_diag_mayusculas():
    # diag_down_calibracion.cpp: "[CAL] Calib cargada de EEPROM. Umbrales:"
    # → case-insensitive.
    assert classify_boot_line(
        "[CAL] Calib cargada de EEPROM. Umbrales:") == CALIB_EEPROM


def test_eeprom_sin_calib_es_failsafe():
    assert classify_boot_line(
        "[DOWN] EEPROM sin calib valida — usando carpet recien medido"
    ) == CALIB_FAILSAFE


def test_usando_carpet_recien_medido_es_failsafe():
    assert classify_boot_line(
        "usando carpet recien medido") == CALIB_FAILSAFE


def test_fail_safe_es_failsafe():
    assert classify_boot_line("[DOWN] modo fail-safe") == CALIB_FAILSAFE


def test_defaults_es_failsafe():
    assert classify_boot_line("usando defaults") == CALIB_FAILSAFE


def test_calibrando_carpet():
    assert classify_boot_line(
        "[DOWN] calibrando carpet... no mover el robot") == CALIB_CALIBRANDO


def test_line_ring_blanco_de_eeprom():
    assert classify_boot_line(
        "[DOWN] line_ring calibrado con blanco de EEPROM") == LINE_EEPROM


def test_case_insensitive():
    assert classify_boot_line(
        "CALIB CARGADA DE EEPROM") == CALIB_EEPROM
    assert classify_boot_line(
        "Calibrando Carpet ahora") == CALIB_CALIBRANDO


def test_line_ring_gana_a_eeprom_generico():
    # La línea de line_ring también contiene "de EEPROM" pero NO "calib cargada"
    # ni "persistida"; debe clasificar como LINE_EEPROM, no como CALIB_EEPROM.
    assert classify_boot_line(
        "line_ring calibrado con blanco de EEPROM") == LINE_EEPROM


def test_lineas_no_calibracion_devuelven_none():
    assert classify_boot_line("[DOWN] conectado, esperando datos…") is None
    assert classify_boot_line("seq=42 frames OK") is None
    assert classify_boot_line("") is None
    assert classify_boot_line("[TOP] IMU listo") is None


def test_none_input_no_rompe():
    assert classify_boot_line(None) is None


# ── BootStatusTracker ───────────────────────────────────────────────────────

def test_tracker_arranca_vacio():
    t = BootStatusTracker()
    assert t.last_calib_status() is None


def test_tracker_guarda_ultimo_estado_de_calib():
    t = BootStatusTracker()
    t.update("[DOWN] calib cargada de EEPROM (persistida)")
    assert t.last_calib_status() == CALIB_EEPROM


def test_tracker_lineas_no_calib_no_pisan_el_estado():
    t = BootStatusTracker()
    t.update("[DOWN] calib cargada de EEPROM (persistida)")
    # Llegan líneas que no hablan de calibración: el estado persiste.
    ret = t.update("seq=10 telemetría")
    assert ret == CALIB_EEPROM
    assert t.last_calib_status() == CALIB_EEPROM


def test_tracker_actualiza_al_cambiar_de_estado():
    t = BootStatusTracker()
    t.update("[DOWN] calibrando carpet... no mover el robot")
    assert t.last_calib_status() == CALIB_CALIBRANDO
    t.update("[DOWN] EEPROM sin calib valida — usando carpet recien medido")
    assert t.last_calib_status() == CALIB_FAILSAFE


def test_tracker_update_devuelve_estado_actual():
    t = BootStatusTracker()
    # Sin match todavía: devuelve None (el estado actual, que es ninguno).
    assert t.update("línea cualquiera") is None
    assert t.update("[DOWN] calib cargada de EEPROM") == CALIB_EEPROM


def test_tracker_reset_olvida_estado():
    t = BootStatusTracker()
    t.update("[DOWN] calib cargada de EEPROM")
    t.reset()
    assert t.last_calib_status() is None


def test_tracker_secuencia_de_boot_realista():
    # Simula el orden real de prints del DOWN al bootear con EEPROM válida.
    t = BootStatusTracker()
    boot = [
        "[DOWN] arrancando…",
        "[DOWN] calibrando carpet... no mover el robot",
        "[DOWN] calib cargada de EEPROM (persistida)",
        "[DOWN] line_ring calibrado con blanco de EEPROM",
        "seq=0",
    ]
    for ln in boot:
        t.update(ln)
    # El último estado de calib relevante es el de la línea (blanco EEPROM).
    assert t.last_calib_status() == LINE_EEPROM
