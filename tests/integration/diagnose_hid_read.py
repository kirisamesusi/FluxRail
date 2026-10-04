"""Finite read-only Windows HID current-state diagnostics, never input injection.

Uses previously observed paths but revalidates serial and caps on every handle.
Success at collecting diagnostics does not mean HID reads passed.
IOCTL definition: upstream libusb/windows_winusb.h HID_OUT_CTL_CODE(104).
Buffer conventions: Microsoft IOCTL_HID_GET_INPUT_REPORT and upstream HIDAPI.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import time


class Caps(C.Structure):
    _fields_ = [(x, W.WORD) for x in ['Usage', 'UsagePage', 'InputReportByteLength',
        'OutputReportByteLength', 'FeatureReportByteLength']] + [('Reserved', W.WORD * 17)] + [
        (x, W.WORD) for x in ['NumberLinkCollectionNodes', 'NumberInputButtonCaps',
        'NumberInputValueCaps', 'NumberInputDataIndices', 'NumberOutputButtonCaps',
        'NumberOutputValueCaps', 'NumberOutputDataIndices', 'NumberFeatureButtonCaps',
        'NumberFeatureValueCaps', 'NumberFeatureDataIndices']]


class StatusUnion(C.Union):
    _fields_ = [('Status', C.c_int32), ('Pointer', C.c_void_p)]


class IoStatus(C.Structure):
    _anonymous_ = ('u',)
    _fields_ = [('u', StatusUnion), ('Information', C.c_size_t)]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--observed-paths', type=Path, required=True)
    ap.add_argument('--serial', required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--query', choices=['metadata', 'hidd', 'ioctl', 'native'], default='metadata',
                    help='At most one control query, only on observed COL02')
    ap.add_argument('--query-access', choices=['metadata', 'read'], default='read')
    args = ap.parse_args()
    if args.output.exists():
        raise RuntimeError('Refusing to overwrite evidence')
    paths = [r for r in json.loads(args.observed_paths.read_text(encoding='utf-8'))
             if r.get('serial') == args.serial]
    assert len(paths) == 3 and {r['caps']['Usage'] for r in paths} == {2, 6}
    assert all('vid_239a&pid_cafe' in r['path'].lower() for r in paths)
    kernel = C.WinDLL('kernel32', use_last_error=True)
    hid = C.WinDLL('hid', use_last_error=True)
    nt = C.WinDLL('ntdll')
    kernel.CreateFileW.argtypes = [W.LPCWSTR, W.DWORD, W.DWORD, C.c_void_p,
                                  W.DWORD, W.DWORD, W.HANDLE]
    kernel.CreateFileW.restype = W.HANDLE
    kernel.CloseHandle.argtypes = [W.HANDLE]
    kernel.CloseHandle.restype = W.BOOL
    kernel.DeviceIoControl.argtypes = [W.HANDLE, W.DWORD, C.c_void_p, W.DWORD,
        C.c_void_p, W.DWORD, C.POINTER(W.DWORD), C.c_void_p]
    kernel.DeviceIoControl.restype = W.BOOL
    for name in ['HidD_GetSerialNumberString', 'HidD_GetProductString']:
        getattr(hid, name).argtypes = [W.HANDLE, C.c_void_p, W.ULONG]
        getattr(hid, name).restype = C.c_ubyte
    hid.HidD_GetPreparsedData.argtypes = [W.HANDLE, C.POINTER(C.c_void_p)]
    hid.HidD_GetPreparsedData.restype = C.c_ubyte
    hid.HidD_FreePreparsedData.argtypes = [C.c_void_p]
    hid.HidP_GetCaps.argtypes = [C.c_void_p, C.POINTER(Caps)]
    hid.HidP_GetCaps.restype = C.c_int32
    hid.HidD_GetInputReport.argtypes = [W.HANDLE, C.c_void_p, W.ULONG]
    hid.HidD_GetInputReport.restype = C.c_ubyte
    nt.NtDeviceIoControlFile.argtypes = [W.HANDLE, W.HANDLE, C.c_void_p,
        C.c_void_p, C.POINTER(IoStatus), W.ULONG, C.c_void_p, W.ULONG,
        C.c_void_p, W.ULONG]
    nt.NtDeviceIoControlFile.restype = C.c_int32
    nt.RtlNtStatusToDosError.argtypes = [C.c_int32]
    nt.RtlNtStatusToDosError.restype = W.ULONG
    invalid = C.c_void_p(-1).value
    ioctl = (0xB << 16) | (104 << 2) | 2
    assert ioctl == 0xB01A2 and C.sizeof(Caps) == 64
    assert C.sizeof(IoStatus) == 2 * C.sizeof(C.c_void_p)
    result = {'started': datetime.now(timezone.utc).isoformat(),
        'serial': args.serial, 'ioctl': f'0x{ioctl:08X}',
        'source_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'observed_paths_sha256': hashlib.sha256(args.observed_paths.read_bytes()).hexdigest(),
        'scope': 'finite current-state controls; no ReadFile stream/output/config writes',
        'query': args.query, 'query_access': args.query_access,
        'attempts': []}
    try:
        for observed in paths:
            variants = [('observed', observed['path'])]
            if observed['path'].lower().endswith('\\kbd'):
                variants.append(('without_kbd_suffix', observed['path'][:-4]))
            for variant, path in variants:
                for access in [0, 0x80000000]:
                    row = {'path': path, 'variant': variant,
                           'desired_access': f'0x{access:08X}', 'calls': []}
                    result['attempts'].append(row)
                    handle = kernel.CreateFileW(path, access, 3, None, 3, 0, None)
                    if handle == invalid:
                        error = C.get_last_error()
                        row.update(opened=False, open_error=error,
                                   open_error_message=C.FormatError(error))
                        continue
                    row['opened'] = True
                    pp = C.c_void_p()
                    try:
                        serial = C.create_unicode_buffer(126)
                        if not hid.HidD_GetSerialNumberString(handle, serial, C.sizeof(serial)):
                            row['identity_error'] = C.get_last_error()
                            continue
                        row['serial'] = serial.value
                        assert serial.value == args.serial, 'Identity mismatch; stopping'
                        assert hid.HidD_GetPreparsedData(handle, C.byref(pp))
                        caps = Caps()
                        status = hid.HidP_GetCaps(pp, C.byref(caps)) & 0xffffffff
                        row['caps_status'] = f'0x{status:08X}'
                        assert status == 0x00110000
                        row['caps'] = {name: getattr(caps, name) for name, _ in Caps._fields_
                                       if name != 'Reserved'}
                        assert all(getattr(caps, k) == observed['caps'][k]
                                   for k in ['Usage', 'UsagePage', 'InputReportByteLength'])
                        report_id = 0 if caps.Usage == 6 else (2 if 'col02' in path.lower() else 1)
                        methods = {'metadata': [], 'hidd': ['HidD_GetInputReport'],
                            'ioctl': ['DeviceIoControl_same_buffer'],
                            'native': ['NtDeviceIoControlFile_same_buffer']}[args.query]
                        query_access = 0 if args.query_access == 'metadata' else 0x80000000
                        if variant != 'observed' or 'col02' not in path.lower() or access != query_access:
                            methods = []
                        for method in methods:
                            length = caps.InputReportByteLength
                            data = C.create_string_buffer(length)
                            data[0] = bytes([report_id])
                            prepared = data.raw.hex()
                            call = {'method': method, 'length': length, 'report_id': report_id}
                            start = time.perf_counter()
                            C.set_last_error(0)
                            if method == 'HidD_GetInputReport':
                                ok = bool(hid.HidD_GetInputReport(handle, data, length))
                                error = None if ok else C.get_last_error()
                            elif method == 'DeviceIoControl_same_buffer':
                                count = W.DWORD()
                                ok = bool(kernel.DeviceIoControl(handle, ioctl, data, length,
                                          data, length, C.byref(count), None))
                                error = None if ok else C.get_last_error()
                                call['bytes_returned'] = count.value
                            else:
                                ios = IoStatus()
                                status = nt.NtDeviceIoControlFile(handle, None, None, None,
                                    C.byref(ios), ioctl, data, length, data, length)
                                call['ntstatus'] = f'0x{status & 0xffffffff:08X}'
                                call['iosb_status'] = f'0x{ios.Status & 0xffffffff:08X}'
                                call['bytes_returned'] = ios.Information
                                assert status != 0x103, 'Unexpected pending on synchronous handle'
                                ok = status >= 0
                                error = None if ok else nt.RtlNtStatusToDosError(status)
                            call.update(elapsed_ms=(time.perf_counter() - start) * 1000,
                                passed=ok, buffer_is_device_report=ok,
                                hex=data.raw.hex() if ok else None)
                            if not ok:
                                call.update(error=error, error_message=C.FormatError(error),
                                            prepared_buffer_hex=prepared,
                                            failed_buffer_hex=data.raw.hex())
                            row['calls'].append(call)
                    finally:
                        if pp:
                            hid.HidD_FreePreparsedData(pp)
                        row['handle_closed'] = bool(kernel.CloseHandle(handle))
    finally:
        result['finished'] = datetime.now(timezone.utc).isoformat()
        result['hid_current_reads_passed'] = sum(c['passed'] for r in result['attempts']
                                               for c in r['calls'])
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps([{'variant': r['variant'], 'usage': r.get('caps', {}).get('Usage'),
        'access': r['desired_access'], 'open_error': r.get('open_error'),
        'calls': [{k: c.get(k) for k in ['method', 'passed', 'error', 'ntstatus', 'hex']}
                  for c in r['calls']]} for r in result['attempts']], ensure_ascii=False))


if __name__ == '__main__':
    main()
