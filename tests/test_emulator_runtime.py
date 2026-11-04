#!/usr/bin/env python3
"""Reject stale UI hierarchies when uiautomator reports success without output."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('emulator_runtime', ROOT / 'android/scripts/emulator_runtime.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory() as folder:
    output = Path(folder)
    for writes_snapshot in (False, True):
        remote = {'xml': '<hierarchy><node content-desc="Old screen"/></hierarchy>'}

        def adb(*args):
            if args[:3] == ('shell', 'rm', '-f'):
                remote.clear()
            elif args[:3] == ('shell', 'uiautomator', 'dump'):
                if writes_snapshot:
                    remote['xml'] = '<hierarchy><node content-desc="Current screen"/></hierarchy>'
                return 'UI capture command returned zero'
            elif args[:2] == ('exec-out', 'cat'):
                if 'xml' not in remote:
                    raise subprocess.CalledProcessError(1, args)
                return remote['xml']
            else:
                raise AssertionError(args)

        if writes_snapshot:
            tree = module.capture_tree(adb, output, 'current')
            assert tree.find('node').get('content-desc') == 'Current screen'
            assert (output / 'current.xml').exists()
            print('PASS capture fresh hierarchy')
        else:
            try:
                module.capture_tree(adb, output, 'stale')
            except subprocess.CalledProcessError:
                pass
            else:
                raise AssertionError('stale hierarchy was accepted')
            assert not (output / 'stale.xml').exists()
            print('PASS reject stale hierarchy after silent dump failure')

# Android can mmap uncompressed .so entries directly from base.apk, so /proc/maps
# then contains APK paths/offsets rather than individual library filenames.
import struct
import zipfile

with tempfile.TemporaryDirectory() as folder:
    apk = Path(folder) / 'test.apk'
    installed = '/data/app/test/org.sudokura.prototype.debug-test/base.apk'
    with zipfile.ZipFile(apk, 'w', compression=zipfile.ZIP_STORED) as archive:
        for library in module.LIBRARIES:
            archive.writestr('lib/x86_64/' + library, b'fixture bytes' * 20)
    rows = []
    with zipfile.ZipFile(apk) as archive, apk.open('rb') as raw:
        for entry in archive.infolist():
            raw.seek(entry.header_offset)
            name_size, extra_size = struct.unpack_from('<HH', raw.read(30), 26)
            offset = entry.header_offset + 30 + name_size + extra_size
            rows.append(f'1000-2000 r-xp {offset:08x} 00:00 1 {installed}')
    actual = module.loaded_libraries(apk, '\n'.join(rows), installed)
    assert set(actual) == set(module.LIBRARIES) and set(actual.values()) == {'apk-mmap'}
    print('PASS direct APK executable mappings bound to library ZIP offsets')
    extracted = '\n'.join(f'1000-2000 r-xp 00000000 00:00 1 '
                          f'{installed.rsplit("/",1)[0]}/lib/x86_64/{name}'
                          for name in module.LIBRARIES)
    assert set(module.loaded_libraries(apk, extracted, installed).values()) == {'extracted'}
    print('PASS extracted native library mappings')
    for maps in ('\n'.join(rows[1:]), '\n'.join(rows).replace('r-xp', 'r--p'),
                 '\n'.join(rows).replace(installed, '/data/app/another/base.apk')):
        try:
            module.loaded_libraries(apk, maps, installed)
        except RuntimeError:
            pass
        else:
            raise AssertionError('missing/readonly/wrong-package executable mappings accepted')
    print('PASS reject missing, non-executable and wrong-package mappings')
