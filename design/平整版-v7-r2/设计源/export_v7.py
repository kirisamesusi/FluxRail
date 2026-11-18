"""Export the v7 delivery from CAD, including neutral parts and offline 3D pages."""
from collections import Counter
from dataclasses import replace
import base64
import csv
import gzip
import hashlib
import html
import json
import math
from pathlib import Path
import re
import shutil
import struct
from urllib.parse import quote

import cadquery as cq
import ezdxf
import markdown

import build_v7 as m

R=m.ROOT
SOURCE=m.SOURCE
BASE=SOURCE/'baseline'

# ID, display name, obtain, order/production dimensions, notes
LOCAL = {
    'P_case': ('tray','底壳与 POM 制动座','打印','PETG，集成下 POM 托面及两根导柱','按 r2 重打'),
    'P_board_keeper': ('keeper','退板挡条','复用或打印','v6 挡条 PETG','与 v6 相同'),
    'P_rotor_hub': ('hub','磁铁杯与浮动盘驱动键','打印','PETG，孔 Ø6.10，肩 Ø6.85','两驱动键带动黄铜盘'),
    'P_carrier_left': ('carrier-l','固定座 左半','打印','PETG；MR 座 Ø10.08、夹层高 5','下外圈双肩、弹簧座和悬挂集成'),
    'P_carrier_right': ('carrier-r','固定座 右半','打印','PETG；MR 座 Ø10.08、夹层高 5','对称半座，按独立 STL'),
    'P_cup': ('cup','固定上轴承杯','打印','PETG，孔 Ø13.08，外导向 Ø17.1','法兰由固定座与封盖夹住'),
    'P_top_clamp': ('top-clamp','一体肩上夹箍','打印','PETG；孔 6.10、肩 Ø7.25','配 M2×8 杯头和 AF4 螺母'),
    'C_pcb': ('fluxdial','FluxDial 小板','复用','15×18×1.6；现有装联','小板共 1 片/旋钮'),
    'C_sensor': ('mt6701','MT6701 包络','板上参考','沿用 FluxDial 装联','不重复买'),
    'C_fpc': ('fpc','排线座包络','板上参考','沿用 FluxDial 装联','不重复买；软排线按实物维护余量选'),
    'C_capacitor': ('capacitor','电容包络','板上参考','沿用 FluxDial 装联','不重复买'),
    'C_shaft': ('shaft','精密光轴','复用或采购','低磁性精密不锈钢 Ø6×24','仅截长去毛刺；到货实测轴径和磁性'),
    'C_magnet': ('magnet','编码器磁铁','复用或采购','Ø6×2.5 径向充磁','不是轴向充磁；实测磁隙与场状态'),
    'C_cap': ('cap','成品旋钮帽','复用或采购','Ø30×25 孔 6，侧面顶丝','实测内腔、孔深、顶丝高度；保留 ≥5 mm 夹轴目标'),
    'C_cap_set': ('cap-set','帽随附顶丝','随帽附带','侧顶丝，CAD 暂按 M3 包络','跟帽配套，不按模型螺纹单买'),
    'C_lower': ('bearing-mr','下轴承 MR106ZZ','复用或采购','6×10×3，双铁盖，无外法兰','每旋钮 1 只；两端面环尺寸验收'),
    'C_upper': ('bearing-686','上轴承 686-2RS','采购','6×13×5，低 / 中阻尼同规格','历史截图 ¥8.50/只；选择一档装配；可另买另一档对比'),
    'W_pom': ('pom','开缺口的白色 POM 垫片','采购','毛坯内 15 × 外 20 × 厚 1','按加工图锉两处防转缺口，每只两片'),
    'W_wave': ('wave','304 三波峰波簧','采购','内 14 × 外 21 × 片厚 0.4','历史截图 ¥6.82/50 只；自由高 2.0 仅 CAD 假设'),
    'A_lid': ('lid','v7 r2 铝封盖','加工','38×38×2 铝，新增 1 个 M2×0.4 通牙','r1 铝盖可补钻攻，中心台阶沿用'),
    'S_keeper': ('keeper-m2','挡条沉头 M2×3','复用或采购','M2×3 沉头','旧规格；小扭矩装入底壳导孔'),
    'S_clamp': ('clamp-m2','夹轴杯头 M2×8','复用','M2×8 内六角杯头','上夹箍用 1 颗'),
    'N_clamp': ('clamp-nut','夹轴六角螺母 M2','复用或采购','M2×0.4，AF4，厚 1.6','顶夹箍和磁铁杯各 1'),
    'T_hanger': ('hanger','双通圆铜柱','复用或采购','M2 母牙双通，Ø3.2×5','必须实际核对 Ø3.2；金属限距'),
    'W_hanger': ('m2-washer','M2 平垫','复用或采购','2.2×5×0.3','仅铜柱下端各 1 片；每套省去上端 4 片'),
    'N_hanger': ('hanger-bottom','铜柱下端杯头 M2×3','复用或采购','M2×3 杯头','下端向上装，别换沉头'),
    'S_lid': ('hanger-top','铜柱上端沉头 M2×4','复用或采购','M2×4 沉头','顶部从铝封盖装入'),
    'I_case': ('case-insert','底壳热熔嵌件','复用或采购','M2×3 铜嵌件','只固定电子托盘；避免压进托盘螺丝过孔'),
    'S_case': ('case-m2','底壳沉头 M2×6','复用或采购','M2×6 沉头','中圈托盘螺丝 4'),
    'S_service': ('service-m2','外圈维修沉头 M2×6','复用或采购','M2×6 沉头','最外侧整套拆取螺丝 4'),
    'S_hub_grub': ('hub-grub','下磁铁杯紧定螺钉','采购','M2×3 平端内六角紧定螺钉','配可回收的 AF4 钢螺母'),
    'S_brake_adjust': ('brake-adjust','外部调阻尼螺丝','采购','M2×6 内六角杯头','每只 1 颗，头下长 6 mm'),
    'P_pressure_yoke': ('brake-yoke','导向压环','打印','PETG，双导柱，调节行程 0.60','先试直线滑动和受压偏斜'),
    'P_pom_saddle': ('pom-saddle','上 POM 防转托环','打印','PETG，环基体 0.75 厚','键朝下夹住上 POM'),
    'C_brake_disc': ('brake-disc','黄铜浮动摩擦盘','加工','Ø20.4×1 黄铜片，双驱动槽','双面平整去毛刺，可在驱动键上轴向滑动'),
}

