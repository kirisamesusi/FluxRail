import { readFile, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { Workbook, SpreadsheetFile } from '@oai/artifact-tool';

const out = process.argv[2];
if (!out) throw new Error('Pass the v7 package root as the first argument.');
const source = JSON.parse(await readFile(join(out,'部件清单.json'),'utf8'));
const priority = {'新购':0,'缺件才补买':1,'按 v7 图加工':2,'打印新版':3,'直接复用 v6':4,'随帽复用':5,'随小板复用':6};
const rows = [...source].sort((a,b)=>(b.qtyKnob>0)-(a.qtyKnob>0) ||
  (priority[a.obtain]??9)-(priority[b.obtain]??9));

const workbook = Workbook.create();
const sheet = workbook.worksheets.add('零件清单');
sheet.showGridLines=false;
sheet.getRange('A2').values=[['FluxRail 平整旋钮 v7 r2 · 可调阻尼 BOM']];
sheet.getRange('A2').format.font={name:'Microsoft YaHei',size:18,bold:true,color:'#173743'};
sheet.getRange('A2').format.rowHeight=29;
sheet.getRange('A3').values=[['从 v6 升级两只需 686 ×2、POM ×4、波簧 ×2、外调杯头 ×2、紧定螺钉 ×2。加工件另列。']];
sheet.getRange('A3').format.font={name:'Microsoft YaHei',size:11,color:'#516978'};
sheet.getRange('A3').format.rowHeight=25;
sheet.getRange('A5:I5').values=[['零件编号','零件','从 v6 升级','单旋钮','整机 INF','整机 音击','规格','试装说明','v6 来源 / 补买条件']];
sheet.getRange('A5:I5').format={
  fill:'#204958',font:{name:'Microsoft YaHei',bold:true,color:'#FFFFFF',size:11},
  horizontalAlignment:'center',verticalAlignment:'center',rowHeight:25
};
const end = rows.length+5;
const keys=['id','name','obtain','qtyKnob','qtyINF','qtyOngeki','spec','note','v6Source'];
sheet.getRange(`A6:I${end}`).values=rows.map(r=>keys.map(k=>r[k]));
sheet.getRange(`A6:I${end}`).format.font={name:'Microsoft YaHei',size:10,color:'#172932'};
sheet.getRange(`A6:I${end}`).format.verticalAlignment='center';
sheet.getRange(`D6:F${end}`).format.horizontalAlignment='center';
sheet.getRange(`D6:F${end}`).format.numberFormat='0';
const widths=[133,225,104,71,80,90,275,330,370];
for(let i=0;i<widths.length;i++){
  const col=String.fromCharCode(65+i);
  sheet.getRange(`${col}5:${col}${end}`).format.columnWidthPx=widths[i];
}
sheet.getRange(`A6:I${end}`).format.rowHeight=25;
sheet.getRange(`G6:I${end}`).format.wrapText=true;
for(let i=0;i<rows.length;i++){
  if(i%2) sheet.getRange(`A${i+6}:I${i+6}`).format.fill='#EFF5F6';
  if(rows[i].note.length>30 || rows[i].spec.length>32 || rows[i].v6Source.length>32){
    sheet.getRange(`A${i+6}:I${i+6}`).format.rowHeight=39;
  }
}
sheet.freezePanes.freezeRows(5);
workbook.recalculate();
const actual=sheet.getRange(`A6:I${end}`).values;
if(actual.length!==rows.length || actual.some((r,i)=>r.some((v,j)=>v!==rows[i][keys[j]]))){
  throw new Error('BOM worksheet differs from the CAD part catalog.');
}
const summary=await workbook.inspect({
  kind:'table',range:'零件清单!A5:I12',include:'values',tableMaxRows:8,tableMaxCols:9
});
await writeFile(join(out,'验证','BOM-inspect.ndjson'),summary.ndjson);
const preview=await workbook.render({sheetName:'零件清单',range:'A1:I14',scale:1,format:'png'});
await writeFile(join(out,'验证','BOM预览.png'),new Uint8Array(await preview.arrayBuffer()));
await (await SpreadsheetFile.exportXlsx(workbook)).save(join(out,'BOM.xlsx'));
