#!/usr/bin/env python3
"""Isolated x86_64 16KiB CI smoke; never addresses a physical phone or production ID."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import time
import xml.etree.ElementTree as ET

PACKAGE = 'org.sudokura.prototype.debug'
ACTIVITY = PACKAGE + '/org.sudokura.prototype.PrototypeActivity'
LIBRARIES = ('libmain.so', 'libSDL2.so', 'libSDL2_ttf.so', 'libSDL2_mixer.so')


def run(args):
    if not re.fullmatch(r'emulator-[0-9]+', args.serial):
        raise ValueError('Only a disposable Android emulator is supported')
    if not re.fullmatch(r'[0-9a-f]{40}', args.source):
        raise ValueError('Invalid source identity')
    output = args.output
    output.mkdir(parents=True, exist_ok=True)

    def adb(*command, binary=False, check=True):
        return subprocess.run([args.adb, '-s', args.serial, *command],
                              check=check, capture_output=True,
                              text=not binary, errors=None if binary else 'replace', timeout=45).stdout

    def shell(*command):
        return adb('shell', *command).strip()

    def wait_ready():
        deadline = time.monotonic() + 180
        while time.monotonic() < deadline:
            if shell('getprop', 'sys.boot_completed') == '1':
                return
            time.sleep(2)
        raise RuntimeError('Emulator boot timeout')

    def dump(name):
        adb('shell', 'uiautomator', 'dump', '/sdcard/sudokura-ui.xml')
        data = adb('exec-out', 'cat', '/sdcard/sudokura-ui.xml')
        (output / (name + '.xml')).write_text(data)
        return ET.fromstring(data)

    def find(tree, text):
        return next((node for node in tree.iter('node')
                     if node.get('content-desc') == text or node.get('text') == text), None)

    def tap(node):
        if node is None:
            raise RuntimeError('Required visible action missing')
        values = re.fullmatch(r'\[(\d+),(\d+)\]\[(\d+),(\d+)\]', node.get('bounds', ''))
        if not values or node.get('enabled') != 'true':
            raise RuntimeError('Action has invalid/disabled bounds')
        x1, y1, x2, y2 = map(int, values.groups())
        shell('input', 'tap', str((x1+x2)//2), str((y1+y2)//2))

    def wait_scene(name, predicate):
        deadline = time.monotonic() + 90
        previous_layout = None
        while time.monotonic() < deadline:
            tree = dump(name)
            # Initial Android insets and surface geometry can settle after
            # the first semantic publication. Do not tap stale pixel bounds.
            layout = [(n.get('class'), n.get('bounds'), n.get('enabled'))
                      for n in tree.iter('node')]
            if predicate(tree):
                if layout == previous_layout:
                    (output / (name + '.png')).write_bytes(adb('exec-out', 'screencap', '-p', binary=True))
                    return tree
                previous_layout = layout
            else:
                previous_layout = None
            time.sleep(2)
        (output / (name + '-failure.png')).write_bytes(adb('exec-out', 'screencap', '-p', binary=True))
        raise RuntimeError('Native UI did not reach stable ' + name)

    def cells(tree):
        return [n for n in tree.iter('node')
                if re.match(r'^Row [1-9], column [1-9](?:\b|,)', n.get('content-desc', ''))]

    wait_ready()
    if shell('getprop', 'ro.kernel.qemu') != '1':
        raise RuntimeError('Not an emulator')
    page_size = shell('getconf', 'PAGE_SIZE')
    if page_size != '16384':
        raise RuntimeError('Expected actual 16KiB environment, got ' + page_size)
    abi = shell('getprop', 'ro.product.cpu.abi')
    if abi != 'x86_64':
        raise RuntimeError('Unexpected test ABI: ' + abi)
    shell('setprop', 'bionic.linker.16kb.app_compat.enabled', 'false')
    shell('setprop', 'pm.16kb.app_compat.disabled', 'true')
    compat = {key: shell('getprop', key) for key in
              ('bionic.linker.16kb.app_compat.enabled', 'pm.16kb.app_compat.disabled')}
    if list(compat.values()) != ['false', 'true']:
        raise RuntimeError('Cannot verify disabled page-size compatibility fallback')
    # Complete only this disposable emulator's first-boot setup.
    shell('settings', 'put', 'global', 'device_provisioned', '1')
    shell('settings', 'put', 'secure', 'user_setup_complete', '1')
    shell('input', 'keyevent', 'KEYCODE_WAKEUP')
    shell('wm', 'dismiss-keyguard')
    (output / 'memory-before.txt').write_text(shell('cat', '/proc/meminfo'))
    adb('install', '-r', str(args.apk))
    adb('logcat', '-c')
    shell('am', 'start', '-W', '-n', ACTIVITY)
    home = wait_scene('home', lambda t: find(t, 'Start new game') is not None)
    tap(find(home, 'Start new game'))
    wait_scene('play', lambda t: len(cells(t)) == 81)
    (output / 'memory-playing.txt').write_text(shell('dumpsys', 'meminfo', PACKAGE))
    pid = shell('pidof', PACKAGE)
    if not re.fullmatch(r'[0-9]+', pid):
        raise RuntimeError('Expected exactly one native game process')
    maps = shell('run-as', PACKAGE, 'cat', '/proc/' + pid + '/maps')
    (output / 'native-maps.txt').write_text(maps)
    if not all(library in maps for library in LIBRARIES):
        raise RuntimeError('Not all packaged native libraries loaded')
    shell('input', 'keyevent', 'KEYCODE_HOME')
    time.sleep(2)
    shell('am', 'start', '-W', '-n', ACTIVITY)
    paused = wait_scene('paused-return', lambda t: find(t, 'Resume') is not None)
    if cells(paused):
        raise RuntimeError('Paused board exposes cells')
    tap(find(paused, 'Resume'))
    wait_scene('resumed', lambda t: len(cells(t)) == 81)
    shell('settings', 'put', 'system', 'accelerometer_rotation', '0')
    shell('settings', 'put', 'system', 'user_rotation', '1')
    time.sleep(3)
    wait_scene('landscape', lambda t: len(cells(t)) == 81)
    width, height = struct.unpack('>II', (output / 'landscape.png').read_bytes()[16:24])
    if width <= height:
        raise RuntimeError('Emulator did not actually rotate to landscape')
    logs = adb('logcat', '-d', '--pid=' + pid)
    (output / 'logcat.txt').write_text(logs)
    if re.search(r'FATAL EXCEPTION|Fatal signal|UnsatisfiedLinkError|dlopen failed', logs):
        raise RuntimeError('Runtime crash/linker failure in logcat')
    receipt = {'source_commit': args.source, 'apk_sha256': hashlib.sha256(args.apk.read_bytes()).hexdigest(),
               'abi': abi, 'page_size': int(page_size), 'compatibility_properties': compat,
               'native_libraries_loaded': list(LIBRARIES), 'memory_receipts': ['memory-before.txt', 'memory-playing.txt'], 'home_play_resume': 'PASS',
               'rotation_smoke': 'PASS', 'production_arm64_acceptance': 'NOT_ESTABLISHED',
               'talkback_acceptance': 'NOT_ESTABLISHED'}
    (output / 'runtime.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--adb', required=True)
    parser.add_argument('--serial', default='emulator-5554')
    parser.add_argument('--apk', type=Path, required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--output', type=Path, required=True)
    run(parser.parse_args())