V6_SOURCE = {
    'tray': 'r2 集成制动座，v6 和 r1 底壳均换新版。',
    'keeper': '直接复用 v6 FluxDial 退板挡条。',
    'hub': '重打带双驱动键的 r2 磁铁杯。',
    'carrier-l': '重打 r2 左固定座，桥位及 MR106ZZ 高度已变。',
    'carrier-r': '重打 r2 右固定座，桥位及 MR106ZZ 高度已变。',
    'cup': '重打 r2 固定杯，r1 浮动杯停用。',
    'top-clamp': '替换 v6 帽内止挡环。初版 v7 夹箍可用。',
    'fluxdial': '直接复用已装联的 v6 FluxDial 小板。',
    'mt6701': '随 v6 FluxDial 小板。',
    'fpc': '插座随 v6 FluxDial 小板。现有排线长度按实装核对。',
    'capacitor': '随 v6 FluxDial 小板。',
    'shaft': '直接复用原 Ø6×24 光轴。',
    'magnet': '用 v6 同规格备件，或拆下完好磁铁复用。胶接件拆不下时才补买。',
    'cap': '直接复用原 Ø30×25 孔 6 顶丝帽。',
    'cap-set': '随 v6 旋钮帽复用。',
    'bearing-mr': '从 v6 每套两只 MR106ZZ 中取一只。',
    'bearing-686': 'v6 无对应件。每只旋钮买一只，低／中阻尼任选。',
    'pom': 'v6 毛毡不复用。每只旋钮买两片。',
    'wave': 'v6 无对应件。每只旋钮买一片。',
    'lid': 'v6 升级按 r2 图加工。r1 铝盖按调节孔图补钻攻。',
    'keeper-m2': '直接复用 v6 挡条螺丝，每只两颗。',
    'clamp-m2': '复用 v6 的原夹轴杯头，移到上夹箍。',
    'clamp-nut': '沿用原夹轴螺母，另一颗从 v6 摩擦盘的 AF4 螺母取。',
    'hanger': '直接复用 v6 双通铜柱，贴铝封盖底面。',
    'm2-washer': 'v6 八片中只取四片，均装在铜柱下端。',
    'hanger-bottom': '直接复用 v6 铜柱下端杯头，实测两端螺钉不互顶。',
    'hanger-top': '直接复用 v6 铜柱上端沉头，实测两端螺钉不互顶。',
    'case-insert': '优先用 v6 同规格库存。旧件已热熔时，完整取出才复用。',
    'case-m2': '直接复用 v6 托盘螺丝，每只四颗。',
    'service-m2': '直接复用 v6 外圈维修螺丝，每只四颗。',
    'hub-grub': 'v6 无此规格。每只需要 1 颗，有库存可用。',
    'brake-adjust': 'v6 阻尼螺丝长 8 mm。r2 用头下长 6 mm 的杯头。',
    'brake-yoke': 'r2 新打印件，每只 1 件。',
    'pom-saddle': 'r2 新打印件，每只 1 件。',
    'brake-disc': 'v6 摩擦盘厚 1.5 mm 且孔不同，按 r2 图做新盘。',
}


