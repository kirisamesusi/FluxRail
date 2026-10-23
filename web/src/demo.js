import { goldenHex } from '../tests/golden.js';
import { decodeSnapshot, unhex, hex, crc32 } from './snapshot.js';
import { hallFields, knobFields, pushFields, validateProfile, validateConfig, validName, assert } from './config.js';
/** 隔离的CDC模拟端口：只演示公共已实现命令，所有能力与阶段8对齐。
 * 不发现硬件、不写Flash。采用公共二进制金样，模拟的状态不充当实板证据。
 */
const golden=unhex(goldenHex),defaults=decodeSnapshot(golden);
const clone=value=>structuredClone(value),next=value=>value===0xffffffff?1:value+1;
const modes=['static','breathing','color_cycle','rainbow_wave','press'];
export class DemoHost {
  constructor(){this.device=clone(defaults);this.saved=null;this.requests=[];this.requestCount=0;this.transfer=null;this.calibrationTail=golden.slice(18+329*4);}
  meta(){const d=this.device;return {schema:1,active_profile:d.active_profile,profile_count:d.profiles.length,next_profile_id:d.next_profile_id,startup_usb_mode:'NORMAL_NKRO',calibration_valid:d.calibration.valid,calibration_generation:d.calibration_generation};}
  snapshot(){
    // 仅模拟需要编码；正式导出直接取设备原始字节以保留浮点/校准精度。
    const d=this.device,bytes=new Uint8Array(18+329*d.profiles.length+this.calibrationTail.length),v=new DataView(bytes.buffer);let at=0;
    function put(type,value){const methods={u8:'setUint8',u16:'setUint16',u32:'setUint32',f32:'setFloat32',f64:'setFloat64'},size={u8:1,u16:2,u32:4,f32:4,f64:8}[type];v[methods[type]](at,value,true);at+=size;}
    const b=x=>put('u8',x?1:0),u=x=>put('u32',x),color=x=>x.forEach(y=>put('u8',y));
    [1,d.revision,d.active_profile,d.next_profile_id].forEach(u);put('u8',d.profiles.length);put('u8',0);
    for(const p of d.profiles){
      u(p.profile_id);{const nb=new TextEncoder().encode(p.name);for(let i=0;i<32;i++)put('u8',nb[i]||0);}
      for(const {action:a}of p.config.keyboard.bindings){put('u8',a.type==='keyboard'?1:a.type==='mouse_button'?2:0);put('u8',a.usage||0);put('u8',a.modifiers||0);put('u8',a.button||0);}
      const c=p.config,l=c.lighting;u(c.keyboard.retrigger_gap_us);u(c.scan.press_debounce_us);u(c.scan.release_debounce_us);
      b(l.enabled);put('u8',modes.indexOf(l.mode));put('u8',l.brightness_level);b(l.per_key);u(l.period_ms);b(l.reverse);b(c.indicator.enabled);color(l.global_color);l.key_colors.forEach(color);color(c.indicator.color);
      const m=p.motion;b(m.hall.enabled);b(m.hall.invert);for(const[k,,,,type]of hallFields)put(type,m.hall[k]);
      for(const k of m.knobs){b(k.enabled);put('u8',k.axis);b(k.invert);for(const[key,,,,type]of knobFields)put(type,k[key]);b(k.push.enabled);for(const[key,,,,type]of pushFields)put(type,k.push[key]);}
      u(m.routing.idle_us);b(m.routing.simultaneous_hall_first);
    }
    bytes.set(this.calibrationTail,at);v.setUint32(at,d.calibration_generation,true);return bytes;
  }
  handle(request){
    if(this.requests.length===1000)this.requests.shift();this.requests.push(clone(request));this.requestCount++;const d=this.device,{op,payload:p,id}=request;let result,ok=true,error;
    try{
      assert(request.protocol_version===1,'协议版本不匹配');
      const find=()=>{const profile=d.profiles.find(pr=>pr.profile_id===p.profile_id);assert(profile,'模板不存在');return profile;};
      const reads=['hello','get_device','list_profiles','get_profile','get_status','get_config','export_config'];
      if(!reads.includes(op))assert(p.expected_revision===d.revision,'REVISION_CONFLICT');
      if(op==='hello')result={product:'FluxRail',board:'FluxRail RP2040 4MiB',firmware_version:'SIMULATION-0.7.0',serial:'SIMULATION-NO-HARDWARE',usb_mode:'NORMAL_NKRO',scope:'device_schema1',capabilities:{nkro:true,mouse_buttons:true,lighting:true,cdc_config:true,ram_preview:true,profiles:true,flash_save:true,global_calibration_storage:true,snapshot_transfer:true,hall:false,knobs:false,push_hardware:false,absolute_x:false,io4:false,bootloader_command:false,max_profiles:12,binding_sources:12,max_line_bytes:4096,max_tokens:384,max_depth:12}};
      else if(op==='get_device')result=this.meta();
      else if(op==='list_profiles')result={active_profile:d.active_profile,max_profiles:12,profiles:d.profiles.map(pr=>({profile_id:pr.profile_id,name:pr.name,preset:pr.profile_id<=4}))};
      else if(op==='get_profile')result=clone(find());
      else if(op==='get_config')result={scope:'runtime_module1',config_ready:true,config:clone(d.profiles.find(pr=>pr.profile_id===d.active_profile).config)};
      else if(op==='get_status')result={config_ready:true,applying:false,apply_failed:false,requested_revision:d.revision,maintenance:'idle',save_result:this.saved?1:0,valid_slots:this.saved?1:0,physical_mask:1<<(Math.floor(Date.now()/1000)%10),protocol_requests:this.requestCount,protocol_rejected:0,overlong_lines:0};
      else if(op==='export_config'){this.transfer=null;const bytes=this.snapshot(),end=Math.min(p.offset+512,bytes.length);assert(p.offset>=0&&p.offset<bytes.length,'偏移越界');result={schema:1,encoding:'hex-le-schema1',length:bytes.length,crc32:crc32(bytes),offset:p.offset,data:hex(bytes.slice(p.offset,end)),next_offset:end};}
      else if(op==='save_config'){this.saved=d.revision;result=this.meta();}
      else if(op==='begin_import'){assert(p.length>0&&p.length<=7680,'长度无效');this.transfer={length:p.length,crc:p.crc32,data:[],revision:d.revision};result=this.meta();}
      else if(op==='import_chunk'){const t=this.transfer;assert(t&&t.revision===d.revision&&p.offset===t.data.length,'INVALID_TRANSFER');const block=unhex(p.data);assert(block.length<=512&&t.data.length+block.length<=t.length,'INVALID_TRANSFER');t.data.push(...block);result=this.meta();}
      else {
        if(op==='set_profile'){const pr=find();const candidate={...pr,config:p.config,motion:p.motion};validateProfile(candidate);pr.config=clone(p.config);pr.motion=clone(p.motion);}
        else if(op==='create_profile'||op==='copy_profile'){const source=find();validName(p.name);assert(d.profiles.length<12&&d.next_profile_id<0xffffffff,'容量已满');d.profiles.push({...clone(source),profile_id:d.next_profile_id++,name:p.name});}
        else if(op==='rename_profile'){validName(p.name);find().name=p.name;}
        else if(op==='delete_profile'){find();assert(p.profile_id>4,'预置不可删除');d.profiles=d.profiles.filter(pr=>pr.profile_id!==p.profile_id);if(d.active_profile===p.profile_id)d.active_profile=4;}
        else if(op==='set_active_profile'){find();d.active_profile=p.profile_id;}
        else if(op==='restore_preset'){find();assert(p.profile_id<=4,'不是预置');d.profiles[d.profiles.findIndex(pr=>pr.profile_id===p.profile_id)]=clone(defaults.profiles.find(pr=>pr.profile_id===p.profile_id));}
        else if(op==='set_config'){validateConfig(p.config);d.profiles.find(pr=>pr.profile_id===d.active_profile).config=clone(p.config);}
        else if(op==='commit_import'){
          const t=this.transfer;assert(t&&t.revision===d.revision&&t.data.length===t.length&&crc32(t.data)===t.crc,'INVALID_TRANSFER');const bytes=Uint8Array.from(t.data),candidate=decodeSnapshot(bytes);
          candidate.revision=d.revision;candidate.next_profile_id=Math.max(candidate.next_profile_id,d.next_profile_id);candidate.calibration_generation=next(d.calibration_generation);this.calibrationTail=bytes.slice(18+329*candidate.profiles.length);this.device=candidate;this.transfer=null;
        }else throw new Error('UNSUPPORTED_OPERATION');
        this.device.revision=next(d.revision);result=this.meta();
      }
    }catch(e){ok=false;error={code:e.message==='REVISION_CONFLICT'?'REVISION_CONFLICT':'SIMULATION_REJECTED',message:e.message};}
    return {protocol_version:1,id,ok,revision:this.device.revision,applied_revision:this.device.revision,saved_revision:this.saved,dirty:this.device.revision!==this.saved,...(ok?{result}:{error})};
  }
}
export function demoPort(host=new DemoHost()){
  let output,fragments='';const port={simulated:true,host};
  port.open=async()=>{
    port.readable=new ReadableStream({start(controller){output=controller;}});
    port.writable=new WritableStream({write(bytes){
      fragments+=new TextDecoder().decode(bytes);let at;
      while((at=fragments.indexOf('\n'))>=0){const request=JSON.parse(fragments.slice(0,at));fragments=fragments.slice(at+1);
        const ascii=JSON.stringify(host.handle(request)).replace(/[^\x00-\x7f]/g,ch=>'\\u'+ch.charCodeAt(0).toString(16).padStart(4,'0'));
        const response=new TextEncoder().encode(ascii+'\n');
        // 分块交付主动覆盖半行解析；无运动算法或主机USB参与。
        for(let i=0;i<response.length;i+=97)output.enqueue(response.slice(i,i+97));}
    }});
  };
  port.setSignals=async()=>{};port.close=async()=>{};return port;
}
export async function createDemoPort(){return demoPort();}
