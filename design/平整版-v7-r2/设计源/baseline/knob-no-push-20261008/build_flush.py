"""MIT. Flush-deck no-PUSH variant; 1:1 offset magnetic rotor, real OCCT solids."""
from pathlib import Path
from dataclasses import replace
import json,sys,math,itertools,time
import cadquery as cq
import build as B
H,K,Q=B.H,B.K,B.Q
box,cyl,ring=B.box,B.cyl,B.ring
ROOT=B.ROOT;OUT=ROOT/'flush';OUT.mkdir(exist_ok=True)
COLORS=B.COLORS
COLORS['aluminum']=(.70,.73,.76)
SX=math.sqrt(21.6**2-5**2);SY=-5.0
GEAR_PHASE=7.5+2*math.degrees(math.atan2(5,SX))
BOARD_ANGLE=180-math.degrees(math.atan2(12.2+SY,SX-12.2))
def board_xy(x,y):
    a=math.radians(BOARD_ANGLE)
    return (x*math.cos(a)-y*math.sin(a),x*math.sin(a)+y*math.cos(a))
P=dict(version='knob-no-push-flush-v3-20261008',units='mm',pushTravel=0,
       mainBearings='2 x MR106ZZ 6x10x3',sensorBearing='1 x MR106ZZ 6x10x3',
       shaftLength=32,capBottomZ=19.6,capTopZ=44.6,panelTopZ=19.1,
       gearModule=.9,gearTeeth=24,gearPressureAngleDeg=20,
       gearCenterDistance=21.6,gearBacklashPitch=.12,gearBottomZ=15.4,gearTopZ=16.8,
       bearingTranslation=-6.7,dampingTranslation=-11.0,
       dampingUnchangedShapes=True,magneticBoardZ=3.6,
       basePocketFloorZ=1.5,basePocketDiameter=16.2,
       secondaryAxisLocal=[-SX,SY],configuration='Disable PUSH and toggle existing invert per knob; no new firmware required.')

def gear():
    z=24;m=.9;rp=m*z/2;rb=rp*math.cos(math.radians(20));ra=rp+m;rf=rp-1.25*m
    def inv(r):
        t=math.sqrt(max(0,(r/rb)**2-1));return t-math.atan(t)
    half=math.pi/(2*z)-P['gearBacklashPitch']/(2*rp)
    a0=half+inv(rp);pts=[]
    for n in range(z):
        c=n*2*math.pi/z
        pts.append((rf*math.cos(c-a0),rf*math.sin(c-a0)))
        for i in range(13):
            r=rb+(ra-rb)*i/12;a=c-half-inv(rp)+inv(r)
            pts.append((r*math.cos(a),r*math.sin(a)))
        at=half+inv(rp)-inv(ra)
        for j in range(1,5):
            a=c-at+2*at*j/4;pts.append((ra*math.cos(a),ra*math.sin(a)))
        for i in range(11,-1,-1):
            r=rb+(ra-rb)*i/12;a=c+half+inv(rp)-inv(r)
            pts.append((r*math.cos(a),r*math.sin(a)))
        pts.append((rf*math.cos(c+a0),rf*math.sin(c+a0)))
        for j in range(1,5):
            a=c+a0+(2*math.pi/z-2*a0)*j/5;pts.append((rf*math.cos(a),rf*math.sin(a)))
    return cq.Workplane('XY',origin=(0,0,15.4)).polyline(pts).close().extrude(1.4).val()

def pcb_clearance(margin=.3,z0=3.5,z1=5.5):
    d=json.loads((B.ENGINE/'enclosure/fluxdial-measured.json').read_text(encoding='utf-8'))
    pts=[(a-SX,b+SY) for a,b in [board_xy(7.5-x,7.5-y) for x,y in d['outline_normalized']]]
    return cq.Workplane('XY',origin=(0,0,z0)).polyline(pts).close().offset2D(margin).extrude(z1-z0).val()

