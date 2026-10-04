"""Read-only SetupAPI/HID probe; never injects input or sends output reports.

Only opens currently present VID_239A/PID_CAFE interfaces with desired access 0.
The zero report is passed to the local Windows parser, never to the device.
Optional --get-input-report reads one actual current state per expected-serial
collection; this does not capture interrupt input events or physical movement.
"""
import ctypes as C
from ctypes import wintypes as W
import json
import argparse
from pathlib import Path

setup = C.WinDLL('setupapi', use_last_error=True)
hid = C.WinDLL('hid', use_last_error=True)
kernel = C.WinDLL('kernel32', use_last_error=True)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--serial',required=True,help='Explicit expected FluxRail serial')
parser.add_argument('--get-input-report',action='store_true',help='Read one current report per expected-serial collection, not an input event stream')
args=parser.parse_args()

class GUID(C.Structure):
    _fields_ = [('Data1', W.DWORD), ('Data2', W.WORD), ('Data3', W.WORD), ('Data4', C.c_ubyte*8)]

class Interface(C.Structure):
    _fields_ = [('cbSize', W.DWORD), ('Guid', GUID), ('Flags', W.DWORD), ('Reserved', C.c_size_t)]

class Caps(C.Structure):
    _fields_ = [(x, W.WORD) for x in ['Usage','UsagePage','InputReportByteLength','OutputReportByteLength','FeatureReportByteLength']] + [('Reserved',W.WORD*17)] + [(x,W.WORD) for x in ['NumberLinkCollectionNodes','NumberInputButtonCaps','NumberInputValueCaps','NumberInputDataIndices','NumberOutputButtonCaps','NumberOutputValueCaps','NumberOutputDataIndices','NumberFeatureButtonCaps','NumberFeatureValueCaps','NumberFeatureDataIndices']]

setup.SetupDiGetClassDevsW.argtypes=[C.POINTER(GUID),W.LPCWSTR,W.HWND,W.DWORD]
setup.SetupDiGetClassDevsW.restype=W.HANDLE
setup.SetupDiEnumDeviceInterfaces.argtypes=[W.HANDLE,C.c_void_p,C.POINTER(GUID),W.DWORD,C.POINTER(Interface)]
setup.SetupDiEnumDeviceInterfaces.restype=W.BOOL
setup.SetupDiGetDeviceInterfaceDetailW.argtypes=[W.HANDLE,C.POINTER(Interface),C.c_void_p,W.DWORD,C.POINTER(W.DWORD),C.c_void_p]
setup.SetupDiGetDeviceInterfaceDetailW.restype=W.BOOL
setup.SetupDiDestroyDeviceInfoList.argtypes=[W.HANDLE]
kernel.CreateFileW.argtypes=[W.LPCWSTR,W.DWORD,W.DWORD,C.c_void_p,W.DWORD,W.DWORD,W.HANDLE]
kernel.CreateFileW.restype=W.HANDLE
kernel.CloseHandle.argtypes=[W.HANDLE]
hid.HidD_GetHidGuid.argtypes=[C.POINTER(GUID)]
hid.HidD_GetPreparsedData.argtypes=[W.HANDLE,C.POINTER(C.c_void_p)]
hid.HidD_GetPreparsedData.restype=W.BOOL
hid.HidD_FreePreparsedData.argtypes=[C.c_void_p]
hid.HidP_GetCaps.argtypes=[C.c_void_p,C.POINTER(Caps)]
hid.HidP_GetCaps.restype=C.c_int32
hid.HidP_GetUsageValue.argtypes=[C.c_int,W.WORD,W.WORD,W.WORD,C.POINTER(W.ULONG),C.c_void_p,C.c_void_p,W.ULONG]
hid.HidP_GetUsageValue.restype=C.c_int32
hid.HidD_GetInputReport.argtypes=[W.HANDLE,C.c_void_p,W.ULONG]
hid.HidD_GetInputReport.restype=C.c_ubyte
for name in ['HidD_GetProductString','HidD_GetSerialNumberString','HidD_GetManufacturerString']:
    fn=getattr(hid,name);fn.argtypes=[W.HANDLE,C.c_void_p,W.ULONG];fn.restype=W.BOOL

