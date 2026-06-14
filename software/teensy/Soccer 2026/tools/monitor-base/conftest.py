"""conftest raíz de tools/monitor-base — pone el paquete en sys.path y expone el
golden generado por el firmware C++ como fixture."""
import os
import sys

import pytest

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

GOLDEN_PATH = os.path.join(_HERE, "tests", "golden_frame_v3.jsonl")
GOLDEN_TOP_PATH = os.path.join(_HERE, "tests", "golden_top_v2.jsonl")


@pytest.fixture
def golden_path():
    return GOLDEN_PATH


@pytest.fixture
def golden_line():
    with open(GOLDEN_PATH, "r", encoding="utf-8") as f:
        return f.readline().rstrip("\n")


@pytest.fixture
def golden_top_line():
    with open(GOLDEN_TOP_PATH, "r", encoding="utf-8") as f:
        return f.readline().rstrip("\n")
