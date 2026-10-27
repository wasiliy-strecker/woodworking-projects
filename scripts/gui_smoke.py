"""Sichtbarer X11-Bedienablauf mit eigenen Fenstern und synthetischen Daten."""

import argparse
import fcntl
import hashlib
import json
import os
import re
import sqlite3
import subprocess
import tempfile
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--clean-environment", action="store_true")
    args = parser.parse_args()
    # This lock covers startup, every event and capture, shutdown and cleanup.
    with open("/tmp/app-factory-cpp-visible-ui.lock", "a") as lock:
        if os.environ.get("DISPLAY", "").split(".")[0] == ":0":
            print("Warte auf gemeinsame sichtbare UI-Sperre", flush=True)
            fcntl.flock(lock, fcntl.LOCK_EX)
        exercise(args)


def exercise(args):
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    if args.clean_environment:
        for key in list(environment):
            if key.startswith(("QT_", "QML_", "HOLZWERKPROJEKT_")) or key in (
                "LD_LIBRARY_PATH",
                "CMAKE_PREFIX_PATH",
            ):
                environment.pop(key)
        environment["PATH"] = "/usr/bin:/bin"
    environment.update(
        QT_QPA_PLATFORM="xcb", QT_SCALE_FACTOR="1", QT_QPA_PLATFORMTHEME="gtk3"
    )
    focused = None
    process = None

    def command(*arguments):
        return subprocess.check_output(
            list(arguments), env=environment, text=True, stderr=subprocess.PIPE
        ).strip()

    def wait_for(predicate, timeout=8):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            try:
                value = predicate()
                if value:
                    return value
            except (subprocess.CalledProcessError, sqlite3.OperationalError):
                pass
            time.sleep(0.08)
        raise AssertionError("GUI-Zustand nicht rechtzeitig erreicht")

    def window(title):
        nonlocal focused

        def find():
            ids = command(
                "xdotool",
                "search",
                "--onlyvisible",
                "--all",
                "--pid",
                str(process.pid),
                "--name",
                title,
            ).splitlines()
            candidates = []
            for identifier in ids:
                geometry = dict(
                    line.split("=", 1)
                    for line in command(
                        "xdotool", "getwindowgeometry", "--shell", identifier
                    ).splitlines()
                )
                area = int(geometry["WIDTH"]) * int(geometry["HEIGHT"])
                if area > 20000 and command(
                    "xdotool", "getwindowpid", identifier
                ) == str(process.pid):
                    candidates.append((area, identifier))
            return max(candidates)[1] if candidates else None

        focused = wait_for(find)
        command("xdotool", "windowactivate", "--sync", focused)
        command("xdotool", "windowraise", focused, "windowfocus", "--sync", focused)
        time.sleep(0.2)
        return focused

    def key(*values):
        for value in values:
            assert command("xdotool", "getwindowpid", focused) == str(process.pid)
            command("xdotool", "windowfocus", focused)
            actual = command("xdotool", "getwindowfocus")
            assert command("xdotool", "getwindowpid", actual) == str(process.pid)
            # Qt shortcuts need real XTest events on this desktop. Never send
            # them unless the explicitly focused window belongs to our process.
            command("xdotool", "key", "--clearmodifiers", value)
            time.sleep(0.16)

    def text(value):
        # Clipboard paste preserves composed and decomposed Unicode exactly.
        subprocess.run(
            ["xsel", "--clipboard", "--input"],
            input=value,
            text=True,
            env=environment,
            check=True,
        )
        key("ctrl+v")

    def capture(name):
        time.sleep(0.2)
        command("import", "-window", focused, str(output / f"{name}.png"))
        if name == "04-projekt-gespeichert" and os.environ.get(
            "HOLZWERKPROJEKT_OUTER_WINDOW"
        ):
            subprocess.run(
                [
                    "import",
                    "-window",
                    os.environ["HOLZWERKPROJEKT_OUTER_WINDOW"],
                    str(output / "sichtbarer-pruefbildschirm.png"),
                ],
                env=dict(
                    os.environ,
                    DISPLAY=os.environ.get("HOLZWERKPROJEKT_OUTER_DISPLAY", ":0"),
                ),
                check=True,
            )
        print(name, flush=True)

    def click(x, y):
        assert command("xdotool", "getwindowpid", focused) == str(process.pid)
        # Read the client origin rather than the window manager's frame origin.
        info = command("xwininfo", "-id", focused)
        left = int(re.search(r"Absolute upper-left X:\s+(-?\d+)", info)[1])
        top = int(re.search(r"Absolute upper-left Y:\s+(-?\d+)", info)[1])
        command(
            "xdotool", "mousemove", "--sync", str(left + x), str(top + y), "click", "1"
        )
        time.sleep(0.2)

    def destructive():
        # Destructive button is first, safe default is second on the tested Linux style.
        key("Left", "space")

    with tempfile.TemporaryDirectory(prefix="holzwerkprojekt-gui-") as temporary:
        directory = Path(temporary)
        database = directory / "holzwerkprojekt.sqlite3"
        source = directory / "regal.png"
        replacement = directory / "holz.jpg"
        svg = directory / "synthetic.svg"
        svg.write_text(
            '<svg xmlns="http://www.w3.org/2000/svg" width="640" height="480"><rect width="640" height="480" fill="#e9eee8"/><ellipse cx="320" cy="425" rx="205" ry="20" fill="#c3cec4"/><path d="M140 95 H170 V420 H140 Z M470 95 H500 V420 H470 Z" fill="#805d3f"/><path d="M150 130 H490 V158 H150 Z M150 265 H490 V293 H150 Z M150 395 H490 V420 H150 Z" fill="#b98f60"/><path d="M165 145 H480 M165 278 H480 M165 408 H480" stroke="#d7b88c" stroke-width="5"/><rect x="204" y="213" width="70" height="52" rx="6" fill="#638e79"/><rect x="324" y="333" width="109" height="62" rx="6" fill="#d9c7a6"/></svg>'
        )
        command("convert", str(svg), str(source))
        command("convert", str(svg), "-modulate", "100,80,105", str(replacement))
        original_bytes = source.read_bytes()
        replacement_bytes = replacement.read_bytes()
        log = (output / "app.log").open("w")

        def query(statement, parameters=()):
            with sqlite3.connect(
                f"file:{database}?mode=ro", uri=True, timeout=0.1
            ) as connection:
                return connection.execute(statement, parameters).fetchall()

        def rows():
            return query(
                "SELECT id,name,material,location,note,status,photo FROM projects ORDER BY id"
            )

        def steps(project_id=1):
            return query(
                "SELECT id,text,completed,position FROM checklist_steps WHERE project_id=? ORDER BY position",
                (project_id,),
            )

        def start():
            nonlocal process
            process = subprocess.Popen(
                [str(binary), "--data-dir", str(directory)],
                cwd=directory,
                env=environment,
                stdout=log,
                stderr=log,
            )
            window("^HolzwerkProjekt$")
            command("xdotool", "windowsize", focused, "1100", "800")
            command("xdotool", "windowmove", focused, "80", "60")
            wait_for(lambda: query("PRAGMA user_version") == [(1,)])
            time.sleep(0.5)

        def stop():
            window("^HolzwerkProjekt$")
            key("ctrl+q")
            process.wait(timeout=6)
            assert process.returncode == 0

        def edit():
            window("^HolzwerkProjekt$")
            time.sleep(0.5)
            key("alt+b")
            window("^Projekt bearbeiten$")

        def save():
            key("ctrl+s")
            try:
                wait_for(
                    lambda: (
                        subprocess.run(
                            [
                                "xdotool",
                                "search",
                                "--onlyvisible",
                                "--all",
                                "--pid",
                                str(process.pid),
                                "--name",
                                "^Projekt (hinzufügen|bearbeiten)$",
                            ],
                            env=environment,
                            check=False,
                            stdout=subprocess.DEVNULL,
                        ).returncode
                        != 0
                    )
                )
            except AssertionError:
                capture("unerwarteter-speicherfehler")
                raise
            window("^HolzwerkProjekt$")
            time.sleep(0.6)

        def picker(path=None):
            key("alt+f")
            window("Projektfoto auswählen")
            capture("native-dateiauswahl")
            if path is None:
                key("Escape")
            else:
                key("ctrl+l")
                text(str(path))
                key("Return")
                time.sleep(0.7)
            window("^Projekt (hinzufügen|bearbeiten)$")
            time.sleep(0.3)

        def discard():
            key("Escape")
            window("Änderungen verwerfen")
            destructive()
            window("^HolzwerkProjekt$")

        def step_field(index):
            key("alt+c")
            for _ in range(index):
                key("Tab", "Tab", "Tab")
            key("Tab")

        try:
            start()
            assert rows() == []
            capture("01-leere-liste")
            click(95, 765)
            window("^Projekt hinzufügen$")
            key("ctrl+s")
            capture("02-pflichtfeld")
            key("alt+p")
            text("Werkstattregal")
            key("alt+m")
            text("Eiche und Birke")
            key("alt+a")
            text("Werkstatt, Wand links")
            key("alt+n")
            text("Oberfläche vor dem Ölen prüfen.")
            key("Return")
            text("  Maße am Probestück kontrollieren.")
            key("alt+s", "Home", "Down")
            for value in ("Holz zuschneiden", "Kanten schleifen", "Oberfläche ölen"):
                key("alt+z")
                text(value)
            step_field(0)
            key("shift+Tab", "space")
            picker()
            picker(source)
            capture("03-entwurf-mit-foto")
            save()
            wait_for(lambda: len(rows()) == 1)
            first = rows()[0]
            assert first[1:6] == (
                "Werkstattregal",
                "Eiche und Birke",
                "Werkstatt, Wand links",
                "Oberfläche vor dem Ölen prüfen.\n  Maße am Probestück kontrollieren.",
                "started",
            )
            assert first[6] == original_bytes
            initial_steps = steps()
            assert [(s[1], s[2], s[3]) for s in initial_steps] == [
                ("Holz zuschneiden", 1, 0),
                ("Kanten schleifen", 0, 1),
                ("Oberfläche ölen", 0, 2),
            ]
            capture("04-projekt-gespeichert")
            key("ctrl+f")
            text("O\u0308LEN")
            capture("05-unicode-schrittsuche")
            key("ctrl+a")
            text("unauffindbar")
            capture("06-keine-treffer")
            key("ctrl+a", "BackSpace")
            edit()
            step_field(0)
            key("shift+Tab", "space")
            key("Escape")
            window("Änderungen verwerfen")
            capture("07-haekchen-sichere-vorauswahl")
            key("Return")
            window("^Projekt bearbeiten$")
            discard()
            assert steps() == initial_steps
            edit()
            # Remove middle row, then rename the correct surviving ID.
            step_field(1)
            key("Tab", "space")
            step_field(1)
            key("ctrl+a")
            text("Oberfläche zweimal ölen")
            key("shift+Tab", "space")
            step_field(0)
            key("shift+Tab", "space")
            key("alt+z")
            key("ctrl+s")
            capture("08-leerer-schritt-erhaelt-entwurf")
            step_field(2)
            text("Beschläge montieren")
            key("alt+s", "End")
            picker(replacement)
            capture("09-checkliste-bearbeitet")
            save()
            edited_steps = steps()
            assert edited_steps[0][0] == initial_steps[0][0] and edited_steps[0][2] == 0
            assert edited_steps[1][0] == initial_steps[2][0]
            assert edited_steps[1][1:] == ("Oberfläche zweimal ölen", 1, 2)
            assert edited_steps[2][1:] == ("Beschläge montieren", 0, 3)
            assert rows()[0][5:] == ("finished", replacement_bytes)
            assert source.read_bytes() == original_bytes
            assert replacement.read_bytes() == replacement_bytes
            # Back to Geplant, then Angefangen with all other values unchanged.
            for status, keys in (("planned", ("Home",)), ("started", ("Home", "Down"))):
                before = rows()[0]
                edit()
                key("alt+s", *keys)
                save()
                assert rows()[0][5] == status
                assert rows()[0][:5] == before[:5] and rows()[0][6] == before[6]
                assert steps() == edited_steps
            # A real late SQL failure must keep the entire dialog alive.
            with sqlite3.connect(database) as db:
                db.execute(
                    "CREATE TRIGGER fail_late BEFORE INSERT ON checklist_steps WHEN NEW.position=2 BEGIN SELECT RAISE(ABORT,'Synthetischer Testfehler'); END"
                )
            before = rows()[0]
            edit()
            key("alt+a", "ctrl+a")
            text("Keller")
            key("ctrl+s")
            time.sleep(0.6)
            window("^Projekt bearbeiten$")
            capture("10-spaeter-speicherfehler")
            assert rows()[0] == before and steps() == edited_steps
            with sqlite3.connect(database) as db:
                db.execute("DROP TRIGGER fail_late")
            save()
            assert rows()[0][3] == "Keller" and steps() == edited_steps
            # Photo-only remove and discard, then a persistent removal on duplicate.
            edit()
            key("alt+e")
            discard()
            assert rows()[0][6] == replacement_bytes
            click(95, 765)
            window("^Projekt hinzufügen$")
            text("Werkstattregal")
            picker(source)
            key("alt+z")
            text("Unabhängiger Schritt")
            save()
            assert len(rows()) == 2
            assert rows()[1][0] != first[0] and steps(2)[0][0] != initial_steps[0][0]
            edit()
            key("alt+e")
            save()
            assert rows()[1][6] is None and rows()[0][6] == replacement_bytes
            capture("11-gleiche-namen")
            stop()
            source.unlink()
            replacement.unlink()
            start()
            assert len(rows()) == 2
            assert rows()[0][6] == replacement_bytes and steps() == edited_steps
            capture("12-neustart-ohne-bildquelle")
            if args.clean_environment:
                paths = sorted(
                    {
                        line.split()[-1]
                        for line in Path(f"/proc/{process.pid}/maps")
                        .read_text()
                        .splitlines()
                        if any(
                            part in line
                            for part in (
                                "libQt6",
                                "libicu",
                                "libqsqlite",
                                "libqjpeg",
                                "libqgtk3",
                            )
                        )
                    }
                )
                assert any("libqsqlite" in p for p in paths)
                assert any("libqjpeg" in p for p in paths)
                assert all(str(binary.parent) in p for p in paths), paths
                (output / "loaded-libraries.json").write_text(
                    json.dumps(paths, indent=2)
                )
            # Selected first project, cancel deletion then confirm.
            key("alt+l")
            window("Projekt löschen")
            capture("13-loeschabbruch")
            key("Return")
            window("^HolzwerkProjekt$")
            assert len(rows()) == 2
            key("alt+l")
            window("Projekt löschen")
            destructive()
            window("^HolzwerkProjekt$")
            wait_for(lambda: len(rows()) == 1)
            assert rows()[0][0] == 2 and len(steps(2)) == 1 and steps(1) == []
            key("alt+l")
            window("Projekt löschen")
            destructive()
            window("^HolzwerkProjekt$")
            wait_for(lambda: rows() == [])
            assert query("SELECT count(*) FROM checklist_steps") == [(0,)]
            capture("14-wieder-leer")
            stop()
            (output / "result.json").write_text(
                json.dumps(
                    {
                        "passed": True,
                        "binary": str(binary),
                        "visible_display": os.environ.get(
                            "HOLZWERKPROJEKT_VISIBLE_DISPLAY",
                            environment.get("DISPLAY"),
                        ),
                        "native_picker": "GTK3",
                        "photo_sha256": hashlib.sha256(replacement_bytes).hexdigest(),
                        "persisted_step_positions": [s[3] for s in edited_steps],
                    },
                    indent=2,
                )
            )
            print("Vollständiger Bedienablauf bestanden", flush=True)
        finally:
            if process is not None and process.poll() is None:
                # Only the process launched by this test is stopped.
                process.terminate()
                process.wait(timeout=6)
            log.close()


if __name__ == "__main__":
    main()