def purchase_action(key, obtain):
    if key in ('case-insert','magnet'):
        return '缺件才补买'
    return {'采购':'新购','加工':'按 v7 图加工','打印':'打印新版',
            '板上参考':'随小板复用','随帽附带':'随帽复用'}.get(obtain,'直接复用 v6')


def put(name, data):
    p=R/name
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(data,encoding='utf-8')


def dump(name,data):
    put(name,json.dumps(data,ensure_ascii=False,indent=2)+'\n')


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f,'sha256').hexdigest()


def entry(part_id):
    if part_id in LOCAL: return LOCAL[part_id]
    if part_id in ('S_hub','S_top'): return LOCAL['S_clamp']
    if part_id in ('N_hub','N_top'): return LOCAL['N_clamp']
    for key in LOCAL:
        if part_id.startswith(key+'_'): return LOCAL[key]
    raise KeyError(part_id)


def normalized(p):
    s=p.shape
    if p.id in ('P_cup','P_top_clamp','P_pressure_yoke','P_pom_saddle'):
        s=s.rotate((0,0,0),(1,0,0),180)
    bb=s.BoundingBox()
    return s.translate((-bb.xmin,-bb.ymin,-bb.zmin))


def stl_check(path, size):
    buf=path.read_bytes()
    count=struct.unpack_from('<I',buf,80)[0]
    assert count and len(buf)==84+count*50
    bounds=[[math.inf]*3,[-math.inf]*3]
    edges=Counter()
    for _,_,_,*verts,flag in struct.iter_unpack('<12fH',buf[84:]):
        vs=[tuple(round(x,5) for x in verts[i:i+3]) for i in (0,3,6)]
        for v in vs:
            for d,x in enumerate(v):
                bounds[0][d]=min(x,bounds[0][d]); bounds[1][d]=max(x,bounds[1][d])
        if len(set(vs))!=3: continue
        for i,j in ((0,1),(1,2),(2,0)):
            edges[tuple(sorted((vs[i],vs[j])))]+=1
    assert max(abs(x) for x in bounds[0])<.035,(path,bounds)
    assert max(abs(x-y) for x,y in zip(bounds[1],size))<.045,(path,bounds,size)
    nonmanifold=sum(n!=2 for n in edges.values())
    return dict(triangles=count,nonmanifoldEdges=nonmanifold)


