"""App-local helpers for real Linux previews on an isolated X11 display."""

import contextlib
import hashlib
import os
import select
import shutil
import struct
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LANGUAGES = ("de-DE", "en-US", "es-ES", "fr-FR", "it-IT", "pt-BR")


def source_hashes():
    paths = [ROOT / "CMakeLists.txt", ROOT / "CMakePresets.json"]
    for folder in (
        "src",
        "tests",
        "translations",
        "assets",
        "store-listings",
        "scripts",
    ):
        paths.extend(
            path
            for path in (ROOT / folder).rglob("*")
            if path.is_file() and "__pycache__" not in path.parts
        )
    return {
        str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(paths)
    }


@contextlib.contextmanager
def virtual_display(executable, output, window_manager=None):
    executable = executable or shutil.which("Xvfb")
    if not executable:
        raise SystemExit("Xvfb is required. Supply its path with --xvfb.")
    window_manager = window_manager or shutil.which("openbox")
    if not window_manager:
        raise SystemExit("Openbox is required. Supply its path with --window-manager.")
    read_fd, write_fd = os.pipe()
    with (
        (output / "xvfb.log").open("w") as log,
        tempfile.TemporaryDirectory(prefix="model-building-projects-xdg-") as runtime,
    ):
        process = subprocess.Popen(
            [
                str(executable),
                "-displayfd",
                str(write_fd),
                "-screen",
                "0",
                "1920x1080x24",
                "-noreset",
                "-nolisten",
                "tcp",
                "-ac",
            ],
            pass_fds=(write_fd,),
            stdout=log,
            stderr=log,
        )
        os.close(write_fd)
        manager = None
        try:
            if not select.select([read_fd], [], [], 15)[0]:
                raise RuntimeError("Xvfb did not start")
            display = os.read(read_fd, 64).decode().strip()
            if not display.isdigit():
                raise RuntimeError("Xvfb returned no display number")
            environment = dict(
                os.environ,
                DISPLAY=f":{display}",
                QT_QPA_PLATFORM="xcb",
                QT_SCALE_FACTOR="1",
                QT_AUTO_SCREEN_SCALE_FACTOR="0",
                XDG_RUNTIME_DIR=runtime,
            )
            environment.pop("WAYLAND_DISPLAY", None)
            environment["XDG_CONFIG_HOME"] = runtime
            config = Path(runtime) / "openbox.xml"
            config.write_text(
                '<openbox_config xmlns="http://openbox.org/3.4/rc"><theme><name>Clearlooks</name></theme><focus><focusNew>yes</focusNew></focus><keyboard><keybind key="A-F10"><action name="ToggleMaximize"/></keybind></keyboard></openbox_config>',
                encoding="utf-8",
            )
            manager = subprocess.Popen(
                [str(window_manager), "--sm-disable", "--config-file", str(config)],
                env=environment,
                stdout=log,
                stderr=log,
            )
            for _ in range(100):
                if manager.poll() is not None:
                    raise RuntimeError("Window manager exited. See xvfb.log")
                result = subprocess.run(
                    ["xprop", "-root", "_NET_SUPPORTING_WM_CHECK"],
                    env=environment,
                    capture_output=True,
                    text=True,
                    check=True,
                )
                if "window id" in result.stdout:
                    break
                time.sleep(0.1)
            else:
                raise RuntimeError("Window manager did not start")
            yield environment
        finally:
            os.close(read_fd)
            if manager:
                manager.terminate()
                manager.wait(timeout=10)
            process.terminate()
            process.wait(timeout=10)


def png_dimensions(path):
    with path.open("rb") as image:
        header = image.read(24)
    assert header[:8] == b"\x89PNG\r\n\x1a\n"
    return struct.unpack(">II", header[16:24])