def frame():
    # Four axial fasteners hold the split fixed seat; PUSH clamp beams removed.
    s=ring(14,9.4,3.8,10.6)
    s=s.fuse(B.chamfered(26,5.5,5.8,7.4).cut(cyl(9.4,5.79,7.41)))
    for sx,sy in itertools.product((-1,1),repeat=2):
        x,y=12.2*sx,12.2*sy
        s=s.fuse(box(5.6,5.6,5.8,7.4,x,y)).cut(cyl(2.4,5.79,7.41,x,y))
        if (sx,sy)==(-1,-1):continue
        post=cyl(5.6,7.4,17.1,x,y)
        post=post.cut(B.chamfered(26.5,5.646,8,15.6))
        post=post.cut(cyl(4.4,7.39,10,x,y)).cut(cyl(2.4,10,14.1,x,y)).cut(cyl(3,14.1,17.11,x,y))
        s=s.fuse(post)
    for z in (4,7.4):s=s.cut(cyl(10.04,z,z+3))
    for sx,sy in itertools.product((-1,1),repeat=2):s=s.fuse(box(2,3,6.5,15.2,11*sx,5.5*sy))
    for y in (-11,11):s=s.cut(cyl(5.4,5.7,7.41,0,y))
    for sx,sy in itertools.product((-1,1),repeat=2):
        a,b=board_xy(5*sx,5*sy);s=s.cut(cyl(4.2,5.1,7.3,-SX+a,SY+b))
    s=s.clean()
    # Retain the v2 split gap; halves are located by the four mounting holes.
    return (s.intersect(box(40,60,0,40,-20.1,0)).clean(),s.intersect(box(40,60,0,40,20.1,0)).clean())

def base_case():
    s=B.chamfered(30,3,5,5.8).cut(cyl(16.2,1.4,5.81))
    sub=pcb_clearance(1,z0=2.8,z1=3.6)
    for y in (SY-9,SY+9):sub=sub.fuse(box(SX+12-14,2.4,2.8,5.8,(-SX-12-14)/2,y))
    for sx,sy in itertools.product((-1,1),repeat=2):sub=sub.cut(cyl(6,2.79,5.01,12.2*sx,12.2*sy))
    s=s.fuse(sub)
    for sx,sy in itertools.product((-1,1),repeat=2):
        x,y=12.2*sx,12.2*sy
        s=s.cut(cyl(2.4,2.79,5.81,x,y))
        a,b=board_xy(5*sx,5*sy)
        s=s.cut(cyl(2.4,2.79,3.61,-SX+a,SY+b))
        s=s.cut(cyl(4,5.19,7.3,-SX+a,SY+b))
    for y in (-11,11):s=s.cut(cyl(2.4,2.79,5.81,0,y))
    # Rear cantilever legs clear the exact main-PCB rear polygon.
    x,y=-SX+3.5,SY-9
    s=s.fuse(box(4,5.6,2.8,13.8,x,y))
    s=s.fuse(box(4,10.2,12.1,14.1,x,SY-4.5))
    seat=ring(14,9.4,11.9,15.3,-SX,SY)
    fpc=box(12.7,6.05,5.19,7.71,.007,10.058).rotate((0,0,0),(0,0,1),BOARD_ANGLE).translate((-SX,SY,0))
    s=s.fuse(seat).cut(cyl(10.04,12.1,15.31,-SX,SY)).cut(pcb_clearance()).cut(fpc).clean()
    for sx,sy in itertools.product((-1,1),repeat=2):
        a,b=board_xy(5*sx,5*sy)
        s=s.cut(cyl(4.2,5.19,7.3,-SX+a,SY+b)).cut(cyl(2.4,2.79,5.2,-SX+a,SY+b))
    solids=sorted(s.Solids(),key=lambda a:a.Volume(),reverse=True)
    assert all(a.Volume()<.05 for a in solids[1:]),'Unexpected disconnected base geometry'
    return solids[0].clean()

def lid():
    s=box(31,31,17.1,19.1).cut(cyl(6.8,17.09,19.11))
    for sx,sy in itertools.product((-1,1),repeat=2):
        if (sx,sy)!=(-1,-1):s=K.cut_cs(s,12.2*sx,12.2*sy,19.1)
    return s.clean()