def independent(knob, full):
    old_rows={r['key']:r for r in json.loads((BASE/'部件清单.json').read_text(encoding='utf-8-sig'))}
    old_map={r['id']:r['partKey'] for r in json.loads((BASE/'整机实例映射.json').read_text(encoding='utf-8-sig'))}
    models={}
    mapping={}
    # A bearing is ordered/exported as one item. Its three labelled solids stay
    # separate in the assembly so CAD can verify shield and ring contacts.
    for p in knob:
        key,name,obtain,spec,note=entry(p.id)
        if key.startswith('bearing-'):
            if not p.id.endswith('_outer'): continue
            prefix=p.id[:-6]
            p=replace(p,shape=cq.Compound.makeCompound([q.shape for q in knob if q.id.startswith(prefix+'_')]))
        if key not in models:
            models[key]=dict(id=key,name=name,obtain=purchase_action(key,obtain),spec=spec,note=note,v6Source=V6_SOURCE[key],
                             module='旋钮',printable=p.printable,qtyKnob=0,qtyINF=0,qtyOngeki=0,part=p)
        models[key]['qtyKnob']+=1
    count_seen=set()
    instances=[]
    for p in full:
        if p.id.startswith(('K1_','K2_')):
            key=entry(p.id[3:])[0]
            unit=p.id[:3]
        else:
            key='host-'+old_map[p.id]
            unit=p.id
            if key not in models:
                old=old_rows[old_map[p.id]]
                models[key]=dict(id=key,name=old['name'],obtain='直接复用 v6',
                                 v6Source='沿用同名 v6 整机件，按所用滑块版本选配。',
                                 spec='沿用 v6 整机件',note=old['note'],module=old['module'],
                                 printable=p.printable,qtyKnob=0,qtyINF=0,qtyOngeki=0,part=p)
        mapping[p.id]=key
        variant='INF' if p.id=='Q_cap_inf' else '音击' if p.id=='Q_cap_ongeki' else '通用'
        instances.append(dict(id=p.id,partKey=key,name=p.name,variant=variant))
        seen=(key,unit) if key.startswith('bearing-') else (key,p.id)
        if seen in count_seen: continue
        count_seen.add(seen)
        if variant!='音击': models[key]['qtyINF']+=1
        if variant!='INF': models[key]['qtyOngeki']+=1

    for key,row in models.items():
        p=row.pop('part'); s=normalized(p)
        bb=s.BoundingBox()
        size=[round(d,5) for d in (bb.xlen,bb.ylen,bb.zlen)]
        clean=re.sub(r'[<>:"/\\|?*]','-',row['name']).strip(' .')
        stem=f'{key} {clean}'
        step=Path('独立零件')/row['module']/(stem+'.step')
        stl=Path('打印STL' if p.printable else '独立零件')/row['module']/(stem+'.stl')
        for path in (step,stl): (R/path).parent.mkdir(parents=True,exist_ok=True)
        cq.exporters.export(s,str(R/step))
        cq.exporters.export(s,str(R/stl),tolerance=.018,angularTolerance=.12)
        read=cq.importers.importStep(str(R/step)).val()
        assert read.isValid() and len(read.Solids())==len(s.Solids()),(key,len(read.Solids()))
        assert math.isclose(read.Volume(),s.Volume(),rel_tol=3e-4,abs_tol=.003),(key,read.Volume(),s.Volume())
        rb=read.BoundingBox()
        assert max(abs(v) for v in [rb.xmin,rb.ymin,rb.zmin])<.02,(key,rb)
        assert max(abs(x-y) for x,y in zip((rb.xlen,rb.ylen,rb.zlen),size))<.02,(key,rb,size)
        mesh=stl_check(R/stl,size)
        assert not mesh['nonmanifoldEdges'],(key,mesh)
        row.update(sizeMM=size,step=step.as_posix(),stl=stl.as_posix(),solids=len(s.Solids()),
                   stepSHA256=sha(R/step),stlSHA256=sha(R/stl),mesh=mesh)
    dump('部件清单.json',list(models.values()))
    dump('整机实例映射.json',instances)
    return models,mapping


