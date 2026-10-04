import { assert, range, validateProfile, hallFields, knobFields, pushFields } from './config.js';
/** 公共DeviceCodec.cpp / schema1的逐字段适配，绝不读取C++对象布局。
 * 导入完整候选先在网页检查长度、CRC、字段/范围，固件commit再执行完整地图语义检查。
 * 文件封套只保存协议已有hex-le-schema1字节及设备身份，不是第二个设备存储schema。
 */
export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const b of bytes) { crc ^= b; for (let i=0;i<8;i++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0); }
  return (crc ^ 0xffffffff) >>> 0;
}
export const hex = bytes => Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');
export function unhex(text) {
  assert(typeof text === 'string' && text.length > 0 && text.length % 2 === 0 && /^[0-9a-f]+$/.test(text),'hex格式无效');
  return Uint8Array.from(text.match(/../g),b=>parseInt(b,16));
}
const modes=['static','breathing','color_cycle','rainbow_wave','press'];
export function decodeSnapshot(bytes) {
  assert(bytes.length >= 18 && bytes.length <= 7680,'快照长度无效');
  const v = new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength); let offset=0;
  function read(type) {
    const sizes={u8:1,u16:2,u32:4,f32:4,f64:8}, size=sizes[type]; assert(offset+size<=bytes.length,'快照被截断');
    const methods={u8:'getUint8',u16:'getUint16',u32:'getUint32',f32:'getFloat32',f64:'getFloat64'};
    const n=v[methods[type]](offset,true); offset+=size; assert(Number.isFinite(n),'快照包含NaN/Infinity'); return n;
  }
  function boolean() { const b=read('u8'); assert(b<=1,'布尔字节无效'); return !!b; }
  const color=()=>[read('u8'),read('u8'),read('u8')];
  const schema=read('u32'),revision=read('u32'),active_profile=read('u32'),next_profile_id=read('u32'),count=read('u8'),startup=read('u8');
  assert([1,2].includes(schema) && revision>0 && count>=4 && count<=12 && (schema===1?startup===0:startup<=1) && next_profile_id>=5,'schema/模板数量/USB模式不支持');
  const profiles=[];
  for(let i=0;i<count;i++) {
    const profile_id=read('u32'); let name='', ended=false;
    for(let j=0;j<32;j++){const b=read('u8'); if(!b)ended=true; else if(!ended)name+=String.fromCharCode(b);}
    assert(ended,'模板名称缺少NUL终止'); const bindings=[];
    for(let j=0;j<12;j++){
      const type=read('u8'),usage=read('u8'),modifiers=read('u8'),button=read('u8');
      assert(type<=2,'动作enum无效');
      // 公共decoder检查inactive字段归零，避免隐藏动作被错误导入。
      assert(type===1 || (!usage&&!modifiers),'未使用的键盘字段不为0'); assert(type===2 || !button,'未使用的鼠标字段不为0');
      const action=type===0?{type:'disabled'}:type===1?{type:'keyboard',usage,modifiers}:{type:'mouse_button',button};
      bindings.push({source_id:j,action});
    }
    const keyboard={bindings,retrigger_gap_us:read('u32')};
    const scan={press_debounce_us:read('u32'),release_debounce_us:read('u32')};
    const lighting={enabled:boolean(),mode:modes[read('u8')],brightness_level:read('u8'),per_key:boolean(),period_ms:read('u32'),reverse:boolean()};
    const indicator={enabled:boolean()}; lighting.global_color=color(); lighting.key_colors=Array.from({length:10},color); indicator.color=color();
    const hall={enabled:boolean(),invert:boolean()}; for(const [k,,,,type] of hallFields)hall[k]=read(type);
    const knobs=[];
    for(let j=0;j<2;j++){
      const k={enabled:boolean(),axis:read('u8'),invert:boolean()}; for(const [key,,,,type]of knobFields)k[key]=read(type);
      k.push={enabled:boolean()};for(const[key,,,,type]of pushFields)k.push[key]=read(type);knobs.push(k);
    }
    const routing={idle_us:read('u32'),simultaneous_hall_first:boolean()};
    const p={profile_id,name,config:{keyboard,scan,lighting,indicator},motion:{hall,knobs,routing}}; if(schema===2)p.io4_bindings=Array.from({length:12},()=>read('u8')); validateProfile(p); profiles.push(p);
  }
  const ids=profiles.map(p=>p.profile_id);
  assert(new Set(ids).size===count && [1,2,3,4].every(id=>ids.includes(id)) && ids.includes(active_profile) && ids.every(id=>id<next_profile_id),'模板ID/活动ID/高水位无效');
  const calibration_generation=read('u32'),calibration={valid:boolean()};
  if(calibration.valid){
    calibration.version=read('u16');assert(calibration.version===1,'校准版本不支持');calibration.channels=[];
    for(let i=0;i<3;i++){
      const c={baseline:read('f32'),baseline_noise:read('f32'),gain:read('f32'),polarity:read('u8')};
      range(c.baseline,16,4079);range(c.baseline_noise,0,8);range(c.gain,0.1,10);assert([1,255].includes(c.polarity),'校准极性无效');calibration.channels.push(c);
    }
    calibration.left_mm=read('f32');calibration.right_mm=read('f32');range(calibration.left_mm,-200,200);range(calibration.right_mm,-200,200);
    assert(calibration.right_mm-calibration.left_mm>=1,'机械端点无效');const nodes=read('u8');range(nodes,7,17,true);calibration.nodes=[];
    for(let i=0;i<nodes;i++){
      const n={mm:read('f32'),response:[read('f32'),read('f32'),read('f32')],noise:[read('f32'),read('f32'),read('f32')]};
      range(n.mm,calibration.left_mm,calibration.right_mm);n.response.forEach(x=>range(x,-4095,4095));n.noise.forEach(x=>range(x,0,80));
      if(i)range(n.mm-calibration.nodes[i-1].mm,0.5,20);calibration.nodes.push(n);
    }
    calibration.sweep_segments=read('u32');calibration.sweep_left=boolean();calibration.sweep_right=boolean();calibration.noise_mm=read('f32');range(calibration.noise_mm,0,5);
    assert(calibration.sweep_left&&calibration.sweep_right&&calibration.sweep_segments===(1<<(nodes-1))-1,'扫描覆盖不完整');
    assert(Math.abs(calibration.nodes[0].mm-calibration.left_mm)<=0.01&&Math.abs(calibration.nodes.at(-1).mm-calibration.right_mm)<=0.01,'校准节点未覆盖端点');
  }
  assert(offset===bytes.length,'快照含多余字节');
  return {schema,revision,active_profile,next_profile_id,profiles,startup_usb_mode:startup?'IO4':'NORMAL_NKRO',calibration_generation,calibration};
}
export async function exportSnapshot(client) {
  let offset=0,revision,crc,length;const bytes=[];
  do {
    const c=await client.request('export_config',{offset});
    assert([1,2].includes(c.schema)&&c.encoding===`hex-le-schema${c.schema}`,'导出schema不支持');
    if(offset===0){revision=client.revision;crc=c.crc32;length=c.length;range(length,1,7680,true);range(crc,0,0xffffffff,true);}
    assert(client.revision===revision&&c.crc32===crc&&c.length===length&&c.offset===offset,'导出期间配置改变，请从首块重新导出');
    const block=unhex(c.data);assert(block.length<=512&&c.next_offset===offset+block.length&&c.next_offset<=length,'导出分块无效');bytes.push(...block);offset=c.next_offset;
  }while(offset<length);
  const result=Uint8Array.from(bytes);assert(crc32(result)===crc,'导出CRC错误');decodeSnapshot(result);return result;
}
export async function importSnapshot(client,bytes,expected) {
  decodeSnapshot(bytes);assert(client.revision===expected,'导入预览后设备配置改变，请重新读回并预览');
  await client.request('begin_import',{expected_revision:expected,length:bytes.length,crc32:crc32(bytes)},4000);
  for(let offset=0;offset<bytes.length;offset+=512){
    assert(client.revision===expected,'导入期间revision改变');
    await client.request('import_chunk',{expected_revision:expected,offset,data:hex(bytes.slice(offset,offset+512))},4000);
  }
  assert(client.revision===expected,'提交前revision改变');
  return client.request('commit_import',{expected_revision:expected},4000);
}
export function archive(bytes,hello) {
  return {product:hello.product,board:hello.board,serial:hello.serial,firmware_version:hello.firmware_version,schema:decodeSnapshot(bytes).schema,encoding:`hex-le-schema${decodeSnapshot(bytes).schema}`,length:bytes.length,crc32:crc32(bytes),data:hex(bytes)};
}
export function parseArchive(text) {
  assert(text.length<=20000,'配置文件过大');const a=JSON.parse(text);
  assert(a.product==='FluxRail'&&a.board==='FluxRail RP2040 4MiB'&&[1,2].includes(a.schema)&&a.encoding===`hex-le-schema${a.schema}`,'文件产品/板型/schema不匹配');
  range(a.length,1,7680,true);range(a.crc32,0,0xffffffff,true);
  assert(typeof a.data==='string'&&a.data.length===a.length*2,'文件数据长度不匹配');const bytes=unhex(a.data);
  assert(crc32(bytes)===a.crc32,'配置CRC不匹配');const device=decodeSnapshot(bytes);assert(device.schema===a.schema,"封套schema与payload不符");return {archive:a,bytes,device};
}
