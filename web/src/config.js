/** schema1字段约束的网页适配。参数与公共C++校验保持一致，固件仍是最终校验者。
 * 描述表也用于高级编辑。gain字段保持实际倍率；产品1.0～10.0刻度
 * 由sensitivity.js换算，不能在快照/协议校验处再乘除5或截断旧值。
 */
export const hallFields = [
  ['center_gain','中心倍率',0.1,8,'f32'],['range_min','输出下界（归一化）',0,1,'f32'],['range_max','输出上界（归一化）',0,1,'f32'],
  ['quiet_tau_us','静止滤波 µs',0,20000,'u32'],['moving_tau_us','移动滤波 µs',0,20000,'u32'],
  ['fast_speed_mm_s','滤波过渡速度 mm/s',1,5000,'f32'],['activity_mm','活动门槛 mm',0.01,5,'f32'],
  ['activity_hold_us','活动保持 µs',0,100000,'u32'],['enter_signal','进入信号 ADC count',1,12000,'f32'],
  ['exit_signal','退出信号 ADC count',1,12000,'f32'],['curve_tolerance','曲线容差 ADC count',1,1000,'f32'],['max_gap_us','最大有效间隔 µs',1000,100000,'u32']
];
export const knobFields = [
  ['gain','倍率',0.01,16,'f64'],['counts_per_turn_at_gain1','灵敏度5.0每圈 HID count',1,1000000,'f64'],
  ['filter_tau_us','滤波 µs',0,20000,'u32'],['max_gap_us','最大有效间隔 µs',500,100000,'u32'],
  ['max_turns_per_second','可信转速 圈/s',0.01,1000,'f64'],['activity_ticks','活动门槛 ticks',1,8191,'u16'],['activity_hold_us','活动保持 µs',0,1000000,'u32']
];
export const pushFields = [
  ['press_us','按下确认 µs',0,20000,'u32'],['rotating_press_us','旋转按下确认 µs',0,50000,'u32'],['release_us','释放确认 µs',0,20000,'u32'],
  ['rearm_us','重新就绪 µs',500,100000,'u32'],['max_hold_us','保持超时 µs',1000,16000000,'u32'],['max_sample_gap_us','采样间隔上限 µs',500,100000,'u32']
];
export function assert(value, message) { if (!value) throw new Error(message); }
export function range(value, min, max, integer = false) {
  assert(typeof value === 'number' && Number.isFinite(value) && value >= min && value <= max && (!integer || Number.isInteger(value)), `数值超出范围 ${min}～${max}`);
}
export function shape(object, names) {
  assert(object && typeof object === 'object' && !Array.isArray(object) && Object.keys(object).length === names.length && names.every(n => Object.hasOwn(object,n)), '字段缺失或含未知字段');
}
function bool(value) { assert(typeof value === 'boolean','开关须为布尔值'); }
function rgb(color) { assert(Array.isArray(color) && color.length === 3,'RGB须为三通道'); color.forEach(v => range(v,0,255,true)); }
export function validName(name) { assert(typeof name === 'string' && /^[\x20-\x21\x23-\x5b\x5d-\x7e]{1,31}$/.test(name),'名称须为1～31个ASCII字符，不含引号和反斜杠'); }
export function validateConfig(config) {
  shape(config,['keyboard','scan','lighting','indicator']);
  shape(config.keyboard,['bindings','retrigger_gap_us']);
  const bindings = config.keyboard.bindings;
  assert(Array.isArray(bindings) && bindings.length === 12,'绑定必须恰好12路');
  bindings.forEach((binding,i) => {
    shape(binding,['source_id','action']); assert(binding.source_id === i,'物理来源顺序不正确');
    const a = binding.action;
    if (a.type === 'disabled') shape(a,['type']);
    else if (a.type === 'keyboard') {
      shape(a,['type','usage','modifiers']); range(a.usage,4,231,true);
      assert(a.usage <= 164 || a.usage >= 224,'不支持的HID usage'); range(a.modifiers,0,255,true);
    } else { shape(a,['type','button']); assert(a.type === 'mouse_button' && [1,2,4].includes(a.button),'不支持的动作'); }
  });
  range(config.keyboard.retrigger_gap_us,0,100000,true);
  shape(config.scan,['press_debounce_us','release_debounce_us']); Object.values(config.scan).forEach(v => range(v,0,20000,true));
  const l = config.lighting;
  shape(l,['enabled','per_key','reverse','mode','brightness_level','global_color','key_colors','period_ms']);
  [l.enabled,l.per_key,l.reverse].forEach(bool);
  assert(['static','breathing','color_cycle','rainbow_wave','press'].includes(l.mode),'灯效不支持');
  range(l.brightness_level,0,100,true); range(l.period_ms,100,60000,true); rgb(l.global_color);
  assert(Array.isArray(l.key_colors) && l.key_colors.length === 10,'逐键色须按BT1～10保存'); l.key_colors.forEach(rgb);
  shape(config.indicator,['enabled','color']); bool(config.indicator.enabled); rgb(config.indicator.color);
}
function validateFields(object, fields) {
  for (const [key,,min,max,type] of fields) {
    assert(typeof object[key]==='number' && Number.isFinite(object[key]),'参数须为有限JSON数字');
    // C++ Hall阈值是binary32；合法的0.01f略小于JS的0.01，按同一精度比较。
    if(type==='f32') range(Math.fround(object[key]),Math.fround(min),Math.fround(max));
    else range(object[key],min,max,type.startsWith('u'));
  }
}
export function validateMotion(motion) {
  shape(motion,['hall','knobs','routing']); const h = motion.hall;
  shape(h,['enabled','invert',...hallFields.map(f=>f[0])]); bool(h.enabled); bool(h.invert); validateFields(h,hallFields);
  assert(h.range_min < h.range_max && h.moving_tau_us <= h.quiet_tau_us && h.exit_signal <= h.enter_signal,'滑轨上下界、滤波或信号滞回不正确');
  assert(Array.isArray(motion.knobs) && motion.knobs.length === 2,'旋钮须为两路');
  for (const k of motion.knobs) {
    shape(k,['enabled','axis','invert',...knobFields.map(f=>f[0]),'push']); bool(k.enabled); bool(k.invert); range(k.axis,0,1,true); validateFields(k,knobFields);
    shape(k.push,['enabled',...pushFields.map(f=>f[0])]); bool(k.push.enabled); validateFields(k.push,pushFields);
    assert(k.push.rotating_press_us >= k.push.press_us,'旋转按压确认不能短于普通按压');
  }
  shape(motion.routing,['idle_us','simultaneous_hall_first']); range(motion.routing.idle_us,0,100000,true); bool(motion.routing.simultaneous_hall_first);
}
export function validateProfile(p) { range(p.profile_id,1,0xffffffff,true); validName(p.name); validateConfig(p.config); validateMotion(p.motion); if(p.io4_bindings!==undefined){assert(Array.isArray(p.io4_bindings)&&p.io4_bindings.length===12,"IO4功能绑定须为十二路");p.io4_bindings.forEach(v=>range(v,0,12,true));} }
export function verifyHello(hello) {
  assert(hello.product === 'FluxRail' && hello.board === 'FluxRail RP2040 4MiB','产品或板型不匹配，停止配置');
  assert(typeof hello.serial === 'string' && hello.serial.length > 0 && typeof hello.firmware_version === 'string','握手缺少固件版本/序号');
  assert(['device_schema1','device_schema2','runtime_module1'].includes(hello.scope) && hello.usb_mode === 'NORMAL_NKRO','协议scope或USB身份不支持');
  const c = hello.capabilities;
  assert(c?.cdc_config === true && c.nkro===true && c.mouse_buttons===true && c.lighting===true && c.ram_preview===true && c.binding_sources === 12 && c.max_line_bytes === 4096 && c.max_tokens===384 && c.max_depth===12,'设备能力与客户端不兼容');
  assert(!['device_schema1','device_schema2'].includes(hello.scope) || (c.profiles === true && c.max_profiles === 12),'schema1模板能力不兼容');
  return hello;
}
