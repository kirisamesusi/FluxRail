"""Real IO4 host protocol test. Metadata identity first, bounded input reads, RAM protocol commands; no Flash/config or injected physical input."""
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

assert not args.output.exists(), 'Refusing to overwrite evidence'
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
        if 'vid_0ca3&pid_0021' not in device_path.lower(): continue
        r={'path':device_path};results.append(r)
        handle=kernel.CreateFileW(device_path,0,3,None,3,0,None)
        if handle==invalid:r['open_error']=C.get_last_error();continue
        pp=C.c_void_p()
        try:
            for name,api in [('product','HidD_GetProductString'),('serial','HidD_GetSerialNumberString'),('manufacturer','HidD_GetManufacturerString')]:
                value=C.create_unicode_buffer(256)
                if getattr(hid,api)(handle,value,C.sizeof(value)):r[name]=value.value
            if r.get('serial')!=args.serial:continue
            if not hid.HidD_GetPreparsedData(handle,C.byref(pp)):r['preparsed_error']=C.get_last_error();continue
            caps=Caps();status=hid.HidP_GetCaps(pp,C.byref(caps))&0xffffffff
            r['caps_status']=f'0x{status:08X}';r['caps']={name:getattr(caps,name) for name,_ in Caps._fields_ if name!='Reserved'}
            if status!=0x00110000 or caps.InputReportByteLength!=64 or caps.UsagePage!=1 or caps.Usage!=4:raise AssertionError('Unexpected IO4 report descriptor')
        finally:
            if pp:hid.HidD_FreePreparsedData(pp)
            kernel.CloseHandle(handle)
finally:setup.SetupDiDestroyDeviceInfoList(devices)
import time
from datetime import datetime,timezone
report={'started':datetime.now(timezone.utc).isoformat(),'metadata':results,'steps':[],'flash_writes':0,'configuration_writes':0,'game':'not_run','error':None}
def put():
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
target=[r for r in results if r.get('serial')==args.serial]
assert len(target)==1 and target[0].get('caps_status')=='0x00110000'
r=target[0];assert r['caps']['InputReportByteLength']==r['caps']['OutputReportByteLength']==64
class Overlapped(C.Structure):
    _fields_=[('Internal',C.c_size_t),('InternalHigh',C.c_size_t),('Offset',W.DWORD),('OffsetHigh',W.DWORD),('hEvent',W.HANDLE)]
kernel.CreateEventW.argtypes=[C.c_void_p,W.BOOL,W.BOOL,W.LPCWSTR];kernel.CreateEventW.restype=W.HANDLE
kernel.WaitForSingleObject.argtypes=[W.HANDLE,W.DWORD];kernel.WaitForSingleObject.restype=W.DWORD
kernel.GetOverlappedResult.argtypes=[W.HANDLE,C.POINTER(Overlapped),C.POINTER(W.DWORD),W.BOOL];kernel.GetOverlappedResult.restype=W.BOOL
kernel.CancelIoEx.argtypes=[W.HANDLE,C.POINTER(Overlapped)];kernel.CancelIoEx.restype=W.BOOL
for n in ('ReadFile','WriteFile'):
    getattr(kernel,n).argtypes=[W.HANDLE,C.c_void_p,W.DWORD,C.POINTER(W.DWORD),C.POINTER(Overlapped)];getattr(kernel,n).restype=W.BOOL
hid.HidD_SetOutputReport.argtypes=[W.HANDLE,C.c_void_p,W.ULONG];hid.HidD_SetOutputReport.restype=C.c_ubyte
def decoded(data):
    return {'report_id':data[0],'analog_words':[int.from_bytes(data[i:i+2],'little') for i in range(1,29,2)],'switch_words':[int.from_bytes(data[i:i+2],'little') for i in (29,31)],'system':data[33],'usb':data[34],'reserved_hex':data[35:].hex()}
def step(name,**values):
    report['steps'].append({'name':name,'time':datetime.now(timezone.utc).isoformat(),**values});put()
