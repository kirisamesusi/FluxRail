"""MIT. FluxRail fixed, dual-bearing knob variant. Source v2 is never overwritten."""
from pathlib import Path
from dataclasses import dataclass, replace
import importlib.util, sys, json, math, itertools, time
import cadquery as cq

ROOT = Path(__file__).resolve().parent
ENGINE = ROOT / 'source-baseline'
OUT = ROOT / 'generated'
OUT.mkdir(exist_ok=True)

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod

H = load('no_push_host', ENGINE / 'enclosure/build.py')
K = H.K
Q = load('no_push_slider', ENGINE / 'slider-quick-release/build.py')
box, cyl, ring = K.box, K.cyl, K.ring
COLORS = {**H.COLORS, **Q.COLORS, 'cosmetic': (.075,.085,.095)}
PARAMS = dict(version='knob-no-push-v3-20261008', units='mm', pushTravel=0,
              dampingTranslationZ=-4.3, capBottomLocal=22.7, shaftLength=30,
              guardOD=32.8, guardID=30.5, guardTopLocal=22.2,
              baseZ=5.0, panelTopZ=19.1, bearingSeatDiameter=10.04,
              bearingSeatOD=14.0, fixedMountScrewLength=16,
              magneticGapUnchanged=True)

def chamfered(w, corner, z0, z1):
    a=w/2; b=a-corner
    pts=[(-a,-b),(-b,-a),(b,-a),(a,-b),(a,b),(b,a),(-b,a),(-a,b)]
    return cq.Workplane('XY',origin=(0,0,z0)).polyline(pts).close().extrude(z1-z0).val()

def fixed_frame():
    # Bearing seats stay at the exact v2 positions; no sliding guides/springs.
    s=ring(14,9.4,10.5,17.3)
    s=s.fuse(chamfered(26,5.5,11.2,13.2).cut(cyl(9.4,11.19,13.21)))
    for y in (-6.2,6.2):
        s=s.fuse(box(26,4.8,10.4,14.6,0,y))
    # Four mounting ears rest on rigid base posts. Reuse existing base holes.
    for sx,sy in itertools.product((-1,1),repeat=2):
        x,y=12.2*sx,12.2*sy
        s=s.fuse(box(5.6,5.6,10.8,13.2,x,y))
        s=s.cut(cyl(2.4,10.79,13.21,x,y))
    s=s.cut(cyl(15.8,8.0,10.4)).cut(cyl(9.4,10.4,17.31))
    for z in (10.7,14.1): s=s.cut(cyl(10.04,z,z+3))
    # Preserve floating felt-plate guide interfaces, relocated with the plates.
    for sx,sy in itertools.product((-1,1),repeat=2):
        s=s.fuse(box(2,3,13.2,21.9,11*sx,5.5*sy))
    for y in (-11,11): s=s.cut(cyl(2.4,11.0,13.21,0,y))
    for y in (-6.2,6.2):
        s=s.cut(K.rotate_x_axis(cyl(2.4,-14,14)).translate((0,y,12.5)))
        cut=K.rotate_x_axis(K.cone(3.8,2.4,-12.7,.7)).fuse(K.rotate_x_axis(cyl(4.2,-13.01,-12.7)))
        s=s.cut(cut.translate((0,y,12.5)))
        s=s.cut(K.rotate_x_axis(cyl(3,9,13.01)).translate((0,y,12.5)))
    s=s.clean()
    left=s.intersect(box(40,60,0,40,-20.1,0)).clean()
    right=s.intersect(box(40,60,0,40,20.1,0)).clean()
    return left,right