def hexagon(af,z0,z1,cx=0):
    return cq.Workplane('XY',origin=(cx,0,z0)).polygon(6,af/math.cos(math.pi/6)).extrude(z1-z0).val()

def secondary_rotor():
    # Two-piece keyed rotor permits actual bearing insertion before gear fitting.
    # Bottom-open cup: the magnet can actually be inserted before PCB fitting.
    cup=cyl(8.4,10.3,13.6,-21.6).cut(cyl(6.12,10.29,13.4,-21.6))
    neck=cyl(6,13.4,17.6,-21.6).fuse(cyl(6.8,13.9,14.3,-21.6))
    neck=neck.fuse(hexagon(5,17.6,18.3,-21.6))
    s=cup.fuse(neck).cut(cyl(3,14.8,17.8,-21.6)).cut(cyl(2.4,17.8,18.31,-21.6)).cut(cyl(2,14.2,14.8,-21.6)).clean()
    s=s.cut(K.cone(2.4,3.8,18.0,.7,-21.6)).cut(cyl(2.4,17.8,18.31,-21.6))
    return s

def knob_world(compression=0,angle=0):
    old={p.id:p for p in K.build(compression=compression)};parts=[]
    def add(id,name,cat,s,group='fixed',ex=(0,0,0),printable=False,ref=''):
        if group=='rotor':s=s.rotate((0,0,0),(0,0,1),angle)
        elif group=='sensor_rotor':s=s.rotate((-21.6,0,0),(-21.6,0,1),-angle)
        if group=='sensor_rotor' or id in {'C_pcb','C_sensor','C_fpc','C_capacitor','C_sensor_bearing'} or id.startswith('S_pcb_'):s=s.translate((21.6-SX,SY,0))
        parts.append(H.Part(id,name,cat,s.clean(),ex,printable,group,ref))
    skip=('P_tile','P_case','P_carrier_','P_stop_','C_spring_','I_pcb_','I_mount_','I_stop_','S_mount_','S_stop_','I_carrier_','S_carrier_','P_magnet_cup')
    damping={'C_disc','P_lower_plate','P_upper_plate','F_lower','F_upper','I_adjust_-11','I_adjust_11','S_adjust_-11','S_adjust_11'}
    sensor={'C_pcb','C_sensor','C_fpc','C_capacitor'}
    for id,p in old.items():
        if any(id.startswith(k) for k in skip):continue
        s=p.shape;name=p.name;group='fixed'
        if id in damping:
            s=s.translate((0,0,-11.0));group='rotor' if p.group=='rotor' else 'damping'
        elif id in {'P_rotor_hub','I_hub','S_hub'}:
            if id=='P_rotor_hub':s=s.intersect(box(30,30,17.5,21.8))
            s=s.rotate((0,0,20.55),(1,0,20.55),180).translate((0,0,-6.7))
            if id=='P_rotor_hub':
                s=s.fuse(gear().cut(cyl(6.08,15.39,16.81)))
                # Preserve the same radial copper insert and set-screw access.
                s=s.cut(K.rotate_x_axis(cyl(3,3.3,13)).translate((0,0,14.75)))
                s=s.cut(K.rotate_x_axis(cyl(2.2,0,3.31)).translate((0,0,14.75)))
                name='上置锁轴轮毂及24齿主齿轮'
            group='rotor'
        elif id in sensor:s=s.rotate((0,0,0),(0,0,1),BOARD_ANGLE).translate((-21.6,0,2.8))
        elif id=='C_magnet':s=s.translate((-21.6,0,2.8));group='sensor_rotor'
        elif id=='C_cap':s=s.translate((0,0,-8.4));group='cap'
        elif id=='C_shaft':s=cyl(6,1.8,33.8);name='不锈钢光轴 Ø6×32';group='rotor'
        elif p.group=='rotor' or id.startswith('C_bearing_'):s=s.translate((0,0,-6.7));group='rotor' if p.group=='rotor' else 'fixed'
        if id.startswith('S_pcb_'):
            f=id.split('_');a,b=board_xy(float(f[-2])*5,float(f[-1])*5);x=a-21.6;y=b
            s=K.screw(x,y,5.2,4);name='旁置小板M2×4杯头螺丝'
        add(id,name,p.category,s,group,p.explode,p.printable,p.reference)
    add('P_case','无PUSH固定底座及旁置磁感支架','cosmetic',base_case(),ex=(0,0,-24),printable=True)
    for id,s in zip(('P_carrier_left','P_carrier_right'),frame()):
        add(id,'固定双轴承座'+('左半' if id.endswith('left') else '右半'),'petg',s,ex=(-22 if id.endswith('left') else 22,0,28),printable=True)
    add('P_top_spacer','摩擦盘下方内圈承力垫0.4','petg',ring(6.8,6.08,10.4,10.8),'rotor',(0,0,40),True)
    for sx,sy in itertools.product((-1,1),repeat=2):
        add(f'S_fixed_{sx}_{sy}','固定座M2×5杯头螺丝','metal',K.screw(12.2*sx,12.2*sy,7.4,5),ex=(0,0,55))
        if (sx,sy)==(-1,-1):continue
        add(f'I_lid_{sx}_{sy}','封盖M2×3铜嵌件','metal',K.insert(12.2*sx,12.2*sy,14.1),ex=(0,0,62))
        add(f'S_lid_{sx}_{sy}','封盖M2×8沉头螺丝','metal',K.screw(12.2*sx,12.2*sy,19.1,8,'cs'),ex=(0,0,85))
    add('A_lid','与顶板齐平的2mm铝封盖','aluminum',lid(),'cover',(0,0,75))
    add('C_sensor_bearing','磁感副轴MR106ZZ','metal',ring(10,6,12.1,15.1,-21.6),ex=(-25,0,45))
    add('P_sensor_rotor','径向磁铁杯及副轴键','petg',secondary_rotor().translate((0,0,-2.2)),'sensor_rotor',(-24,0,28),True)
    sg=gear().cut(hexagon(5.12,15.39,16.1).rotate((0,0,0),(0,0,1),-GEAR_PHASE))
    sg=K.cut_cs(sg,0,0,16.8).fuse(ring(6.8,6.08,15.1,15.4))
    sg=sg.rotate((0,0,0),(0,0,1),GEAR_PHASE).translate((-21.6,0,0))
    add('P_sensor_gear','24齿副齿轮及内圈肩','petg',sg,'sensor_rotor',(-24,0,65),True)
    add('I_sensor_rotor','副轴M2×3铜嵌件','metal',K.insert(-21.6,0,12.6),'sensor_rotor',(-24,0,50))
    add('S_sensor_gear','副齿轮M2×3沉头螺丝','metal',K.screw(-21.6,0,16.8,3,'cs'),'sensor_rotor',(-24,0,80))
    return parts

