// Merge only the isolated preview's three overlapping top blocks.
export function mergeHeader() {
  const header=document.querySelector('.workspace-header'),status=document.querySelector('.status-panel');
  if(!header||!status)throw Error('Missing header merge target');
  header.classList.add('unified-device-header');
  const copy=header.querySelector(':scope > div');copy.classList.add('device-header-copy');
  const title=copy.querySelector('h1');title.id='configuration-title';header.setAttribute('aria-labelledby',title.id);
  title.append(document.getElementById('demo-label'));
  copy.querySelector('.header-tag').remove();
  copy.querySelector('p').remove();
  const summary=document.getElementById('device-summary');summary.textContent='设置模板和输入方式，改好后点“应用并保存”。';copy.append(summary);
  header.append(status.querySelector('.toolbar'));
  const feedback=document.createElement('div');feedback.className='device-header-feedback';
  const notice=document.getElementById('notice');notice.textContent='尚未连接设备。连接后可读取和修改设置。';
  feedback.append(notice,status.querySelector('.state-line'));header.append(feedback);status.remove();
}