def base_case():
    # Low base and rigid posts, without v2 tall cosmetic square.
    floor=box(30,30,0,.8).fuse(box(8,18,0,.8,0,9))
    body=box(30,30,.8,10.8).cut(box(27.6,27.6,.79,10.81))
    upper=box(29,29,10.8,13.7).cut(box(26.6,26.6,10.79,13.71))
    s=floor.fuse(body).fuse(upper)
    for sx,sy in itertools.product((-1,1),repeat=2):
        x,y=12.2*sx,12.2*sy
        s=s.fuse(box(5.6,5.6,.8,10.8,x,y)).cut(cyl(2.4,0,10.81,x,y))
        s=s.cut(box(5.8,5.8,10.8,13.8,x,y))
    # Exact board outline + .3 clearance, as in the delivered v2.
    r=json.loads((ENGINE/'enclosure/fluxdial-measured.json').read_text(encoding='utf-8'))
    pts=[(7.5-x,7.5-y) for x,y in r['outline_normalized']]
    pcbclear=cq.Workplane('XY',origin=(0,0,.8)).polyline(pts).close().offset2D(.3).extrude(1.9).val()
    s=s.cut(pcbclear).cut(box(8,4,1,5.5,0,14))
    for sx,sy in itertools.product((-1,1),repeat=2):s=s.cut(cyl(2.4,-.01,.81,5*sx,5*sy))
    return s.clean()

def guard():
    # Detachable four-tab sleeve: lower tabs fit the existing square panel hole.
    # Non-load-bearing tabs use .6 mm walls, isolated from the bearing support.
    tabs=box(30.6,30.6,12.1,14.4).cut(box(29.4,29.4,12.09,14.41)).intersect(cyl(32.8,12.09,14.41))
    shell=ring(32.8,30.5,14.4,22.2)
    flange=H.rounded(34.4,34.4,14.4,15.2,0,0,3.2).cut(cyl(30.5,14.39,15.21))
    s=shell.fuse(tabs).fuse(flange)
    for sx,sy in itertools.product((-1,1),repeat=2):s=s.cut(cyl(4.6,14.39,15.4,12.2*sx,12.2*sy))
    # Unchanged hub set-screw is accessed with the cap and guard removed.
    return s.clean()

def knob_local(compression=0,angle=0):
    old={p.id:p for p in K.build(compression=compression)}
    frame=fixed_frame()
    new=[]
    def add(id,name,cat,s,group='fixed',ex=(0,0,0),printable=False,ref=''):
        if group=='rotor':s=s.rotate((0,0,0),(0,0,1),angle)
        new.append(H.Part(id,name,cat,s.clean(),ex,printable,group,ref))
    skip=('P_tile','P_case','P_carrier_','P_stop_','C_spring_','I_pcb_','I_mount_','I_stop_','S_mount_','S_stop_')
    damping={'C_disc','P_lower_plate','P_upper_plate','F_lower','F_upper','I_adjust_-11','I_adjust_11','S_adjust_-11','S_adjust_11'}
    for id,p in old.items():
        if any(id.startswith(x) for x in skip):continue
        s=p.shape;name=p.name;group='fixed';ex=p.explode
        if id in damping:
            s=s.translate((0,0,-4.3));group='damping' if p.group!='rotor' else 'rotor'
        elif id in {'P_rotor_hub','I_hub','S_hub'}:
            # Flip the hub so the washer bond face remains the hub's wide face.
            if id=='P_rotor_hub':s=s.intersect(box(30,30,17.5,21.8))
            s=s.rotate((0,0,20.55),(1,0,20.55),180)
            group='rotor';name='上置摩擦盘锁轴轮毂' if id=='P_rotor_hub' else name
        elif id=='C_cap':
            s=s.translate((0,0,-5.3));group='cap'
        elif id=='C_shaft':
            s=cyl(6,8.5,38.5);name='不锈钢光轴 Ø6×30';group='rotor'
        elif p.group=='rotor':group='rotor'
        if id.startswith('S_pcb_'):
            fields=id.split('_');x=float(fields[-2])*5;y=float(fields[-1])*5
            s=K.screw(x,y,2.4,5);name='小板M2×5杯头螺丝'
        add(id,name,p.category,s,group,ex,p.printable,p.reference)
    add('P_case','无PUSH低位固定底座','cosmetic',base_case(),ex=(0,0,-25),printable=True)
    add('P_guard','可拆圆形防尘护套','cosmetic',guard(),ex=(0,0,80),printable=True)
    for id,s in zip(('P_carrier_left','P_carrier_right'),frame):
        add(id,'固定双轴承座'+('左半' if id.endswith('left') else '右半'),'petg',s,ex=(-22 if id.endswith('left') else 22,0,35),printable=True)
    # The disk bears on the upper bearing inner ring via this .4 mm spacer.
    add('P_top_spacer','摩擦盘下方内圈承力垫0.4','petg',ring(6.8,6.08,17.1,17.5),'rotor',(0,0,48),True)
    for sx,sy in itertools.product((-1,1),repeat=2):
        add(f'S_fixed_{sx}_{sy}','固定座M2×16杯头螺丝','metal',K.screw(12.2*sx,12.2*sy,13.2,16),ex=(0,0,60))
    return new