def write_csv(path,header,rows):
    p=R/path; p.parent.mkdir(parents=True,exist_ok=True)
    with p.open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.writer(f); w.writerow(header); w.writerows(rows)


def bom(models):
    write_csv('BOM.csv',['编号','零件','范围','从 v6 升级','单旋钮','双旋钮 INF','双旋钮 音击','规格','说明','v6 来源 / 补买条件'],
              [[r[k] for k in ('id','name','module','obtain','qtyKnob','qtyINF','qtyOngeki','spec','note','v6Source')]
               for r in models.values()])
    write_csv('打印清单.csv',['编号','名称','单旋钮','整机 INF','整机 音击','STL','建议朝向'],
              [[r[k] for k in ('id','name','qtyKnob','qtyINF','qtyOngeki','stl')]+[{
                  'tray':'壳底贴床，给 POM 座背面局部支撑','hub':'磁铁杯口朝下，给驱动键和杯顶局部支撑',
                  'cup':'Ø13.08 轴承装入口贴床；给法兰外缘加局部支撑',
                  'top-clamp':'大端平面贴床，内圈肩向上','carrier-l':'悬挂耳贴床，桥和中座加局部支撑',
                  'carrier-r':'悬挂耳贴床，桥和中座加局部支撑','keeper':'挡条底面贴床',
                  'brake-yoke':'按 STL，环和伸出臂局部支撑','pom-saddle':'平面贴床，两键朝上',
              }.get(r['id'],'延用 v6 方向；确认装配面')] for r in models.values() if r['printable']])


def download_assets(models):
    # A file:// page cannot fetch local binary files or rely on download on
    # a file:// link. Load a compressed classic-script payload on first use.
    files={}
    for row in models.values():
        for k in ('step','stl'):
            rel=row[k]
            files[rel]=base64.b64encode(gzip.compress((R/rel).read_bytes(),compresslevel=7,mtime=0)).decode('ascii')
    put('零件数据.js','globalThis.FluxRailPartFiles='+json.dumps(files,ensure_ascii=False,separators=(',',':'))+';\n')
    shutil.copyfile(SOURCE/'part_download.js',R/'单件保存.js')