def knobs_global(compression=0,angle=0):
    result=[]
    for i,(cx,cy) in enumerate(H.KC,1):
        for p in knob_world(compression,angle):
            s=p.shape
            if i==2:s=s.mirror('YZ')
            s=s.rotate((0,0,0),(0,0,1),180).translate((cx,cy,0))
            result.append(replace(p,id=f'K{i}_{p.id}',shape=s,group=f'knob{i}_{p.group}'))
    return result

def environment():
    env=B.environment();out=[]
    newknobs=knobs_global()
    for p in env:
        s=p.shape
        if p.id=='A_base':
            for i,(cx,cy) in enumerate(H.KC,1):
                s=s.cut(cyl(16.2,1.5,5.01,cx,cy))
                sec=cx+SX if i==1 else cx-SX
                secy=cy-SY
                pocket=pcb_clearance(1.2,z0=2.8,z1=5.01)
                for y in (SY-9,SY+9):pocket=pocket.fuse(box(SX+12-14+.4,2.8,2.8,5.01,(-SX-12-14)/2,y))
                pocket=pocket.fuse(box(4.4,6,2.8,5.01,-SX+3.5,SY-9))
                if i==2:pocket=pocket.mirror('YZ')
                pocket=pocket.rotate((0,0,0),(0,0,1),180).translate((cx,cy,0))
                for dx,dy in itertools.product((-12.2,12.2),repeat=2):pocket=pocket.cut(cyl(5.6,2.79,5.02,cx+dx,cy+dy))
                s=s.cut(pocket)
                for a,b in itertools.product((-5,5),repeat=2):
                    dx,dy=board_xy(a,b);x,y=(sec-dx if i==1 else sec+dx),secy-dy
                    s=s.cut(cyl(1.6,1.2,2.81,x,y)).cut(K.cone(0,1.6,.7193,.4807,x,y))
        elif p.id.startswith('A_acrylic_'):
            # Exact new-module keepout, with .25 mm manufacturing allowance.
            for i,(cx,cy) in enumerate(H.KC,1):
                sec=cx+SX if i==1 else cx-SX
                s=s.cut(box(29,29,4.99,15.11,sec,cy-SY))
        elif p.id=='A_locator' or p.id=='A_foam':
            for i,(cx,cy) in enumerate(H.KC,1):
                sec=cx+SX if i==1 else cx-SX
                s=s.cut(box(29,29,11.5,17.11,sec,cy-SY))
        out.append(replace(p,shape=s.clean()))
    return out

