// Local confirmation proposal only; no bootloader or file-transfer commands.
export function applyUF2Confirmation() {
  const q=selector=>{const node=document.querySelector(selector);if(!node)throw Error('Missing UF2 preview target: '+selector);return node;};
  const make=(tag,text='',className='')=>{const node=document.createElement(tag);node.textContent=text;if(className)node.className=className;return node;};
  q('#update .update-steps').remove();
  const entry=q('#integration-bootloader');entry.disabled=false;entry.setAttribute('aria-haspopup','dialog');entry.setAttribute('aria-controls','uf2-download-confirmation');
  q('#update .section-heading p').textContent='检查 UF2 文件和版本，点击“进入 UF2 下载模式”查看更新步骤。';
  const dialog=make('dialog','','confirmation-dialog');dialog.id='uf2-download-confirmation';
  dialog.setAttribute('aria-labelledby','uf2-download-title');dialog.setAttribute('aria-describedby','uf2-download-description');
  const title=make('h2','进入 UF2 下载模式？');title.id='uf2-download-title';
  const warning=make('p','进入后，设备会断开连接，未保存的修改会丢失。请先应用并保存要保留的设置。','uf2-warning');warning.id='uf2-download-description';
  const steps=make('ol','','uf2-dialog-steps');
  for(const text of ['确认进入后，电脑会显示 RPI-RP2 磁盘。','将适用于本设备的 UF2 文件复制到该磁盘。','更新完成后重新连接网页，核对固件版本并检查设备功能。'])steps.append(make('li',text));
  const manual=q('#manual-update-help');manual.textContent='物理进入方法：按住 BOOT 接入 USB，再松开按键。';
  const fallback=make('details','','uf2-dialog-fallback');fallback.append(make('summary','无法从网页进入时'),manual);
  const note=make('p','本页只演示确认弹窗，不会进入下载模式或更新固件。','hint');
  const actions=make('div','','toolbar dialog-actions');
  const cancel=make('button','取消');cancel.id='uf2-download-cancel';cancel.type='button';
  const confirm=make('button','确认进入','primary');confirm.id='uf2-download-confirm';confirm.type='button';
  actions.append(cancel,confirm);dialog.append(title,warning,steps,fallback,note,actions);document.body.append(dialog);
}