def drawings():
    path=Path('旋钮/加工图')
    (R/path).mkdir(parents=True,exist_ok=True)
    def dxf(which):
        d=ezdxf.new('R2010',units=4)
        for name,color in [('THROUGH',7),('TOP_CS_90',1),('BOTTOM_POCKET_1P30',5),('DRILL_TAP_M2',3)]:
            d.layers.new(name,dxfattribs={'color':color})
        ms=d.modelspace()
        if which=='through':
            ms.add_lwpolyline([(-19,-19),(19,-19),(19,19),(-19,19)],close=True,dxfattribs={'layer':'THROUGH'})
            ms.add_circle((0,0),m.GUIDE_BORE/2,dxfattribs={'layer':'THROUGH'})
        for x,y in m.MOUNT+m.HANGERS+m.CASE_MOUNT:
            ms.add_circle((x,y),1.2 if which=='through' else 2.1,
                          dxfattribs={'layer':'THROUGH' if which=='through' else 'TOP_CS_90'})
        if which!='through':
            ms.add_circle((0,0),m.POCKET_OD/2,dxfattribs={'layer':'BOTTOM_POCKET_1P30'})
            ms.add_circle((m.ADJ_X,m.ADJ_Y),.8,dxfattribs={'layer':'DRILL_TAP_M2'})
        file=R/path/('r2封盖-01-通切.dxf' if which=='through' else 'r2封盖-02-二次加工.dxf')
        d.saveas(file)
        read=ezdxf.readfile(file)
        assert read.units==4 and len(read.modelspace())==14
    dxf('through');dxf('secondary')
    write_csv(path/'r2封盖孔表.csv',['类型','X mm','Y mm','通孔','上表面','下表面'],
              [['中心',0,0,f'Ø{m.GUIDE_BORE:.2f}','去毛刺',f'Ø{m.POCKET_OD:.2f} 平底深 {m.POCKET_ROOF-m.LID_Z:.2f}'],
               ['调阻尼',m.ADJ_X,m.ADJ_Y,'Ø1.60 钻底孔','切削丝锥 M2×0.4 通牙','去毛刺']]+
              [[label,x,y,'Ø2.40','90° M2 沉头；模型口 Ø4.20，试螺钉配齐平','保持通孔'] for label,xy in
               [('悬挂',m.HANGERS),('底壳',m.CASE_MOUNT),('外圈',m.MOUNT)] for x,y in xy])
    for name,shape in [('r2黄铜摩擦盘',m.disk()),('r2-POM缺口',m.pom(m.POM_Z))]:
        output=R/path/(name+'.dxf')
        wires=cq.Workplane('XY').newObject([shape]).faces('>Z').wires()
        cq.exporters.exportDXF(wires,str(output))
        d=ezdxf.readfile(output)
        assert d.units==4 and len(d.modelspace())>=5
    out=R/'整机/复用加工图'
    out.mkdir(parents=True,exist_ok=True)
    for f in (BASE/'v6-machine-drawings').iterdir(): shutil.copyfile(f,out/f.name)


def explode(p):
    if p.id.startswith('K1_'): delta=(-37,24,35)
    elif p.id.startswith('K2_'): delta=(37,24,35)
    else: delta=(0,0,0)
    return tuple(a+b for a,b in zip(p.ex,delta))


def export_assembly(knob,inf,ongeki):
    rows=[]
    for rel,parts,ex in [('旋钮/旋钮总成.step',knob,False),('旋钮/旋钮爆炸.step',knob,True),
                         ('整机/整机-INF.step',inf,False),('整机/整机-音击.step',ongeki,False),
                         ('整机/整机-INF-爆炸.step',[replace(p,ex=explode(p)) for p in inf],True),
                         ('整机/整机-音击-爆炸.step',[replace(p,ex=explode(p)) for p in ongeki],True)]:
        path=R/rel; path.parent.mkdir(parents=True,exist_ok=True)
        rows.append(m.B.save_step(parts,path,ex))
        print('ASSEMBLY',rel,len(parts),flush=True)
    dump('验证/STEP装配.json',rows)