def full(mode='inf',slide=None,compression=0,angle=0,key=0,env=None):
    env=environment() if env is None else env
    x=H.RX if slide is None else slide
    out=[]
    for p in env:
        t=(0,0,-key) if p.group=='keys' else ((x-H.RX,0,0) if p.id=='M_block' else (0,0,0))
        out.append(replace(p,shape=p.shape.translate(t)))
    extras=[H.Part(p.id,p.name,p.category,p.shape.translate((x,H.RY,0)),p.ex,p.printable,'slider',p.reference) for p in Q.build(mode)]
    return out+extras+knobs_global(compression,angle)

def allowed(a,b):
    pair={a.id,b.id}
    if 'A_base' in pair:
        other=b.id if a.id=='A_base' else a.id
        return other.startswith(('S_rim_','B_pcb_col_','B_rail_lower_','K1_S_fixed_','K2_S_fixed_','K1_S_pcb_','K2_S_pcb_'))
    if 'M_block' in pair and any(i.startswith('Q_mount_') for i in pair):return True
    for prefix in ('','K1_','K2_'):
        ids={i[len(prefix):] for i in pair if i.startswith(prefix)} if prefix else pair
        if len(ids)!=2:continue
        if any((i=='I_hub' and 'P_rotor_hub' in ids) or (i.startswith('I_adjust_') and 'P_lower_plate' in ids) or (i=='I_sensor_rotor' and 'P_sensor_rotor' in ids) or (i.startswith('I_lid_') and any(j.startswith('P_carrier_') for j in ids)) for i in ids):return True
    return False

def audit(parts):
    saved=B.allowed;B.allowed=allowed
    try:return B.audit(parts)
    finally:B.allowed=saved

def moving_audit(parts):
    # Fixed environment was checked in the first full state; subsequent states
    # only recheck pairs involving a moved key/slider/rotary mechanism.
    bounds={p.id:p.shape.BoundingBox() for p in parts};bad=[];planned=[];count=0
    moving={p.id for p in parts if p.id.startswith(('K1_','K2_','Q_')) or p.id=='M_block' or p.group=='keys'}
    for a,b in itertools.combinations(parts,2):
        if a.id not in moving and b.id not in moving:continue
        aa,bb=bounds[a.id],bounds[b.id]
        if any(min(getattr(aa,k+'max'),getattr(bb,k+'max'))-max(getattr(aa,k+'min'),getattr(bb,k+'min'))<=1e-6 for k in 'xyz'):continue
        volume=a.shape.intersect(b.shape).Volume();count+=1
        if volume>1e-5:
            row=dict(a=a.id,b=b.id,volumeMM3=round(volume,6));(planned if allowed(a,b) else bad).append(row)
    return dict(unintendedIntersections=bad,plannedThreadsOrInserts=planned,invalid=[p.id for p in parts if not p.shape.isValid() or len(p.shape.Solids())!=1],pairsCalculated=count,scope='Moving parts against all parts; static environment checked in state 1')

