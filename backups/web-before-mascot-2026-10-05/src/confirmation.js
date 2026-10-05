import { validName } from './config.js';
/** 原生dialog提供焦点约束、Escape取消与背景隔离，全部内容使用textContent。 */
export function mountConfirmation(dialog) {
  const form=dialog.querySelector('form'),title=dialog.querySelector('[data-title]'),description=dialog.querySelector('[data-description]'),nameRow=dialog.querySelector('[data-name-row]'),input=dialog.querySelector('input'),error=dialog.querySelector('[data-error]'),submit=dialog.querySelector('[data-confirm]'),cancel=dialog.querySelector('[data-cancel]');
  let pending=null,needsName=false;
  const finish=value=>{const resolve=pending;pending=null;dialog.close();resolve?.(value);};
  cancel.addEventListener('click',()=>finish(null));
  dialog.addEventListener('cancel',event=>{event.preventDefault();finish(null);});
  dialog.addEventListener('close',()=>{if(pending){const resolve=pending;pending=null;resolve(null);}});
  form.addEventListener('submit',event=>{
    event.preventDefault();
    try{if(needsName)validName(input.value);error.textContent='';finish(needsName?{name:input.value}:{});}
    catch(e){error.textContent=e.message;input.focus();}
  });
  input.addEventListener('input',()=>{error.textContent='';});
  return options=>{
    if(pending)throw new Error('请先处理当前确认窗口');
    needsName=options.name!==undefined;title.textContent=options.title;description.textContent=options.description;
    nameRow.hidden=!needsName;input.disabled=!needsName;input.required=needsName;input.value=options.name??'';
    error.textContent='';submit.textContent=options.confirmLabel??'确认';
    return new Promise(resolve=>{pending=resolve;dialog.showModal();if(needsName){input.focus();input.select();}else cancel.focus();});
  };
}