def viewer(name, parts, models, mapping, machine=False):
    engine=BASE/'knob-no-push-20261008/source-baseline/slider-quick-release/vendor/three-0.160.0'
    three=(engine/'three.module.js').read_text(encoding='utf-8')
    orbit=(engine/'OrbitControls.js').read_text(encoding='utf-8')
    # The offline page has no external imports or network requirements.
    ex=re.search(r'export \{([^}]+)\};\s*$',three)
    assert ex
    exports={s.strip().split(' as ')[-1]:s.strip().split(' as ')[0] for s in ex[1].split(',')}
    im=re.search(r'import \{([^}]+)\} from \'three\';',orbit,re.S)
    assert im
    orbit=orbit[:im.start()]+orbit[im.end():]
    orbit=re.sub(r'export \{ OrbitControls \};?','',orbit)
    # Isolate identifiers in each vendor source to prevent duplicate consts.
    required={'Scene','PerspectiveCamera','WebGLRenderer','HemisphereLight','DirectionalLight',
              'Mesh','MeshStandardMaterial','BufferGeometry','Float32BufferAttribute',
              'Group','Box3','Vector3','Color','DoubleSide','Raycaster','Vector2','SRGBColorSpace'}
    required|={s.strip() for s in im[1].split(',') if s.strip()}
    lib='const THREE=(()=>{\n'+three[:ex.start()]+'\nreturn {'+','.join(f'{k}:{exports[k]}' for k in sorted(required))+'};})();\n'
    lib+='const Orbit=(()=>{const {'+','.join(s.strip() for s in im[1].split(',') if s.strip())+'}=THREE;\n'+orbit+'\nreturn OrbitControls;})();'
    data=[]
    for p in parts:
        row=m.B.mesh(p)
        row['key']=mapping[p.id] if machine else entry(p.id)[0]
        row['rgb']=m.B.COLORS[p.category]
        row['ex']=explode(p) if machine else p.ex
        row['variant']='INF' if p.id=='Q_cap_inf' else '音击' if p.id=='Q_cap_ongeki' else ''
        data.append({k:row[k] for k in ('id','name','v','f','key','rgb','ex','variant','group')})
    cat={k:{x:r[x] for x in ('name','step','stl','printable','note')} for k,r in models.items() if k in {p['key'] for p in data}}
    title='整机 · 左右旋钮与滑轨' if machine else '旋钮 · 总成与爆炸'
    template=(SOURCE/'viewer.html').read_text(encoding='utf-8')
    raw=json.dumps(dict(parts=data,files=cat,machine=machine),ensure_ascii=False,separators=(',',':')).replace('</','<\\/')
    put(name,template.replace('__TITLE__',title).replace('__LIBRARY__',lib).replace('__DATA__',raw))


CSS="""body{margin:0;background:#f3f5f6;color:#1a2730;font:16px/1.65 system-ui,'Microsoft YaHei',sans-serif}main{max-width:1120px;margin:auto;padding:30px}
a{color:#086477}h1{line-height:1.18}h2{margin-top:1.6em}nav{display:flex;gap:16px;flex-wrap:wrap}
table{border-collapse:collapse;width:100%}th,td{border-bottom:1px solid #d6e0e4;padding:9px;text-align:left}th{background:#e0edf0}td{vertical-align:top}
section{background:white;border:1px solid #d4dfe3;border-radius:14px;padding:22px;margin:20px 0}code{background:#edf2f4;padding:1px 4px}img{max-width:100%}.muted{color:#4e616c}
@media(max-width:750px){main{padding:16px}table{font-size:13px;display:block;overflow:auto}}"""


def page(file,title,body):
    put(file,f'<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{title}</title><style>{CSS}</style><main><nav><a href="index.html">v7 入口</a><a href="旋钮模型.html">旋钮 3D</a><a href="整机模型.html">整机 3D</a><a href="单件下载.html">单件下载</a><a href="采购说明.html">采购</a><a href="打印装配.html">打印与装配</a></nav>{body}</main>')


