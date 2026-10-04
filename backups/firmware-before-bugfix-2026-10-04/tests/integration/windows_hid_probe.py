"""Read-only SetupAPI/HID metadata probe; never sends input/output reports.

Only opens currently present VID_239A/PID_CAFE interfaces with desired access 0.
The zero report is passed to the local Windows parser, never to the device.
"""
import ctypes as C
from ctypes import wintypes as W
import json
from pathlib import Path

setup = C.WinDLL('setupapi', use_last_error=True)
hid = C.WinDLL('hid', use_last_error=True)
kernel = C.WinDLL('kernel32', use_last_error=True)

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
        finally:
            if pp:hid.HidD_FreePreparsedData(pp)
            kernel.CloseHandle(handle)
finally:setup.SetupDiDestroyDeviceInfoList(devices)
out=Path(__file__).parent/'hardware_2026-10-04'/'windows-hid-probe.json'
out.write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(results,ensure_ascii=False,indent=2))