def knobs_global(compression=0,angle=0):
    result=[]
    for i,(cx,cy) in enumerate(H.KC,1):
        for p in knob_local(compression,angle):
            s=p.shape.rotate((0,0,0),(0,0,1),180).translate((cx,cy,5))
            result.append(replace(p,id=f'K{i}_{p.id}',shape=s,group=f'knob{i}_{p.group}'))
    return result

def environment():
    source,_=H.build()
    skip={'P_slider_cap_tray','M_slide_magnet',*[f'S_block_{i}' for i in range(1,5)]}
    return [p for p in source if p.id not in skip and not p.id.startswith(('K1_','K2_'))]

def full(mode='inf',slide=None,compression=0,angle=0,key=0,env=None):
    env=environment() if env is None else env
    env=[replace(p,shape=p.shape.translate((0,0,-key))) if p.group=='keys' else p for p in env]
    x=H.RX if slide is None else slide
    env=[replace(p,shape=p.shape.translate((x-H.RX,0,0))) if p.id=='M_block' else p for p in env]
    extras=[H.Part(p.id,p.name,p.category,p.shape.translate((x,H.RY,0)),p.ex,p.printable,'slider',p.reference) for p in Q.build(mode)]
    return env+extras+knobs_global(compression,angle)

def mesh(p):
    vs,fs=p.shape.tessellate(.06,.16)
    return dict(id=p.id,name=p.name,category=p.category,v=[[round(v.x,5),round(v.y,5),round(v.z,5)] for v in vs],f=fs,ex=p.ex,printable=p.printable,group=p.group,reference=p.reference)

def allowed(a,b):
    pair={a.id,b.id}
    if 'A_base' in pair:
        other=b.id if a.id=='A_base' else a.id
        return other.startswith(('S_rim_','B_pcb_col_','B_rail_lower_','K1_S_fixed_','K2_S_fixed_','K1_S_pcb_','K2_S_pcb_'))
    if 'M_block' in pair and any(i.startswith('Q_mount_') for i in pair):return True
    for prefix in ('','K1_','K2_'):
        ids={i[len(prefix):] for i in pair if i.startswith(prefix)} if prefix else pair
        if len(ids)!=2:continue
        if any((i.startswith('I_carrier_') and 'P_carrier_right' in ids) or (i=='I_hub' and 'P_rotor_hub' in ids) or (i.startswith('I_adjust_') and 'P_lower_plate' in ids) for i in ids):return True
    return False