def pages(models):
    for name in ('采购说明','打印装配'):
        body=markdown.markdown((R/(name+'.md')).read_text(encoding='utf-8'),extensions=['tables','fenced_code'])
        page(name+'.html',name,body)
    rows=[]
    for r in models.values():
        step=quote(r['step']); stl=quote(r['stl'])
        purpose='打印 STL' if r['printable'] else '参考 STL'
        rows.append(f"<tr><td>{html.escape(r['id'])}</td><td>{html.escape(r['name'])}</td><td>{html.escape(r['obtain'])}</td><td>{r['qtyKnob']} / {r['qtyINF']} / {r['qtyOngeki']}</td><td><a data-part-file href=\"{step}\" download>STEP</a> · <a data-part-file href=\"{stl}\" download>{purpose}</a></td></tr>")
    page('单件下载.html','单件下载','<h1>独立零件</h1><p>数量为单旋钮 / INF 整机 / 音击整机。参考 STL 不用于打印。轴承独立 STEP 将内圈、外圈和盖合并为同一采购件。</p><table><thead><tr><th>编号</th><th>零件</th><th>取得</th><th>数量</th><th>下载</th></tr></thead><tbody>'+''.join(rows)+'</tbody></table><script src="单件保存.js"></script>')
    page('index.html','FluxRail · 平整旋钮 v7 r2',
         '<h1>FluxRail 平整旋钮 v7 r2</h1><p class="muted">2026-11-06 · 外部可调 POM 阻尼试装版 · 毫米 · 无 PUSH</p>'
         '<section><h2>先打开模型</h2><p><a href="旋钮模型.html">旋钮 3D / 爆炸 / 单件</a> · <a href="整机模型.html">整机 3D / INF 与音击</a> · <a href="单件下载.html">单件 STEP 与 STL</a></p>'
         '<p><a href="旋钮/旋钮总成.step">旋钮 STEP</a> · <a href="旋钮/旋钮爆炸.step">旋钮爆炸 STEP</a> · <a href="整机/整机-INF.step">整机 INF STEP</a> · <a href="整机/整机-音击.step">整机音击 STEP</a> · <a href="整机/整机-INF-爆炸.step">INF 爆炸 STEP</a> · <a href="整机/整机-音击-爆炸.step">音击爆炸 STEP</a></p></section>'
         '<section><h2>试装和加工</h2><p><a href="采购说明.html">采购规格</a> · <a href="打印装配.html">打印、装拆与到货复查</a> · <a href="BOM.xlsx">BOM Excel</a> · <a href="BOM.csv">BOM CSV</a> · <a href="打印清单.csv">打印清单</a></p>'
         '<p><a href="旋钮/加工图/r2尺寸图.svg">加工尺寸图</a> · <a href="旋钮/加工图/r2封盖-01-通切.dxf">通切 DXF</a> · <a href="旋钮/加工图/r2封盖-02-二次加工.dxf">钻攻及铣削 DXF</a> · <a href="旋钮/加工图/r2封盖孔表.csv">孔表</a> · <a href="旋钮/加工图/r2黄铜摩擦盘.dxf">黄铜盘 DXF</a> · <a href="旋钮/加工图/r2-POM缺口.dxf">POM 缺口 DXF</a></p></section>'
         '<section><h2>验证与源文件</h2><p><a href="README.md">完整交付说明</a> · <a href="验证/README.md">检查范围</a> · <a href="参数.json">参数</a> · <a href="设计源/重建.md">重建</a> · <a href="续做.md">续做记录</a></p>'
         '<p class="muted">波簧自由高度 2.0 mm 是绘图估算。到货后先量零件，再按说明试装。</p></section>')


def main():
    import argparse
    ap=argparse.ArgumentParser()
    ap.add_argument('target',choices=['all','models','pages','assemblies'],nargs='?',default='all')
    mode=ap.parse_args().target
    if mode=='pages':
        models={r['id']:r for r in json.loads((R/'部件清单.json').read_text(encoding='utf-8'))}
        if not (R/'零件数据.js').exists(): download_assets(models)
        pages(models)
        return
    knob=m.knob()
    inf=m.full()
    ongeki=m.full('ongeki')
    if mode in ('all','assemblies'):
        export_assembly(knob,inf,ongeki)
    if mode=='assemblies': return
    both=inf+[p for p in ongeki if p.id not in {q.id for q in inf}]
    models,mapping=independent(knob,both)
    dump('参数.json',m.PARAMS)
    bom(models)
    drawings()
    download_assets(models)
    viewer('旋钮模型.html',knob,models,mapping)
    viewer('整机模型.html',both,models,mapping,True)
    dump('验证/独立导出.json',dict(catalog=len(models),printable=sum(r['printable'] for r in models.values()),
                                  machineViewer=len(both),knobSolids=len(knob),watertight=True,stepReadback=True))
    if (R/'打印装配.md').exists(): pages(models)
    print('PARTS',len(models),'MODEL',len(knob),len(both),flush=True)


if __name__=='__main__':
    main()