if __name__=='__main__':
    (OUT/'parameters.json').write_text(json.dumps(P,indent=2),encoding='utf-8')
    ps=knob_world();r=audit(ps)
    (OUT/'local-check.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
    print('LOCAL',len(ps),json.dumps(r),flush=True)
    if r['invalid'] or r['unintendedIntersections']:sys.exit(2)
    if '--local-only' in sys.argv:sys.exit(0)
    # Mesh the involute teeth over one full tooth period, not just quadrant angles.
    sweeps=[]
    for angle in range(16):
        row=audit(knob_world(.15,angle));sweeps.append(dict(angle=angle,**row))
        print('gear angle',angle,'unexpected',len(row['unintendedIntersections']),flush=True)
        if row['invalid'] or row['unintendedIntersections']:
            (OUT/'rotation-check.json').write_text(json.dumps(sweeps,indent=2),encoding='utf-8');sys.exit(3)
    (OUT/'rotation-check.json').write_text(json.dumps(sweeps,indent=2),encoding='utf-8')
    env=environment();states=[]
    for mode in ('inf','ongeki'):
        for slide,c,angle,key in [(H.XMIN,0,0,0),(H.RX,.075,45,4),(H.XMAX,.15,90,4)]:
            ps=full(mode,slide,c,angle,key,env);r=audit(ps) if not states else moving_audit(ps)
            row=dict(mode=mode,slideX=slide,feltCompressionEach=c,angle=angle,keyTravel=key,solids=len(ps),**r);states.append(row)
            (OUT/'full-check.json').write_text(json.dumps(states,indent=2),encoding='utf-8')
            print(mode,round(slide,2),'unexpected',len(r['unintendedIntersections']),'invalid',r['invalid'],flush=True)
            if r['invalid'] or r['unintendedIntersections']:sys.exit(4)
    # Standalone module origin is lowest shaft end, Z=1.8 in the device.
    local=[replace(p,id='K1_'+p.id,shape=p.shape.translate((0,0,-1.8))) for p in knob_world()]
    checks=[B.save_step(local,OUT/'旋钮完整装配.step'),B.save_step(local,OUT/'旋钮爆炸装配.step',True)]
    rows=[B.mesh(p) for p in local];comp={p.id:p for p in knob_world(.15)}
    for r in rows:
        p=comp[r['id'].removeprefix('K1_')];m=B.mesh(replace(p,shape=p.shape.translate((0,0,-1.8))))
        r['vCompressed']=m['v'];r['fCompressed']=m['f']
    (OUT/'knob-meshes.json').write_text(json.dumps(rows,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    for mode,title in [('inf','整机-INF快换短帽'),('ongeki','整机-音击快换握把')]:checks.append(B.save_step(full(mode,env=env),OUT/(title+'.step')))
    ps=full('inf',env=env);rows=[B.mesh(p) for p in ps]
    for r in rows:r['variant']='inf' if r['id']=='Q_cap_inf' else 'common'
    p=Q.build('ongeki')[-1];p=H.Part(p.id,p.name,p.category,p.shape.translate((H.RX,H.RY,0)),p.ex,p.printable,'slider',p.reference)
    rows.append({**B.mesh(p),'variant':'ongeki'})
    comp={p.id:p for p in knobs_global(.15)}
    for r in rows:
        if r['id'] in comp:
            m=B.mesh(comp[r['id']]);r['vCompressed']=m['v'];r['fCompressed']=m['f']
    (OUT/'full-meshes.json').write_text(json.dumps(rows,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    (OUT/'assembly-check.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
    print('EXPORT COMPLETE',checks,flush=True)