ctrl=rw=invalid;mutated=False;pending_keepalive=[]
def io(method,data=None):
    buf=C.create_string_buffer(data,64) if data is not None else C.create_string_buffer(64)
    ov=Overlapped();ov.hEvent=kernel.CreateEventW(None,True,False,None);assert ov.hEvent
    count=W.DWORD();ret=False
    try:
        C.set_last_error(0);ok=bool(getattr(kernel,method)(rw,buf,64,C.byref(count),C.byref(ov)));err=C.get_last_error()
        if not ok:
            assert err==997, f'{method} error {err}'
            if kernel.WaitForSingleObject(ov.hEvent,1500)!=0:
                kernel.CancelIoEx(rw,C.byref(ov))
                if kernel.WaitForSingleObject(ov.hEvent,1500)!=0:
                    pending_keepalive.append((buf,ov));ret=True
                raise TimeoutError(method+' did not complete; cancel requested')
            assert kernel.GetOverlappedResult(rw,C.byref(ov),C.byref(count),False), C.get_last_error()
        assert count.value==64, f'{method} short report {count.value}'
        return buf.raw
    finally:
        if not ret:kernel.CloseHandle(ov.hEvent)
def get_input(name):
    b=C.create_string_buffer(64);b[0]=b'\x01';C.set_last_error(0)
    ok=bool(hid.HidD_GetInputReport(ctrl,b,64));step(name,method='HidD_GetInputReport',passed=ok,hex=b.raw.hex() if ok else None,error=None if ok else C.get_last_error())
    assert ok,'Control input read failed';v=decoded(b.raw);assert v['report_id']==1 and v['reserved_hex']=='00'*29;return v
def output(cmd,param=0,method='control'):
    global mutated
    data=bytes([0x10,cmd,param])+bytes(61);mutated=True
    step('output_attempt',method=method,command=cmd,param=param,hex=data.hex())
    if method=='control':
        b=C.create_string_buffer(data,64);C.set_last_error(0);ok=bool(hid.HidD_SetOutputReport(ctrl,b,64));step('output_completed',method=method,passed=ok,error=None if ok else C.get_last_error());assert ok,'SetOutputReport failed'
    else:io('WriteFile',data);step('output_completed',method='WriteFile',passed=True)
try:
    ctrl=kernel.CreateFileW(r['path'],0,3,None,3,0,None);assert ctrl!=invalid
    serial=C.create_unicode_buffer(256);assert hid.HidD_GetSerialNumberString(ctrl,serial,C.sizeof(serial)) and serial.value==args.serial
    assert get_input('initial_control')['system']==2
    rw=kernel.CreateFileW(r['path'],0xC0000000,3,None,3,0x40000000,None);assert rw!=invalid, f'IO4 read/write open failed {C.get_last_error()}'
    assert hid.HidD_GetSerialNumberString(rw,serial,C.sizeof(serial)) and serial.value==args.serial
    for i in range(8):
        data=io('ReadFile');v=decoded(data);step('interrupt_input',method='ReadFile',index=i,hex=data.hex(),decoded=v)
        assert v['report_id']==1 and v['switch_words']==[0x4000,0x8000] and v['system']==2 and v['reserved_hex']=='00'*29
    output(1,4);assert get_input('cmd1_state')['system']==0x30
    output(3,method='interrupt');assert get_input('cmd3_interrupt_state')['system']==0
    output(2,32);assert get_input('cmd2_state')['system']==0x30
    output(3);assert get_input('cmd3_control_state')['system']==0
    time.sleep(.45);assert get_input('timeout_400ms_state')['system']==2
    output(4);assert get_input('zero_gpio_state')['system']==2
    # Put the system in initialized state so accepting unknown cmd as timeout/init is observable.
    output(3);assert get_input('before_unknown_state')['system']==0
    output(0x85);assert get_input('unknown_0x85_state')['system']==0
    report['interrupt_input_passed']=True;report['control_cmd1_cmd2_passed']=True;report['interrupt_cmd3_passed']=True;report['timeout_400ms_passed']=True;report['zero_gpio_transport_passed']=True;report['unknown_preserved_system_state']=True
except BaseException as e:
    report['error']=repr(e)
finally:
    try:
        if mutated and ctrl!=invalid:
            output(1,1);output(3);time.sleep(.13);report['idle_restored']=get_input('final_idle')['system']==2
    except BaseException as e:report['cleanup_error']=repr(e)
    for handle in (rw,ctrl):
        if handle!=invalid:kernel.CloseHandle(handle)
    for buf,ov in pending_keepalive:kernel.CloseHandle(ov.hEvent)
    report['finished']=datetime.now(timezone.utc).isoformat();report['handles_closed']=True;report['passed']=report['error'] is None and not report.get('cleanup_error') and report.get('idle_restored',False);put()
print(json.dumps(report,ensure_ascii=False))
assert report['passed']
