/** USB HID Keyboard usage与US键盘录入。字母菜单不附带Shift；手填大写附带Shift。
 * 仅录入绑定，不监听/推断设备按键状态，也不禁止重复usage。
 */
export const keys = [];
for (let i = 0; i < 26; i++) keys.push({ usage: i + 4, label: String.fromCharCode(65 + i), code: 'Key' + String.fromCharCode(65 + i) });
for (let i = 0; i < 10; i++) keys.push({ usage: 30 + i, label: String((i + 1) % 10), code: 'Digit' + (i + 1) % 10 });
const named = [
  [40,'Enter','Enter'],[41,'Esc','Escape'],[42,'Backspace','Backspace'],[43,'Tab','Tab'],[44,'Space','Space'],
  [45,'-','Minus'],[46,'=','Equal'],[47,'[','BracketLeft'],[48,']','BracketRight'],[49,'\\','Backslash'],
  [51,';','Semicolon'],[52,"'",'Quote'],[53,'`','Backquote'],[54,',','Comma'],[55,'.','Period'],[56,'/','Slash'],
  [57,'Caps Lock','CapsLock'],[70,'Print Screen','PrintScreen'],[71,'Scroll Lock','ScrollLock'],[72,'Pause','Pause'],
  [73,'Insert','Insert'],[74,'Home','Home'],[75,'Page Up','PageUp'],[76,'Delete','Delete'],[77,'End','End'],[78,'Page Down','PageDown'],
  [79,'→','ArrowRight'],[80,'←','ArrowLeft'],[81,'↓','ArrowDown'],[82,'↑','ArrowUp'],[83,'Num Lock','NumLock'],
  [84,'Num /','NumpadDivide'],[85,'Num *','NumpadMultiply'],[86,'Num -','NumpadSubtract'],[87,'Num +','NumpadAdd'],[88,'Num Enter','NumpadEnter'],
  [99,'Num .','NumpadDecimal'],[101,'Menu','ContextMenu'],
  [224,'Left Ctrl','ControlLeft'],[225,'Left Shift','ShiftLeft'],[226,'Left Alt','AltLeft'],[227,'Left Meta','MetaLeft'],
  [228,'Right Ctrl','ControlRight'],[229,'Right Shift','ShiftRight'],[230,'Right Alt','AltRight'],[231,'Right Meta','MetaRight']
];
for (let i = 0; i < 24; i++) named.push([i < 12 ? 58 + i : 104 + i - 12, 'F' + (i + 1), 'F' + (i + 1)]);
for (let i = 0; i < 10; i++) named.push([i === 9 ? 98 : 89 + i, 'Num ' + (i + 1) % 10, 'Numpad' + (i + 1) % 10]);
keys.push(...named.map(([usage,label,code]) => ({usage,label,code})));
export function fromEvent(event) {
  if (event.repeat || event.isComposing) return null;
  const key = keys.find(k => k.code === event.code);
  if (!key) throw new Error('该键无法捕获，请从菜单选择或填写HID usage');
  // 单独录入修饰键时不重复写入自己的修饰位。
  const modifiers = key.usage >= 224 ? 0 : (event.ctrlKey ? 1 : 0) | (event.shiftKey ? 2 : 0) | (event.altKey ? 4 : 0) | (event.metaKey ? 8 : 0);
  return { type: 'keyboard', usage: key.usage, modifiers };
}
export function fromCharacter(text) {
  if ([...text].length !== 1) throw new Error('请输入一个US布局字母、数字或符号');
  if (/^[a-zA-Z]$/.test(text)) return {type:'keyboard',usage:text.toLowerCase().charCodeAt(0)-93,modifiers:/[A-Z]/.test(text)?2:0};
  const plain = '1234567890-=[]\\;\'`,./';
  const shifted = '!@#$%^&*()_+{}|:"~<>?';
  let i = plain.indexOf(text), modifiers = 0;
  if (i < 0) { i = shifted.indexOf(text); modifiers = 2; }
  if (text === ' ') return {type:'keyboard',usage:44,modifiers:0};
  if (i < 0) throw new Error('不支持该字符；布局、Caps Lock和输入法会影响最终文字');
  const usages = [30,31,32,33,34,35,36,37,38,39,45,46,47,48,49,51,52,53,54,55,56];
  return {type:'keyboard',usage:usages[i],modifiers};
}
export function actionLabel(action) {
  if (action.type === 'disabled') return '禁用';
  if (action.type === 'mouse_button') return ({1:'鼠标左键',2:'鼠标右键',4:'鼠标中键'})[action.button];
  const label = keys.find(k => k.usage === action.usage)?.label || 'HID ' + action.usage;
  return action.modifiers ? label + ' · modifiers=' + action.modifiers : label;
}