def audit(parts):
    bounds={p.id:p.shape.BoundingBox() for p in parts};bad=[];planned=[]
    for a,b in itertools.combinations(parts,2):
        aa,bb=bounds[a.id],bounds[b.id]
        if any(min(getattr(aa,k+'max'),getattr(bb,k+'max'))-max(getattr(aa,k+'min'),getattr(bb,k+'min'))<=1e-6 for k in 'xyz'):continue
        volume=a.shape.intersect(b.shape).Volume()
        if volume>1e-5:
            row=dict(a=a.id,b=b.id,volumeMM3=round(volume,6))
            (planned if allowed(a,b) else bad).append(row)
    invalid=[p.id for p in parts if not p.shape.isValid() or len(p.shape.Solids())!=1]
    return dict(unintendedIntersections=bad,plannedThreadsOrInserts=planned,invalid=invalid)

def save_step(parts,file,exploded=False):
    a=cq.Assembly(name=file.stem.replace('-','_'))
    for p in parts:a.add(p.shape.translate(p.ex) if exploded else p.shape,name=p.id,color=cq.Color(*COLORS[p.category]))
    a.export(str(file));r=cq.importers.importStep(str(file)).val()
    assert r.isValid() and len(r.Solids())==len(parts),(file,len(r.Solids()),len(parts))
    return dict(file=file.name,solids=len(parts),valid=True)

if __name__=='__main__':
    (OUT/'parameters.json').write_text(json.dumps(PARAMS,indent=2),encoding='utf-8')
    local=knob_local();r=audit(local)
    (OUT/'local-check.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
    print('Local',len(local),'parts:',json.dumps(r),flush=True)
    if r['invalid'] or r['unintendedIntersections']:sys.exit(2)
    env=environment();records=[]
    for mode in ('inf','ongeki'):
        for slide,c,angle,key in [(H.XMIN,0,0,0),(H.RX,.075,45,4),(H.XMAX,.15,90,4)]:
            ps=full(mode,slide,c,angle,key,env);result=audit(ps)
            row=dict(mode=mode,slideX=slide,feltCompressionEach=c,angle=angle,keyTravel=key,solids=len(ps),**result);records.append(row)
            print(mode,round(slide,3),c,angle,'unexpected',len(result['unintendedIntersections']),'invalid',result['invalid'],flush=True)
            (OUT/'full-check.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
            if result['invalid'] or result['unintendedIntersections']:sys.exit(3)
    parts=[replace(p,id='K1_'+p.id) for p in local]
    checks=[save_step(parts,OUT/'旋钮完整装配.step'),save_step(parts,OUT/'旋钮爆炸装配.step',True)]
    (OUT/'knob-meshes.json').write_text(json.dumps([mesh(p) for p in parts],ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    compressed={p.id:p for p in knob_local(.15)}
    data=json.loads((OUT/'knob-meshes.json').read_text(encoding='utf-8'))
    for row in data:
        extra=mesh(compressed[row['id'].removeprefix('K1_')]);row['vCompressed']=extra['v'];row['fCompressed']=extra['f']
    (OUT/'knob-meshes.json').write_text(json.dumps(data,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    for mode,title in [('inf','整机-INF快换短帽'),('ongeki','整机-音击快换握把')]:
        ps=full(mode,env=env);checks.append(save_step(ps,OUT/(title+'.step')))
    # Both interchangeable slider caps in the full viewer, as in the v2 delivery.
    ps=full('inf',env=env);rows=[mesh(p) for p in ps]
    for row in rows:row['variant']='inf' if row['id']=='Q_cap_inf' else 'common'
    p=Q.build('ongeki')[-1];p=H.Part(p.id,p.name,p.category,p.shape.translate((H.RX,H.RY,0)),p.ex,p.printable,'slider',p.reference)
    rows.append({**mesh(p),'variant':'ongeki'})
    cglobal={p.id:p for p in knobs_global(.15)}
    for row in rows:
        if row['id'] in cglobal:
            m=mesh(cglobal[row['id']]);row['vCompressed']=m['v'];row['fCompressed']=m['f']
    (OUT/'full-meshes.json').write_text(json.dumps(rows,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    (OUT/'assembly-check.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
    print('Assembly export complete',checks,flush=True)
