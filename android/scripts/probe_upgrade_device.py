#!/usr/bin/env python3
"""Read-only eligibility check for an explicitly selected disposable Android emulator."""
import argparse
import json
import subprocess

PACKAGE = 'org.cajapersonal.sudokura'


def eligible(abi_list, sdk_text, is_emulator, installed):
    try:
        api = int(sdk_text)
    except (TypeError, ValueError):
        return False
    return (is_emulator == '1' and api >= 28 and
            'arm64-v8a' in abi_list.split(',') and not installed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True, help='Explicit disposable emulator serial')
    parser.add_argument('--adb', default='adb')
    args = parser.parse_args()
    if not args.serial.strip() or args.serial.startswith('-'):
        parser.error('A concrete emulator serial is required')

    def query(*commands):
        result = subprocess.run([args.adb, '-s', args.serial, 'shell', *commands],
                                capture_output=True, text=True, timeout=15, check=True)
        return result.stdout.strip()

    abi_list = query('getprop', 'ro.product.cpu.abilist')
    sdk = query('getprop', 'ro.build.version.sdk')
    emulator = query('getprop', 'ro.kernel.qemu')
    existing_package = query('pm', 'path', PACKAGE)
    status = eligible(abi_list, sdk, emulator, bool(existing_package))
    print(json.dumps({'eligible': status, 'is_emulator': emulator == '1',
                      'arm64_supported': 'arm64-v8a' in abi_list.split(','),
                      'sdk': sdk, 'package_already_installed': bool(existing_package),
                      'checks_only': True, 'installed_or_modified': False}, sort_keys=True))
    if not status:
        raise SystemExit('Not eligible: use a fresh disposable ARM64 Android emulator API >=28')


if __name__ == '__main__':
    main()
