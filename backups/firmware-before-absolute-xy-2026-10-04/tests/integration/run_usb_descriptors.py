"""Execute real descriptor constructors/callbacks from the formal ARM ELF.
Only pre-USB saved-mode selection, port connection, C runtime ROM helpers and
unique-ID hardware boundary are replaced; no USB bus/host enumeration claim.
"""
from pathlib import Path
import hashlib,json,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
HERE=ROOT/'tests/integration'
sys.path.insert(0,str(ROOT/'tests/module1/build/python-deps'))
import unicorn
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
assert unicorn.__version__=='2.1.3'
firmware=json.loads((HERE/'build/firmware_results.json').read_text())
assert firmware['exit_code']==0
for name,digest in firmware['source_sha256'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,name
elf=HERE/'build/arduino/FluxRail.ino.elf'
db=json.loads((HERE/'build/arduino/compile_commands.json').read_text())
compiler=Path(db[0]['arguments'][0]).with_suffix('.exe')
rows=subprocess.check_output([str(compiler.parent/'arm-none-eabi-nm.exe'),'-n','-C',str(elf)],text=True)
symbols={}
for line in rows.splitlines():
    parts=line.split(maxsplit=2)
    if len(parts)==3:symbols[parts[2]]=int(parts[0],16)
def sym(name):
    if name not in symbols:raise RuntimeError('Missing real ELF symbol: '+name)
    return symbols[name]
raw=elf.read_bytes();assert raw[:6]==b'\x7fELF\x01\x01'
loads=[]
phoff=struct.unpack_from('<I',raw,28)[0];phsize,phcount=struct.unpack_from('<HH',raw,42)
for i in range(phcount):
    typ,off,addr,_,size,memsize,_,_=struct.unpack_from('<8I',raw,phoff+i*phsize)
    if typ==1 and memsize:loads.append((addr,raw[off:off+size]))
reports=[]
for mode in [0,1]:
    m=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);m.mem_map(0x10000000,0x400000);m.mem_map(0x20000000,0x50000)
    for addr,data in loads:
        if data:m.mem_write(addr,data)
    trace=[];hooks={}
    def return_value(value=None):
        if value is not None:m.reg_write(UC_ARM_REG_R0,value)
        m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR))
    def memory_copy():
        d,s,n=(m.reg_read(r) for r in [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2]);m.mem_write(d,bytes(m.mem_read(s,n)));return_value(d)
    def memory_set():
        d,v,n=(m.reg_read(r) for r in [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2]);m.mem_write(d,bytes([v&255])*n);return_value(d)
    def mode_boundary():
        trace.append('mode_before_default_cdc');return_value(mode==0)
    def port_boundary():
        trace.append('first_port_connect');return_value()
    def guard_acquire():return_value(1)
    def guard_release():
        m.mem_write(m.reg_read(UC_ARM_REG_R0),struct.pack('<I',1));return_value()
    def serial_boundary():
        m.mem_write(m.reg_read(UC_ARM_REG_R0),bytes.fromhex('0123456789abcdef'));return_value(8)
    for name,fn in [('__wrap_memcpy',memory_copy),('__wrap_memset',memory_set),('fluxrail_usb_wants_cdc',mode_boundary),('TinyUSB_Port_InitDevice(unsigned char)',port_boundary),('__cxa_guard_acquire',guard_acquire),('__cxa_guard_release',guard_release),('fluxrail::app::App::prepareUsbBoot(bool)',lambda:return_value(mode)),('TinyUSB_Port_GetSerialNumber(unsigned char*)',serial_boundary),('__aeabi_atexit',lambda:return_value(0))]:
        if name in symbols:hooks[sym(name)]=fn
        elif name != "__aeabi_atexit":raise RuntimeError(name)
    def code_hook(machine,addr,size,data):
        if addr in hooks:hooks[addr]()
    m.hook_add(UC_HOOK_CODE,code_hook)
    end=0x100ff000
    def call(name,*args):
        m.reg_write(UC_ARM_REG_SP,0x20041000);m.reg_write(UC_ARM_REG_LR,end|1)
        for reg,value in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3],args):m.reg_write(reg,value)
        m.emu_start(sym(name)|1,end,count=1000000)
        assert m.reg_read(UC_ARM_REG_PC)==end,'ARM instruction budget incomplete: '+name
        return m.reg_read(UC_ARM_REG_R0)
    # Device has no nontrivial constructor; begin initializes its descriptor state.
    # The platform API owns SerialTinyUSB, exported as Serial.
    call('Adafruit_USBD_CDC::Adafruit_USBD_CDC()',sym('Serial'))
    assert call('Adafruit_USBD_Device::begin(unsigned char)',sym('TinyUSBDevice'),0)==1
    assert trace==['mode_before_default_cdc','first_port_connect'],trace
    ptr=call('tud_descriptor_device_cb');dev=bytes(m.mem_read(ptr,18));vid,pid=struct.unpack_from('<HH',dev,8)
    ptr=call('tud_descriptor_configuration_cb',0);head=bytes(m.mem_read(ptr,9));total=struct.unpack_from('<H',head,2)[0];cfg=bytes(m.mem_read(ptr,total))
    interfaces=[];endpoints=[];hid_strings=[]
    p=9
    while p<total:
        n,t=cfg[p:p+2];assert n>=2 and p+n<=total
        if t==4:
            interfaces.append(cfg[p+5]);
            if cfg[p+5]==3:hid_strings.append(cfg[p+8])
        if t==5:endpoints.append({'address':cfg[p+2],'size':struct.unpack_from('<H',cfg,p+4)[0],'interval':cfg[p+6]})
        p+=n
    assert p==total and head[4]==len(interfaces)
    strings={}
    for index in [1,2,3]+hid_strings:
        if not index:continue
        ptr=call('tud_descriptor_string_cb',index,0x409);assert ptr
        size=m.mem_read(ptr,1)[0];data=bytes(m.mem_read(ptr,size));assert data[1]==3
        strings[str(index)]=data[2:].decode('utf-16-le')
    assert strings['3']=='0123456789ABCDEF'
    reportptr=call('tud_hid_descriptor_report_cb',0)
    assert reportptr
    if mode==1:
        assert (vid,pid)==(0x0ca3,0x0021) and dev[4:7]==bytes(3)
        assert interfaces==[3] and total==41 and len(endpoints)==2
        assert all(e['size']==64 and e['interval']==1 for e in endpoints)
        assert strings['1']=='SEGA' and len(strings['2'])==92
        assert len(hid_strings)==1 and len(strings[str(hid_strings[0])])==93
        assert strings[str(hid_strings[0])]==strings['2']+';'
        report=bytes(m.mem_read(reportptr,struct.unpack('<H',m.mem_read(sym('fluxrail::usb::kIo4DescriptorBytes'),2))[0]))
    else:
        assert interfaces==[2,10,3,3] and dev[4:7]==bytes([0xef,2,1])
        assert strings['1']=='FluxRail' and strings['2']=='FluxRail NKRO prototype'
        assert len(endpoints)==5 and len(hid_strings)==2
        # Query the actual second HID instance and parse report lengths/axes.
        mouseptr=call('tud_hid_descriptor_report_cb',1);assert mouseptr
        report_len=None;p=9;instance=-1
        while p<total:
            n,t=cfg[p:p+2]
            if t==4 and cfg[p+5]==3:instance+=1
            if t==0x21 and instance==1:report_len=struct.unpack_from('<H',cfg,p+7)[0]
            p+=n
        assert report_len
        report=bytes(m.mem_read(mouseptr,report_len))
        # HID globals persist across collections; inspect every Input's report ID,
        # report size/count, usage list and relative bit rather than string matching.
        p=0;rid=0;size=0;count=0;page=0;usages=[];inputs=[]
        while p<len(report):
            prefix=report[p];p+=1;length=[0,1,2,4][prefix&3]
            value=int.from_bytes(report[p:p+length],'little');p+=length
            typ=(prefix>>2)&3;tag=prefix>>4
            if typ==1:
                if tag==0:page=value
                elif tag==7:size=value
                elif tag==8:rid=value
                elif tag==9:count=value
            elif typ==2 and tag==0:usages.append(value)
            elif typ==0:
                if tag==8:inputs.append({'id':rid,'bits':size*count,'page':page,'usages':usages[:],'flags':value})
                usages=[]
        assert sum(i['bits'] for i in inputs if i['id']==1)==40
        assert sum(i['bits'] for i in inputs if i['id']==2)==16
        absolute=[i for i in inputs if i['id']==2]
        assert len(absolute)==1 and absolute[0]['page']==1 and absolute[0]['usages']==[0x30]
        assert not (absolute[0]['flags']&4),'absolute X must not be Relative'
        relative=[i for i in inputs if i['id']==1 and i['page']==1 and i['usages']==[0x30,0x31]]
        assert len(relative)==1 and relative[0]['flags']&4

    reports.append({'mode':'IO4' if mode else 'NORMAL_NKRO','device_hex':dev.hex(),'config_hex':cfg.hex(),'strings':strings,'interfaces':interfaces,'endpoints':endpoints,'startup_trace':trace,'report_hex':report.hex() if report else None,'passed':True})
result={'validation':'ARM execution of real formal-ELF descriptor construction and callbacks','elf_sha256':hashlib.sha256(raw).hexdigest(),'unicorn':unicorn.__version__,'results':reports,'hardware_tested':False,'game_tested':False,'boundaries':['saved mode already validated by App C++ suite','USB port init suppressed','ROM memcpy/memset','flash unique ID synthetic']}
(HERE/'build/usb_descriptor_results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS: actual formal ELF both personas, all descriptors/long strings/unique serial/first-connect ordering; no host enumeration')
