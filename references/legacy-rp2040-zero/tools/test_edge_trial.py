"""Execute the actual ARM-compiled edge map and gain capture with fake ADC/time.

Requires unicorn and pyelftools (may be installed into build/test-deps).
No board is required; this does not simulate the actual magnet or USB hardware.
"""
from pathlib import Path
import argparse
import math
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'build/test-deps'))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP, UC_ARM_REG_LR
from elftools.elf.elffile import ELFFile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
args = parser.parse_args()
sketch = ROOT / 'firmware/RP2040RhythmControllerEdgeCalibrated'
source = (sketch / 'RP2040RhythmControllerEdgeCalibrated.ino').read_text(encoding='utf-8')
gain_functions = source[source.index('bool captureSensorGainSample()'):source.index('bool captureCenterCalibration()')]
gain_struct = source[source.index('struct GainCaptureState {'):source.index('MouseReport mouseReport =')]
testdir = ROOT / 'build/edge-tests'
testdir.mkdir(parents=True, exist_ok=True)
test_source = '''
#include <stdint.h>
#include "EdgeResponse.h"
constexpr uint8_t HALL_COUNT = 4;
constexpr float FACTORY_SENSOR_REFERENCE = 64.0f;
constexpr float DEFAULT_LEFT_ENDPOINT_RATIO = .105f, DEFAULT_RIGHT_ENDPOINT_RATIO = .105f;
constexpr float DEFAULT_LEFT_HANDOFF_RATIO = .30f, DEFAULT_RIGHT_HANDOFF_RATIO = .30f;
template<class T> T clampValue(T x,T a,T b) { return x<a?a:(x>b?b:x); }
float safeMin(float a,float b) {return a<b?a:b;}
float safeMax(float a,float b) {return a>b?a:b;}
uint32_t fakeTime;
uint32_t millis() {return fakeTime;}
void sendStatusJson(const char*) {}
int saves;
void saveHallCalibration() {++saves;}
struct {bool active;} endpointScan;
bool hallSignalReliable;
uint8_t hallDominantSensor;
float rawActivation[4],sensorGainSamples[4];
struct {uint16_t raw[4];} hallRuntime;
struct {
  float sensorReference[4],sensorGain[4];
  float leftEndpointRatio,rightEndpointRatio,leftHandoffRatio,rightHandoffRatio;
} hallCal;
''' + gain_struct + '''
void resetHallTracking() { gainCapture.active=false; }
''' + gain_functions + '''
extern "C" float edge_map(float x,float ep,float handoff) {
  return EdgeResponse::map(x,ep,handoff);
}
extern "C" int gain_case(int mode) {
  fakeTime=0; saves=0; gainCapture={}; endpointScan.active=false;
  hallSignalReliable=true; hallDominantSensor=1;
  for(int i=0;i<4;++i) {
    hallCal.sensorReference[i]=100; hallCal.sensorGain[i]=1;
    sensorGainSamples[i]=100; rawActivation[i]=100; hallRuntime.raw[i]=2048;
  }
  hallCal.sensorReference[0]=200; sensorGainSamples[0]=200;
  if(!captureSensorGainSample()) return 0;
  // A duplicate click must not replace the in-progress capture.
  if(captureSensorGainSample()) return 0;
  for(fakeTime=2;fakeTime<=500;fakeTime+=(mode==3?20:2)) {
    if(mode==1 && fakeTime>=250) hallDominantSensor=2;
    if(mode==2) hallRuntime.raw[0]=4095;
    if(mode==4) rawActivation[0]=10;
    if(mode==5 && fakeTime==250) rawActivation[0]=160;
    else if(mode==5) rawActivation[0]=100;
    serviceGainCapture();
  }
  if(gainCapture.active) { fakeTime=502; serviceGainCapture(); }
  if(mode==0) return saves==1 && hallCal.sensorReference[0]==100 && hallCal.sensorGain[0]==1;
  return saves==0 && hallCal.sensorReference[0]==200;
}
'''
(testdir / 'harness.cpp').write_text(test_source, encoding='utf-8')
elfpath = testdir / 'harness.elf'
subprocess.run([args.compiler, '-mcpu=cortex-m0plus', '-mthumb', '-mfloat-abi=soft', '-Os',
                '-fno-exceptions', '-fno-rtti', '-nostdlib',
                '-I', str(sketch), str(testdir / 'harness.cpp'), '-Wl,-Ttext=0x10000000',
                '-Wl,-Tdata=0x20000000', '-Wl,-e,edge_map', '-lc', '-lgcc', '-o', str(elfpath)], check=True)
