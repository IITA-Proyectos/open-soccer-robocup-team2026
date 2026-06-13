"""monitor_base — App de PC de monitoreo/calibración de la placa base (DOWN).

Lee la telemetría USB del firmware de competencia down (monitor USB; la app lo
activa sola al conectar) y muestra de forma simple: el anillo de 32 sensores de luz, la línea detectada, la
interpretación que viaja a la CENTRAL (LineStatusV2) y la odometría OTOS; además
asiste la calibración. Anda con el robot real (--port), con una grabación
(--replay) o con el simulador (--sim).

Contrato: docs/firmware/TELEMETRIA-DOWN.md  |  Módulo firmware: src/shared/telemetry_down.*
"""
from .protocol import (
    Frame, LineStatus, Otos, Ring, ProtocolError,
    parse_line, parse_obj, decode_flags, SCHEMA_VERSION,
)
from .sensor_health import SensorHealthTracker, SensorStatus, Health
from .calibration import CalibrationAssistant, SensorCalib
from .boot_status import BootStatusTracker, classify_boot_line
from .simulator import Simulator
from .recorder import Recorder

__all__ = [
    "Frame", "LineStatus", "Otos", "Ring", "ProtocolError",
    "parse_line", "parse_obj", "decode_flags", "SCHEMA_VERSION",
    "SensorHealthTracker", "SensorStatus", "Health",
    "CalibrationAssistant", "SensorCalib",
    "BootStatusTracker", "classify_boot_line",
    "Simulator", "Recorder",
]

__version__ = "1.0.0"
