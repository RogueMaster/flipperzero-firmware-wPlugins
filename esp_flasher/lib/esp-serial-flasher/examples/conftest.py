import logging
import os
import sys
from datetime import datetime
from typing import List, Optional

import pytest


def pytest_configure(config):
    target = config.getoption("--target")

    if "stm32" in target:
        config.option.embedded_services = "serial"
    elif "raspberry" in target:
        pass
    elif "pi_pico" in target:
        config.option.embedded_services = "serial"
    elif "zephyr" in target:
        config.option.embedded_services = "serial"
    else:
        config.option.embedded_services = "esp,idf"


def pytest_collection_modifyitems(
    session: pytest.Session, config: pytest.Config, items: List[pytest.Item]
):
    """Modify test collection based on selected target"""
    target = config.getoption("--target")

    # Filter the test cases
    filtered_items = []
    for item in items:
        # filter by target
        all_markers = [marker.name for marker in item.iter_markers()]
        if target not in all_markers:
            continue

        filtered_items.append(item)
    items[:] = filtered_items


@pytest.fixture(scope="session", autouse=True)
def session_tempdir() -> str:
    _tmpdir = os.path.join(
        os.path.dirname(__file__),
        "pytest_embedded_log",
        datetime.now().strftime("%Y-%m-%d_%H-%M-%S"),
    )
    os.makedirs(_tmpdir, exist_ok=True)
    return _tmpdir


@pytest.fixture
def config(request: pytest.FixtureRequest) -> str:
    return getattr(request, "param", None)


@pytest.fixture
def build_dir(
    request: pytest.FixtureRequest, app_path: str, config: Optional[str]
) -> str:
    """
    Check local build dir and return the valid one

    Returns:
        valid build directory
    """
    check_dirs = []
    build_folder = os.getenv("CI_BUILD_FOLDER", "build")
    if config is not None:
        check_dirs.append(f"{build_folder}_{config}")
    check_dirs.append(build_folder)

    for check_dir in check_dirs:
        binary_path = os.path.join(app_path, check_dir)
        if os.path.isdir(binary_path):
            logging.info(f"find valid binary path: {binary_path}")
            return check_dir

        logging.warning(
            "checking binary path: %s... missing... try another place", binary_path
        )

    logging.error("no build dir valid. Please build the binary and run pytest again.")
    sys.exit(1)