emu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
emu.mem_map(0x10000000, 0x100000)
emu.mem_map(0x20000000, 0x40000)
stop = 0x100F0000
with elfpath.open('rb') as stream:
    elf = ELFFile(stream)
    for seg in elf.iter_segments():
        if seg['p_type'] == 'PT_LOAD':
            addr = seg['p_vaddr']
            if seg['p_filesz']:
                # Linker may include a read-only ELF-header page below text.
                if addr < 0x10000000:
                    lo = addr & ~4095
                    emu.mem_map(lo, 0x10000000 - lo)
                emu.mem_write(addr, seg.data())
    symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}

def call(name, values):
    for reg, val in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2], values):
        emu.reg_write(reg, val)
    emu.reg_write(UC_ARM_REG_SP, 0x2003F000)
    emu.reg_write(UC_ARM_REG_LR, stop | 1)
    emu.emu_start(symbols[name] | 1, stop, count=4000000)
    return emu.reg_read(UC_ARM_REG_R0)

def bits(x): return struct.unpack('<I', struct.pack('<f', x))[0]
def mapped(x, ep=.105, handoff=.3):
    return struct.unpack('<f', struct.pack('<I', call('edge_map', [bits(x), bits(ep), bits(handoff)])))[0]

# Dense sweeps check direction, range and continuity for independent calibrations.
checks = 0
for ep, handoff in [(a,b) for a in [.02,.07,.105,.20,.32] for b in [.1,.3,.6]]:
    last = mapped(0,ep,handoff)
    for i in range(1,2001):
        x=i/2000
        y=mapped(x,ep,handoff)
        assert math.isfinite(y) and -1e-6 <= y <= 1.000001
        assert y >= last-1e-6, (ep,handoff,x,last,y)
        assert y-last < .0025, (ep,handoff,x,'discontinuity',y-last)
        if x <= .88: assert abs(y-x) < 1e-6
        last=y
        checks+=1
    assert abs(last-1) < 1e-6
assert math.isfinite(mapped(.93, float('nan'), float('inf')))
for mode, label in enumerate(['stable lower replacement','changed sensor','ADC clipping','too few samples','weak signal','unstable capture']):
    assert call('gain_case',[mode]) == 1, label
    print('PASS gain capture:',label)
print(f'PASS {checks} ARM-executed edge samples: monotonic, bounded, continuous, interior unchanged')

# Validate both delivered UF2 structures and RP2040 family; no EEPROM overlap.
for uf2 in (ROOT/'dist/edge-trial-1').glob('*.uf2'):
    data=uf2.read_bytes()
    assert len(data)%512==0
    count=len(data)//512
    for n in range(count):
        block=data[n*512:(n+1)*512]
        magic1,magic2,flags,address,size,index,total,family=struct.unpack_from('<8I',block)
        assert (magic1,magic2)==(0x0A324655,0x9E5D5157)
        assert struct.unpack_from('<I',block,508)[0]==0x0AB16F30
        assert flags & 0x2000 and family==0xE48BFF56
        assert index==n and total==count and size<=476
        assert 0x10000000<=address and address+size<=0x101FF000
    print('PASS UF2:',uf2.name,count,'blocks; RP2040 family; EEPROM region untouched')
