"""Native color picker check using only owned windows and temporary collections."""
import argparse
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
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    # Run inside an owned Xephyr display. The outer runner holds the shared UI lock.
    if os.environ.get('DISPLAY', '').split('.')[0] in ('', ':0'):
        raise SystemExit('Use a separate visible test display for this script.')
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    for key in list(env):
        if key.startswith(('QT_', 'QML_', 'HOLZWERKPROJEKT_')) or key in ('LD_LIBRARY_PATH', 'CMAKE_PREFIX_PATH'):
            env.pop(key)
    env.update(QT_QPA_PLATFORM='xcb', QT_QPA_PLATFORMTHEME='gtk3', PATH='/usr/bin:/bin')
    process = None
    focused = None

    def command(*arguments):
        return subprocess.check_output(arguments, text=True, env=env, stderr=subprocess.DEVNULL).strip()

    def wait(predicate, timeout=10):
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            try:
                result = predicate()
                if result:
                    return result
            except (subprocess.CalledProcessError, sqlite3.OperationalError):
                pass
            time.sleep(0.08)
        raise AssertionError('Expected GUI state did not appear')

    def window(title):
        nonlocal focused
        def find():
            matches = command('xdotool', 'search', '--onlyvisible', '--all', '--pid', str(process.pid), '--name', '^' + re.escape(title) + '$').splitlines()
            candidates = []
            for identifier in matches:
                geometry = dict(line.split('=', 1) for line in command('xdotool', 'getwindowgeometry', '--shell', identifier).splitlines())
                area = int(geometry['WIDTH']) * int(geometry['HEIGHT'])
                if area > 10000 and command('xdotool', 'getwindowpid', identifier) == str(process.pid):
                    candidates.append((area, identifier))
            return max(candidates)[1] if candidates else None
        focused = wait(find)
        command('xdotool', 'windowactivate', '--sync', focused)
        time.sleep(0.15)

    def key(*keys):
        assert command('xdotool', 'getwindowpid', focused) == str(process.pid)
        command('xdotool', 'windowfocus', '--sync', focused)
        command('xdotool', 'key', '--clearmodifiers', *keys)
        time.sleep(0.15)

    def select_color():
        assert command('xdotool', 'getwindowpid', focused) == str(process.pid)
        geometry = dict(line.split('=', 1) for line in command(
            'xdotool', 'getwindowgeometry', '--shell', focused).splitlines())
        command('xdotool', 'mousemove', '--window', focused,
                str(int(geometry['WIDTH']) - 50), str(int(geometry['HEIGHT']) - 22),
                'click', '1')
        time.sleep(0.3)

    def capture(name):
        command('import', '-window', focused, str(output / (name + '.png')))

    with tempfile.TemporaryDirectory(prefix='holzwerkprojekt-color-native-') as directory:
        data = Path(directory)
        (data / 'language.txt').write_text('de-DE\n')
        database = data / 'holzwerkprojekt.sqlite3'
        with (output / 'app.log').open('w') as log:
            def start():
                nonlocal process
                process = subprocess.Popen([str(binary), '--data-dir', directory], env=env, stdout=log, stderr=log)
                window('Woodworking Projects')
                def ready():
                    with sqlite3.connect(f'file:{database}?mode=ro', uri=True) as connection:
                        return connection.execute('PRAGMA user_version').fetchone() == (2,)
                wait(ready)
                time.sleep(0.3)

            def colors():
                with sqlite3.connect(f'file:{database}?mode=ro', uri=True) as connection:
                    return connection.execute('SELECT color FROM projects ORDER BY id').fetchall()

            def stop():
                window('Woodworking Projects')
                key('ctrl+q')
                process.wait(timeout=8)

            try:
                start()
                key('ctrl+n')
                window('Projekt hinzufügen')
                command('xdotool', 'type', '--clearmodifiers', 'Native color sample')
                key('Tab', 'Tab', 'space')
                window('Farbe auswählen')
                capture('native-color-picker')
                select_color()
                window('Projekt hinzufügen')
                key('ctrl+s')
                wait(lambda: colors() == [('#FFFFFF',)])
                window('Woodworking Projects')
                capture('saved-color')
                stop()
                start()
                assert colors() == [('#FFFFFF',)]
                key('alt+b')
                window('Projekt bearbeiten')
                key('Tab', 'Tab', 'space')
                window('Farbe auswählen')
                key('Escape')
                window('Projekt bearbeiten')
                key('Escape')
                window('Woodworking Projects')
                assert colors() == [('#FFFFFF',)]
                key('alt+b')
                window('Projekt bearbeiten')
                key('Tab', 'Tab', 'Tab', 'space')
                key('ctrl+s')
                wait(lambda: colors() == [('',)])
                stop()
                start()
                assert colors() == [('',)]
                stop()
                (output / 'result.json').write_text(json.dumps({'picker': 'native selection button and keyboard cancellation', 'color': '#FFFFFF', 'removed': True, 'restarts': 2, 'independent_runtime': True}, indent=2) + '\n')
            except Exception:
                command('import', '-window', 'root', str(output / 'failure.png'))
                raise
            finally:
                if process is not None and process.poll() is None:
                    process.terminate()
                    process.wait(timeout=8)


if __name__ == '__main__':
    main()