guid=GUID();hid.HidD_GetHidGuid(C.byref(guid))
devices=setup.SetupDiGetClassDevsW(C.byref(guid),None,None,0x12)
invalid=C.c_void_p(-1).value
if devices==invalid: raise C.WinError(C.get_last_error())
results=[]
try:
    index=0
    while True:
        entry=Interface();entry.cbSize=C.sizeof(entry)
        if not setup.SetupDiEnumDeviceInterfaces(devices,None,C.byref(guid),index,C.byref(entry)):
            if C.get_last_error()!=259: raise C.WinError(C.get_last_error())
            break
        index+=1;required=W.DWORD()
        setup.SetupDiGetDeviceInterfaceDetailW(devices,C.byref(entry),None,0,C.byref(required),None)
        detail=C.create_string_buffer(required.value);C.cast(detail,C.POINTER(W.DWORD))[0]=8 if C.sizeof(C.c_void_p)==8 else 6
        if not setup.SetupDiGetDeviceInterfaceDetailW(devices,C.byref(entry),detail,required,None,None): raise C.WinError(C.get_last_error())
        device_path=C.wstring_at(C.addressof(detail)+4)
        if 'vid_239a&pid_cafe' not in device_path.lower(): continue
        r={'path':device_path};results.append(r)
        handle=kernel.CreateFileW(device_path,0,3,None,3,0,None)
        if handle==invalid:
            r['open_error']=C.get_last_error();r['open_error_message']=C.FormatError(r['open_error']);continue
        pp=C.c_void_p()
        try:
            for name,api in [('product','HidD_GetProductString'),('serial','HidD_GetSerialNumberString'),('manufacturer','HidD_GetManufacturerString')]:
                text=C.create_unicode_buffer(256)
                if getattr(hid,api)(handle,text,C.sizeof(text)):r[name]=text.value
            if not hid.HidD_GetPreparsedData(handle,C.byref(pp)):
                r['preparsed_error']=C.get_last_error();continue
            caps=Caps();status=hid.HidP_GetCaps(pp,C.byref(caps))&0xffffffff
            r['caps_status']=f'0x{status:08X}'
            if status!=0x00110000: continue
            r['caps']={name:getattr(caps,name) for name,_ in Caps._fields_ if name!='Reserved'}
            data=C.create_string_buffer(caps.InputReportByteLength)
            if 'col01' in device_path.lower():data[0]=b'\x01'
            if 'col02' in device_path.lower():data[0]=b'\x02'
            r['local_parser_zero_report']={}
            for name,usage in [('X',0x30),('Y',0x31)]:
                value=W.ULONG();status=hid.HidP_GetUsageValue(0,1,0,usage,C.byref(value),pp,data,caps.InputReportByteLength)&0xffffffff
                r['local_parser_zero_report'][name]={'status':f'0x{status:08X}','value':value.value}
            if args.get_input_report and r.get('serial')==args.serial:
                actual=C.create_string_buffer(caps.InputReportByteLength)
                actual[0]=data[0]
                success=bool(hid.HidD_GetInputReport(handle,actual,caps.InputReportByteLength))
                error=C.get_last_error() if not success else None
                report={'passed':success,'length':caps.InputReportByteLength,'hex':actual.raw.hex() if success else None,
                        'buffer_is_device_report':success,
                        'method':'HidD_GetInputReport single control state read; not motion/event capture'}
                if not success:
                    report['prepared_buffer_hex']=actual.raw.hex()
                    report['error']=error;report['error_message']=C.FormatError(error)
                elif caps.Usage==2:
                    report['parsed']={}
                    for name,usage in [('X',0x30),('Y',0x31)]:
                        value=W.ULONG();status=hid.HidP_GetUsageValue(0,1,0,usage,C.byref(value),pp,actual,caps.InputReportByteLength)&0xffffffff
                        report['parsed'][name]={'status':f'0x{status:08X}','value':value.value}
                r['control_get_input_report']=report
        finally:
            if pp:hid.HidD_FreePreparsedData(pp)
            kernel.CloseHandle(handle)
finally:setup.SetupDiDestroyDeviceInfoList(devices)
out=args.output
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(results,ensure_ascii=False,indent=2))
target=[r for r in results if r.get('serial')==args.serial]
assert any(r.get('caps',{}).get('Usage')==6 for r in target), 'Expected serial keyboard missing'
for collection in ['col01','col02']:
    matches=[r for r in target if collection in r['path'].lower()]
    assert len(matches)==1, f'Expected serial {collection} must be present exactly once'
    item=matches[0]
    assert item['caps_status']=='0x00110000' and item['caps']['InputReportByteLength']==6
    assert all(item['local_parser_zero_report'][axis]['status']=='0x00110000' for axis in ['X','Y']), f'{collection} X/Y parse failed'
if args.get_input_report:
    assert all(r.get('control_get_input_report',{}).get('passed') for r in target), 'Expected serial current input report read failed'
    for item in target:
        if item['caps']['Usage']!=2: continue
        actual=item['control_get_input_report']
        expected_id=2 if 'col02' in item['path'].lower() else 1
        assert bytes.fromhex(actual['hex'])[0]==expected_id, 'Actual current report ID mismatch'
        assert all(actual['parsed'][axis]['status']=='0x00110000' for axis in ['X','Y']), 'Actual current report X/Y parse failed'
        if expected_id==2:
            assert all(0<=actual['parsed'][axis]['value']<=32767 for axis in ['X','Y']), 'Absolute report coordinate out of range'
