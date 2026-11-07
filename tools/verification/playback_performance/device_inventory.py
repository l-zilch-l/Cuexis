"""Read-only device/content inventory. Does not open audio, play, or register samples."""
import argparse
import ctypes as ct
import datetime as dt
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from tools.verification.playback_performance import prepare_profiles as prepare


def default_audio():
    from ctypes import wintypes as w
    import winreg

    class Guid(ct.Structure):
        _fields_ = [('bytes', ct.c_ubyte * 16)]

    def guid(value):
        return Guid.from_buffer_copy(uuid.UUID(value).bytes_le)

    def method(obj, index, result, *args):
        table = ct.cast(obj, ct.POINTER(ct.POINTER(ct.c_void_p))).contents
        return ct.WINFUNCTYPE(result, ct.c_void_p, *args)(table[index])

    def check(code):
        if code < 0:
            raise OSError(f'Core Audio HRESULT 0x{code & 0xffffffff:08x}')

    ole = ct.WinDLL('ole32')
    ole.CoInitializeEx.argtypes = [ct.c_void_p, w.DWORD]
    ole.CoCreateInstance.argtypes = [ct.POINTER(Guid), ct.c_void_p, w.DWORD,
                                   ct.POINTER(Guid), ct.POINTER(ct.c_void_p)]
    ole.CoTaskMemFree.argtypes = [ct.c_void_p]
    check(ole.CoInitializeEx(None, 0))
    enumerator = ct.c_void_p()
    try:
        cls = guid('bcde0395-e52f-467c-8e3d-c4579291692e')
        iid = guid('a95664d2-9614-4f35-a746-de8db63617e6')
        check(ole.CoCreateInstance(ct.byref(cls), None, 1, ct.byref(iid), ct.byref(enumerator)))
        rows = []
        for role, name in enumerate(('console', 'multimedia', 'communications')):
            device = ct.c_void_p()
            check(method(enumerator, 4, ct.c_long, ct.c_int, ct.c_int,
                         ct.POINTER(ct.c_void_p))(enumerator, 0, role, ct.byref(device)))
            try:
                identifier = ct.c_void_p()
                check(method(device, 5, ct.c_long, ct.POINTER(ct.c_void_p))(device, ct.byref(identifier)))
                try:
                    endpoint_id = ct.wstring_at(identifier)
                finally:
                    ole.CoTaskMemFree(identifier)
                state = w.DWORD()
                check(method(device, 6, ct.c_long, ct.POINTER(w.DWORD))(device, ct.byref(state)))
                key = 'SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render\\' + endpoint_id[-38:] + '\\Properties'
                with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key) as properties:
                    def optional(property_id):
                        try:
                            return winreg.QueryValueEx(properties, property_id)[0]
                        except FileNotFoundError:
                            return None
                    description = optional('{a45c254e-df1c-4efd-8020-67d146a850e0},2')
                    interface = optional('{b3f8fa53-0004-438e-9003-51a46e139bfc},6')
                rows.append({'role': name, 'endpoint_id': endpoint_id, 'state': state.value,
                             'device_description': description, 'interface_name': interface})
            finally:
                method(device, 2, w.ULONG)(device)
        return rows
    finally:
        if enumerator.value:
            method(enumerator, 2, w.ULONG)(enumerator)
        ole.CoUninitialize()


def primary_display():
    from ctypes import wintypes as w

    class Mode(ct.Structure):
        _fields_ = [('device', w.WCHAR * 32), ('spec', w.WORD), ('driver', w.WORD),
            ('size', w.WORD), ('extra', w.WORD), ('fields', w.DWORD), ('union', ct.c_byte * 16),
            ('color', w.SHORT), ('duplex', w.SHORT), ('y_resolution', w.SHORT),
            ('tt_option', w.SHORT), ('collate', w.SHORT), ('form', w.WCHAR * 32),
            ('pixels', w.WORD), ('bits', w.DWORD), ('width', w.DWORD), ('height', w.DWORD),
            ('flags', w.DWORD), ('frequency', w.DWORD), ('tail', w.DWORD * 8)]

    mode = Mode()
    mode.size = ct.sizeof(mode)
    user = ct.WinDLL('user32', use_last_error=True)
    user.EnumDisplaySettingsW.argtypes = [w.LPCWSTR, w.DWORD, ct.POINTER(Mode)]
    if not user.EnumDisplaySettingsW(None, 0xffffffff, ct.byref(mode)):
        raise ct.WinError(ct.get_last_error())
    return {'driver_name': mode.device, 'width': mode.width, 'height': mode.height,
            'refresh_hz': mode.frequency, 'method': 'EnumDisplaySettingsW primary/current'}


def contents(bundle):
    result = {}
    for kind, binding in [('music', 'binding-release.json'), ('dfjk', 'binding-keyboard-release.json')]:
        path = bundle / binding
        data = prepare.load(path)
        for relative, expected in data['files'].items():
            if prepare.sha(bundle / relative) != expected:
                raise ValueError('Previously verified content changed: ' + relative)
        result[kind] = {'binding': str(path), 'binding_sha256': prepare.sha(path),
                        'files': data['files'], 'historical_player_sha256': data['player_sha256']}
    if result['music']['files'].get('music-release/music.cxc') == result['dfjk']['files'].get('keyboard-release/practice.cxc'):
        raise ValueError('Music and DFJK must have distinct courses')
    return result


def inventory(bundle):
    if os.name != 'nt':
        raise RuntimeError('Windows device inventory required')
    command = ['powershell.exe', '-NoProfile', '-Command',
        'Get-CimInstance Win32_VideoController | Select-Object Name,PNPDeviceID,DriverVersion | ConvertTo-Json -Compress']
    gpu = json.loads(subprocess.check_output(command, encoding='utf-8-sig', errors='strict', timeout=30))
    display, audio = primary_display(), default_audio()
    return {'schema': 'pd5.device-inventory.v1', 'created_utc':dt.datetime.now(dt.timezone.utc).isoformat(),
        'collector_sha256':prepare.sha(__file__), 'display': display, 'audio_defaults': audio,
        'gpu': gpu, 'contents': contents(bundle), 'reference_refresh_matches': display['refresh_hz'] == 240,
        'formal_registration': False, 'formal_ready': False,
        'remaining': ['Bind actual SDL backend to the stable endpoint identity in the audio harness',
            'Freeze scripted input and real-player event boundaries, observer and memory domains',
            'Recheck devices/power/foreground immediately before and after each formal process']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, default=Path.home() / 'Desktop/Cuexis-PD3c-DeviceAcceptance-2026-10-19')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare.save_new(args.output, inventory(args.bundle))
    print(args.output)
