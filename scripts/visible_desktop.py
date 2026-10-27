"""Eigenen sichtbaren Xephyr mit Openbox starten und den GUI-Ablauf ausführen."""

import argparse
import fcntl
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--wm-root",
        type=Path,
        help="Optionaler entpackter Openbox-Präfix mit usr und etc",
    )
    args, smoke_args = parser.parse_known_args()
    root = Path(__file__).resolve().parents[1]
    output = root / "build/manual/desktop"
    output.mkdir(parents=True, exist_ok=True)
    title = f"HolzwerkProjekt Pruefbildschirm {os.getpid()}"
    outer_env = dict(os.environ)
    outer_env.setdefault("DISPLAY", ":0")
    manager = (
        str(args.wm_root.resolve() / "usr/bin/openbox")
        if args.wm_root
        else shutil.which("openbox")
    )
    if not manager or not shutil.which("Xephyr"):
        raise SystemExit(
            "Xephyr und Openbox werden für diesen zusätzlichen sichtbaren Test benötigt."
        )
    with open("/tmp/app-factory-cpp-visible-ui.lock", "a") as lock:
        print("Warte auf gemeinsame sichtbare UI-Sperre", flush=True)
        fcntl.flock(lock, fcntl.LOCK_EX)
        read_fd, write_fd = os.pipe()
        with (
            (output / "xephyr.log").open("w") as server_log,
            (output / "openbox.log").open("w") as wm_log,
        ):
            server = subprocess.Popen(
                [
                    "Xephyr",
                    "-displayfd",
                    str(write_fd),
                    "-screen",
                    "1440x1050",
                    "-title",
                    title,
                    "-nolisten",
                    "tcp",
                    "-ac",
                ],
                pass_fds=(write_fd,),
                env=outer_env,
                stdout=server_log,
                stderr=server_log,
            )
            os.close(write_fd)
            wm = None
            try:
                number = os.read(read_fd, 100).decode().strip()
                if not number.isdecimal():
                    raise RuntimeError(
                        "Der eigene sichtbare X11-Bildschirm konnte nicht gestartet werden."
                    )
                display = ":" + number
                own = dict(os.environ, DISPLAY=display)
                wm_env = dict(own)
                wm_args = [manager]
                if args.wm_root:
                    prefix = args.wm_root.resolve()
                    wm_env.update(
                        LD_LIBRARY_PATH=str(prefix / "usr/lib/x86_64-linux-gnu"),
                        XDG_DATA_DIRS=str(prefix / "usr/share") + ":/usr/share",
                        XDG_CONFIG_DIRS=str(prefix / "etc/xdg") + ":/etc/xdg",
                    )
                    wm_args += ["--config-file", str(prefix / "etc/xdg/openbox/rc.xml")]
                wm = subprocess.Popen(wm_args, env=wm_env, stdout=wm_log, stderr=wm_log)
                time.sleep(1)
                # Xephyr does not always publish _NET_WM_PID. Its unique title
                # is chosen before launch and belongs only to this owned server.
                outer = None
                for _ in range(50):
                    matches = subprocess.run(
                        ["xdotool", "search", "--onlyvisible", "--name", title],
                        env=outer_env,
                        text=True,
                        capture_output=True,
                        check=False,
                    )
                    if matches.returncode == 0:
                        outer = matches.stdout.splitlines()[-1]
                        break
                    time.sleep(0.1)
                if outer is None:
                    raise RuntimeError(
                        "Das eigene Xephyr-Fenster wurde nicht sichtbar."
                    )
                subprocess.run(
                    ["xdotool", "windowactivate", "--sync", outer],
                    env=outer_env,
                    check=True,
                )
                own["HOLZWERKPROJEKT_OUTER_WINDOW"] = outer
                own["HOLZWERKPROJEKT_OUTER_DISPLAY"] = outer_env["DISPLAY"]
                own["HOLZWERKPROJEKT_VISIBLE_DISPLAY"] = (
                    outer_env["DISPLAY"] + " via Xephyr " + display
                )
                print("Eigener sichtbarer Testbildschirm " + display, flush=True)
                subprocess.run(
                    [sys.executable, str(root / "scripts/gui_smoke.py"), *smoke_args],
                    env=own,
                    check=True,
                )
            finally:
                os.close(read_fd)
                if wm is not None and wm.poll() is None:
                    wm.terminate()
                    wm.wait(timeout=5)
                if server.poll() is None:
                    server.terminate()
                    server.wait(timeout=5)


if __name__ == "__main__":
    main()
