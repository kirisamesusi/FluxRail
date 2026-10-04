/** 只检查RP2040 UF2结构/family/新板4MiB程序地址范围；这些检查不能证明
 * 文件使用了本板GPIO/USB/依赖。必须使用可信的FluxRail新板发布文件。
 * 这里没有上传通道，进入ROM也不代表升级成功。
 */
import { assert } from '../config.js';
export function validateUf2(bytes) {
  assert(bytes.length>0 && bytes.length%512===0 && bytes.length<=8*1024*1024,'UF2文件大小或块对齐无效');
  const v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);const blocks=bytes.length/512,seen=new Set(),ranges=[];
  for(let at=0;at<bytes.length;at+=512){
    const u=(off)=>v.getUint32(at+off,true);
    assert(u(0)===0x0a324655&&u(4)===0x9e5d5157&&u(508)===0x0ab16f30,'UF2 magic无效');
    assert(u(8)===0x2000&&u(28)===0xe48bff56,'需要带RP2040 family ID的标准Flash UF2');
    const address=u(12),size=u(16),number=u(20);
    assert(size>0&&size<=476&&size%4===0&&address%4===0,'UF2 payload或地址对齐无效');
    assert(address>=0x10000000&&address+size<=0x103fb000,'UF2超出新板程序区或触及公共配置保留区');
    assert(u(24)===blocks&&number<blocks&&!seen.has(number),'UF2块总数/序号不完整');seen.add(number);ranges.push([address,address+size]);
  }
  ranges.sort((a,b)=>a[0]-b[0]);for(let i=1;i<ranges.length;i++)assert(ranges[i][0]>=ranges[i-1][1],'UF2地址重叠');
  return {blocks,payload_bytes:ranges.reduce((n,[a,b])=>n+b-a,0),start:ranges[0][0],end:ranges.at(-1)[1]};
}
