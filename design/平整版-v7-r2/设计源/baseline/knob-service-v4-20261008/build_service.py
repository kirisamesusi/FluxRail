"""MIT. Serviceable flush cartridge candidate; mm, whole-machine underside Z0."""
from pathlib import Path
from dataclasses import replace
import sys, math, itertools, json
import cadquery as cq
from functools import lru_cache
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'knob-no-push-20261008'))
import build_flush as F
B,H,K,Q=F.B,F.H,F.K,F.Q
box,cyl,ring=F.box,F.cyl,F.ring
# Service clearance: move only the magnetic secondary axis further rearward.
F.SY=-7.6;F.SX=math.sqrt(21.6**2-F.SY**2)
F.BOARD_ANGLE=180.0
F.GEAR_PHASE=7.5+2*math.degrees(math.atan2(-F.SY,F.SX))
H.OUT[3]+=3.0
ROOT=Path(__file__).resolve().parent
OUT=ROOT/'generated';OUT.mkdir(exist_ok=True)
PARAMS={'revision':'no-push-service-cartridge-v4-20261008','units':'mm','panelTopZ':19.1,
 'cover':{'boundingSize':[61,47,2],'centerXY':[-10.5,1.1],'profile':'existing rail-window notch and rim clearance','z':[17.1,19.1]},
 'serviceOpening':{'boundingSize':[53,41.4],'centerXY':[-11,-.2],'profile':'rail notch and fixed mount ears','upperBaseOnlyZ':[2,5]},
 'baseLaminate':{'lowerMM':2,'upperMM':3,'retiredKnobMountHoles':16,'remainingBodyBlindHoles':19},
 'moduleBottom':{'size':[51.5,40,.8],'centerXY':[-10.75,-.6],'z':[2,2.8],'mainAxisOpenBayDiameter':16.2},
 'bodyRearExtensionMM':3,'sensorOffsetXY':[-F.SX,F.SY],'sensorBoardRotationDeg':180,
 'serviceMountXY':[[-38.8,-15.5],[-30,21.5],[17,-8],[17,8]],
 'bearingMain':2,'bearingSecondary':1,'bearingSpec':'MR106ZZ 6x10x3',
 'damping':'same dual felt 14.8x17.6x1 and disc annulus 6.4x18x1.8; each 0..0.15 compression',
 'discFastening':'custom 3xM2 countersunk holes at R5.25; 3 M2x4 screws and heat inserts; no disc adhesive',
 'gearTransmission':'same 24/24 external gear, center distance21.6; theoretical combined backlash0.24mm/1.273deg',
 'limitations':['CAD candidate only','printed fit/load/creep not validated','magnetic performance/backlash not measured','FPC service loop route not built']}
SC=[(r*math.cos(math.radians(a)),r*math.sin(math.radians(a))) for r,a in [(5.25,60),(5.25,180),(5.25,300)]]
MOUNT=PARAMS['serviceMountXY']
def frame_mount(a,b):return (-9.8,-12.2) if (a,b)==(-1,-1) else (12.2*a,12.2*b)
LAMINATE=[(-6,20),(-6,75),(H.W+6,20),(H.W+6,75),(100,-6),(100,H.H+16)]
PARAMS['laminateFasteners']={'count':6,'spec':'M2x4 countersunk from underside','xy':LAMINATE,'upperTap':'through M2 in 3mm layer','effectiveEngagementMM':2}
@lru_cache(maxsize=1)
def baseline():return B.environment()

def window(z0,z1,margin=0):
 s=box(53+2*margin,41.4+2*margin,z0,z1,-11,-.2)
 # Rail-side material remains fixed. Small ears carry four mounting inserts.
 s=s.cut(box(20,22,-1,80,-43,4.2))
 for x,y in MOUNT:s=s.cut(cyl(4.4,z0-.01,z1+.01,x,y))
 return s.clean()

def rail_window(z0,z1):
 cx,cy=H.KC[0]
 return H.rounded(106,29.4,z0,z1,cx-H.RX,cy-H.RY,2)

def cover_outline(z0,z1,gap=0):
 return box(61+2*gap,47+2*gap,z0,z1,-10.5,1.1).cut(rail_window(z0-.01,z1+.01)).clean()

@lru_cache(maxsize=1)
def reference_base():
 # Original helper's obsolete cantilever is deliberately replaced below.
 return B.chamfered(30,3,5,5.8).cut(cyl(16.2,4.99,5.81)).clean()
F.base_case=reference_base

def cover():
 s=cover_outline(17.1,19.1).cut(cyl(6.8,17.09,19.11))
 for x,y in [(a*12.2,b*12.2) for a,b in itertools.product((-1,1),repeat=2) if (a,b)!=(-1,-1)]+MOUNT:
  s=K.cut_cs(s,x,y,19.1)
 return s.clean()

def cassette():
 # Printable cartridge floor replaces the material removed from the body.
 bounds=box(51.5,40,0,30,-10.75,-.6)
 s=F.base_case().intersect(bounds)
 s=s.fuse(box(51.5,40,2,2.8,-10.75,-.6)).cut(cyl(16.2,1.99,2.81))
 legx=-F.SX-9
 s=s.fuse(box(4.4,5.6,2.8,14.1,legx,F.SY))
 s=s.fuse(box(11,5.6,12.1,14.1,-F.SX-4.5,F.SY)).fuse(ring(14,9.4,11.9,15.3,-F.SX,F.SY))
 s=s.cut(cyl(10.04,12.1,15.31,-F.SX,F.SY))
 for a,b in itertools.product((-1,1),repeat=2):
  x,y=frame_mount(a,b)
  s=s.fuse(cyl(5.6,2.8,5.8,x,y))
  s=s.cut(cyl(3,2.5,5.5,x,y)).cut(cyl(2.4,2.1,5.81,x,y))
  x1,y1=F.board_xy(5*a,5*b);x1-=F.SX;y1+=F.SY
  s=s.fuse(cyl(5.4,2,3.6,x1,y1))
  nutpocket=cq.Workplane('XY',origin=(x1,y1,1.99)).polygon(6,4.2/math.cos(math.pi/6)).extrude(1.62).val()
  s=s.cut(nutpocket)
 # Smooth datum lips; rim is kept below nearby PCB / gears.
 for x in (-35.9,14.4):
  s=s.fuse(box(1.2,40,2.8,3.5,x,-.6))
 for y in (-20,18.8):
  s=s.fuse(box(51.5,1.2,2.8,3.5,-10.75,y))
 s=s.cut(F.pcb_clearance(.3,z0=3.59,z1=5.5))
 for a,b in itertools.product((-1,1),repeat=2):
  x,y=F.board_xy(5*a,5*b);x-=F.SX;y+=F.SY
  s=s.cut(cyl(4.2,5.19,7.31,x,y))
 for y in (-11,11):s=s.cut(cyl(2.4,2.8,5.81,0,y))
 s=s.cut(box(20,22.4,1.99,30,-42.1,4.3))
 for x,y in MOUNT:s=s.cut(cyl(4.8,1.99,30,x,y))
 # Clear fixed main-PCB standoffs even while pulling the unit up.
 for i,(cx,cy) in enumerate(H.KC,1):
  if i!=1:continue
  x,y=H.D['pcb_holes_mm'][2][:2];s=s.cut(cyl(5,1.99,30,cx-x,cy-y))
 # Nothing may project underneath the main PCB if the cartridge is to lift out.
 cx,cy=H.KC[0];pts=[(cx-x,cy-y) for x,y in H.D['board_vertices_mm']]
 shadow=cq.Workplane('XY',origin=(0,0,1.99)).polyline(pts).close().offset2D(.25).extrude(40).val()
 s=s.cut(shadow)
 # Rear withdrawal corridor for the populated PCB, plus four straight hex-key bores.
 s=s.cut(box(15.6,48,3.59,5.5,-F.SX,-23.8))
 for a,b in itertools.product((-1,1),repeat=2):
  x,y=F.board_xy(5*a,5*b);x-=F.SX;y+=F.SY
  s=s.cut(cyl(3,5.19,17.11,x,y))
 return s.clean()

@lru_cache(maxsize=40)
def knob(compression=0,angle=0):
 ps=[]
 def add(id,name,cat,s,group='fixed',ex=(0,0,0),printable=False,reference=''):
  if group=='rotor':s=s.rotate((0,0,0),(0,0,1),angle)
  ps.append(H.Part(id,name,cat,s.clean(),ex,printable,group,reference))
 for p in F.knob_world(compression,0):
  id=p.id;s=p.shape
  if id=='P_case':s=cassette();p=replace(p,name='一体旋钮底壳及副轴支架',reference='本模块自行承载全部机构，底面Z2，主体为2mm底板+3mm激光避让层')
  elif id=='A_lid':s=cover();p=replace(p,name='齐平可抽取模块封盖61x47x2')
  elif id=='P_rotor_hub':
   # Axial heat inserts fit between the shaft bore and the unchanged felt ring.
   s=s.fuse(ring(14.4,6.08,12.6,15.4))
   for x,y in SC:s=s.cut(cyl(3,12.6,15.6,x,y))
   # Restore radial insert and tool bore after widening the hub.
   s=s.cut(K.rotate_x_axis(cyl(3,3.3,13)).translate((0,0,14.75)))
   s=s.cut(K.rotate_x_axis(cyl(2.2,0,3.31)).translate((0,0,14.75)))
   p=replace(p,name='可拆摩擦盘主齿轮及锁轴轮毂',reference='摩擦盘由3颗M2x4下装沉头螺丝固定，轮毂含3个轴向铜嵌件')
  elif id=='C_disc':
   for x,y in SC:
    cutter=cyl(2.4,10.79,12.61,x,y).fuse(cyl(4.2,10.79,11.1,x,y)).fuse(K.cone(3.8,2.4,11.1,.7,x,y))
    s=s.cut(cutter)
   p=replace(p,name='可拆加工摩擦盘18x6.4x1.8',reference='原双面接触环尺寸不变；新增3处R5.25孔，不再使用未加工标准垫片或胶接')
  elif id.startswith('S_fixed_'):
   a,b=map(int,id.split('_')[-2:]);x,y=frame_mount(a,b);s=K.screw(x,y,7.4,5);p=replace(p,name='模块双轴承座M2x5螺丝')
  elif id.startswith('S_pcb_'):
   a,b=map(int,id.split('_')[-2:]);x,y=F.board_xy(5*a,5*b);x-=F.SX;y+=F.SY
   s=K.screw(x,y,5.2,3);p=replace(p,name='模块小板M2x3螺丝')
  elif id=='C_shaft':s=cyl(6,2.3,33.8);p=replace(p,name='主光轴6x31.5',reference='底端Z2.3，与2mm下底板留0.3净空；由32mm光轴截短0.5')
  elif id=='P_carrier_left':
   s=s.fuse(box(7.4,5.6,5.8,7.4,-10.5,-12.2)).fuse(cyl(2.4,5.8,7.4,-12.2,-12.2))
   s=s.cut(cyl(2.4,5.79,7.41,-9.8,-12.2))
   for a,b in itertools.product((-1,1),repeat=2):
    x,y=F.board_xy(5*a,5*b);x-=F.SX;y+=F.SY
    s=s.cut(cyl(4.2,5.19,7.31,x,y))
    s=s.cut(cyl(3,7.3,17.11,x,y))
   fpc=next(q for q in F.knob_world(compression,0) if q.id=='C_fpc').shape.BoundingBox()
   s=s.cut(box(fpc.xlen+.4,fpc.ylen+.4,fpc.zmin-.2,fpc.zmax+.2,(fpc.xmin+fpc.xmax)/2,(fpc.ymin+fpc.ymax)/2))
  elif id=='P_upper_plate':p=replace(p,reference='卸两颗调节螺丝、松轮毂顶丝，主轮毂连盘可上取；在台面卸盘3螺丝后更换上毛毡/压板')
  if p.group=='rotor':s=s.rotate((0,0,0),(0,0,1),angle)
  elif p.group=='sensor_rotor':s=s.rotate((-F.SX,F.SY,0),(-F.SX,F.SY,1),-angle)
  ps.append(replace(p,shape=s.clean()))
 for i,(x,y) in enumerate(SC):
  add(f'I_disc_{i}','摩擦盘M2x3铜嵌件','metal',K.insert(x,y,12.6),'rotor',(x*2,y*2,48))
  screw=K.screw(0,0,0,4,'cs').rotate((0,0,0),(1,0,0),180).translate((x,y,10.8))
  add(f'S_disc_{i}','摩擦盘下装M2x4沉头螺丝','metal',screw,'rotor',(x*2,y*2,-30))
 for a,b in itertools.product((-1,1),repeat=2):
  x,y=frame_mount(a,b)
  add(f'I_fixed_{a}_{b}','底壳主座M2x3铜嵌件','metal',K.insert(x,y,2.5),ex=(a*12,b*12,-15))
  x,y=F.board_xy(5*a,5*b);x-=F.SX;y+=F.SY
  nut=cq.Workplane('XY',origin=(x,y,2)).polygon(6,4/math.cos(math.pi/6)).extrude(1.6).val().cut(cyl(2,1.99,3.61,x,y))
  add(f'N_board_{a}_{b}','小板防转M2六角螺母AF4厚1.6','metal',nut,ex=(-25,0,-15),reference='底开六角捕获槽，螺丝尾端Z2.2不触底板；无需主体短盲牙，隔离涂层/薄绝缘按实物确认')
 for i,(x,y) in enumerate(MOUNT):
  add(f'S_service_{i}','总成固定M2x6沉头螺丝','metal',K.screw(x,y,19.1,6,'cs'),group='body_mount',ex=(0,0,92))
 return ps

def global_shape(s,i,cx,cy):
 if i==2:s=s.mirror('YZ')
 return s.rotate((0,0,0),(0,0,1),180).translate((cx,cy,0))

def knobs_global(compression=0,angle=0):
 return [replace(p,id=f'K{i}_{p.id}',shape=global_shape(p.shape,i,cx,cy),group=f'knob{i}_{p.group}')
  for i,(cx,cy) in enumerate(H.KC,1) for p in knob(compression,angle)]

def environment():
 # Start from PUSH body to avoid inheriting obsolete shallow slots / blind holes.
 ps=[]
 for p in baseline():
  s=p.shape
  if p.id=='A_base':
   for cx,cy in H.KC:
    for off in (5,12.2):
     for dx,dy in itertools.product((-off,off),repeat=2):s=s.fuse(cyl(1.6,0,5,cx+dx,cy+dy))
   for i,(cx,cy) in enumerate(H.KC,1):s=s.cut(global_shape(window(2,5.01),i,cx,cy))
   lower=s.intersect(H.outer(0,2));upper=s.intersect(H.outer(2,5))
   for x,y in LAMINATE:
    lower=lower.cut(cyl(2.4,-.01,2.01,x,y)).cut(cyl(4.2,-.01,.3,x,y)).cut(K.cone(3.8,2.4,.3,.7,x,y))
    upper=upper.cut(cyl(1.6,1.99,5.01,x,y))
   ps.append(replace(p,id='A_base_lower',name='复合底板下层2mm',shape=lower.clean(),reference='完整承力底面；旋钮区域不挖槽不穿孔，原旋钮16个固定盲孔取消'))
   ps.append(replace(p,id='A_base_upper',name='复合底板上层3mm激光避让板',shape=upper.clean(),reference='两处带缺口激光轮廓替代旋钮铣槽；与下层定位夹紧后保留19个机身盲孔钻攻'))
   continue
  elif p.id=='A_top':
   for i,(cx,cy) in enumerate(H.KC,1):s=s.cut(global_shape(window(-.1,20),i,cx,cy))
  elif p.id.startswith('A_acrylic_') or p.id in ('A_locator','A_foam'):
   for i,(cx,cy) in enumerate(H.KC,1):s=s.cut(global_shape(window(-.1,20),i,cx,cy))
   if p.id=='A_locator':
    for i,(cx,cy) in enumerate(H.KC,1):
     rail=box(64,47,13.5,17.1,-10,1.1).cut(window(13.49,17.11)).cut(rail_window(13.49,17.11))
     # The front/rear lips are shortened to match the existing outer outline.
     for x,y in MOUNT:rail=rail.cut(cyl(3,14.1,17.11,x,y)).cut(cyl(2.4,13.09,17.11,x,y))
     rail=global_shape(rail,i,cx,cy).intersect(H.outer(13.5,17.1))
     # Preserve all pre-existing rail / fastener clearances.
     for other in baseline():
      if other.id in ('M_rail','P_rail_stop_1','P_rail_stop_2') or other.id.startswith(('B_rail_upper_','S_rail_','S_pcb_','S_rim_')):
       b=other.shape.BoundingBox();rail=rail.cut(box(b.xlen+.5,b.ylen+.5,13.49,17.11,(b.xmin+b.xmax)/2,(b.ymin+b.ymax)/2))
     s=s.fuse(rail)
    for i,(cx,cy) in enumerate(H.KC,1):
     for x,y in MOUNT:
      cutter=cyl(3,14.1,17.11,x,y).fuse(cyl(2.4,13.09,17.11,x,y))
      s=s.cut(global_shape(cutter,i,cx,cy))
  if p.id=='A_foam' or p.id.startswith('A_acrylic_'):
   # Datum rails extend below the locator; clearance in foam at the same XY.
   for i,(cx,cy) in enumerate(H.KC,1):
    zclear=11.49 if p.id in ('A_foam','A_acrylic_2') else 13.09
    s=s.cut(global_shape(box(64,47,zclear,17.11,-10,1.1),i,cx,cy))
    if p.id.startswith('A_acrylic_'):s=s.cut(global_shape(box(28,30,4.99,15.11,-F.SX,F.SY),i,cx,cy))
  if p.id=='A_top':
   for i,(cx,cy) in enumerate(H.KC,1):s=s.cut(global_shape(cover_outline(17.09,19.11,.15),i,cx,cy))
  ps.append(replace(p,shape=s.clean()))
 for i,(cx,cy) in enumerate(H.KC,1):
  for n,(x,y) in enumerate(MOUNT):
   ps.append(H.Part(f'I_service_{i}_{n}','机身总成固定M2x3铜嵌件','metal',global_shape(K.insert(x,y,14.1),i,cx,cy)))
 for n,(x,y) in enumerate(LAMINATE):
  s=K.screw(0,0,0,4,'cs').rotate((0,0,0),(1,0,0),180).translate((x,y,0))
  ps.append(H.Part(f'S_laminate_{n}','复合底板下装M2x4沉头螺丝','metal',s,(0,0,-20),reference='下板孔2.4，沉头齐平底面；上板M2贯穿牙，有效进入2mm'))
 return ps

def full(mode='inf',slide=None,compression=0,angle=0,key=0,env=None):
 env=environment() if env is None else env;x=H.RX if slide is None else slide
 env=[replace(p,shape=p.shape.translate((0,0,-key) if p.group=='keys' else (x-H.RX,0,0) if p.id=='M_block' else (0,0,0))) for p in env]
 extras=[H.Part(p.id,p.name,p.category,p.shape.translate((x,H.RY,0)),p.ex,p.printable,'slider',p.reference) for p in Q.build(mode)]
 return env+extras+knobs_global(compression,angle)

def allowed(a,b):
 if F.allowed(a,b):return True
 pair={a.id,b.id}
 if 'A_locator' in pair and any(i.startswith('I_service_') for i in pair):return True
 for prefix in ('','K1_','K2_'):
  ids={i[len(prefix):] for i in pair if i.startswith(prefix)} if prefix else pair
  if len(ids)!=2:continue
  if 'P_case' in ids and any(i.startswith('I_fixed_') for i in ids):return True
  if 'P_rotor_hub' in ids and any(i.startswith('I_disc_') for i in ids):return True
 if any(i in pair for i in ('A_base_lower','A_base_upper')):
  other=b.id if a.id in ('A_base_lower','A_base_upper') else a.id
  return other.startswith(('S_rim_','B_pcb_col_','B_rail_lower_','S_laminate_'))
 return False

def audit(ps):
 old=B.allowed;B.allowed=allowed
 try:return B.audit(ps)
 finally:B.allowed=old

def extraction(env,unit):
 # For pure +Z extraction, exact vertical projection must fit every body layer.
 # Check each moving part against the infinite upward extrusion of body material.
 results=[]
 for lift in (0,1,3,5,10,16,20,35,55):
  moved=[replace(p,shape=p.shape.translate((0,0,lift))) for p in unit if not p.id.endswith(tuple(f'S_service_{n}' for n in range(4)))]
  # Body collision only; unit self-contact was checked independently.
  bad=[]
  for a in moved:
   aa=a.shape.BoundingBox()
   for b in env:
    bb=b.shape.BoundingBox()
    if any(min(getattr(aa,k+'max'),getattr(bb,k+'max'))-max(getattr(aa,k+'min'),getattr(bb,k+'min'))<=1e-6 for k in 'xyz'):continue
    v=a.shape.intersect(b.shape).Volume()
    if v>1e-5:bad.append({'a':a.id,'b':b.id,'volume':round(v,6)})
  results.append({'liftMM':lift,'collisions':bad})
 return results

def main():
 (OUT/'parameters.json').write_text(json.dumps(PARAMS,ensure_ascii=False,indent=2),encoding='utf-8')
 ps=knob();r=audit(ps);(OUT/'local-check.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
 print('LOCAL',len(ps),json.dumps(r),flush=True)
 if r['invalid'] or r['unintendedIntersections']:return 2
 if '--local-only' in sys.argv:return 0
 env=environment();checks=[]
 for mode,slide,c,ang,key in [('inf',H.XMIN,0,0,0),('inf',H.XMAX,.15,45,4),('ongeki',H.RX,.075,90,4)]:
  parts=full(mode,slide,c,ang,key,env);r=audit(parts)
  checks.append({'mode':mode,'slide':slide,'compression':c,'angle':ang,'key':key,**r})
  (OUT/'full-check.json').write_text(json.dumps(checks,indent=2),encoding='utf-8')
  print('FULL',mode,json.dumps(r),flush=True)
  if r['invalid'] or r['unintendedIntersections']:return 3
 service_env=[p for p in full('inf',env=env) if not p.id.startswith(('K1_','K2_'))]
 ex=extraction(service_env,knobs_global());(OUT/'extraction-check.json').write_text(json.dumps(ex,indent=2),encoding='utf-8')
 print('EXTRACTION',[(r['liftMM'],len(r['collisions'])) for r in ex],flush=True)
 if any(r['collisions'] for r in ex):return 4
 return export_models()

def export_models():
 local=[replace(p,id='K1_'+p.id) for p in knob()]
 steps=[B.save_step(local,OUT/'旋钮总成.step'),B.save_step(local,OUT/'旋钮爆炸.step',True)]
 rows=[B.mesh(p) for p in local];compressed={p.id:p for p in knob(.15)}
 for r in rows:
  m=B.mesh(compressed[r['id'].removeprefix('K1_')]);r['vCompressed']=m['v'];r['fCompressed']=m['f']
 (OUT/'knob-meshes.json').write_text(json.dumps(rows,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
 env=environment()
 for mode,title in [('inf','整机-INF'),('ongeki','整机-音击')]:steps.append(B.save_step(full(mode,env=env),OUT/(title+'.step')))
 rows=[B.mesh(p) for p in full('inf',env=env)]
 for r in rows:r['variant']='inf' if r['id']=='Q_cap_inf' else 'common'
 p=Q.build('ongeki')[-1];p=H.Part(p.id,p.name,p.category,p.shape.translate((H.RX,H.RY,0)),p.ex,p.printable,'slider',p.reference)
 rows.append({**B.mesh(p),'variant':'ongeki'})
 comp={p.id:p for p in knobs_global(.15)}
 for r in rows:
  if r['id'] in comp:
   m=B.mesh(comp[r['id']]);r['vCompressed']=m['v'];r['fCompressed']=m['f']
 (OUT/'full-meshes.json').write_text(json.dumps(rows,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
 (OUT/'assembly-check.json').write_text(json.dumps(steps,indent=2),encoding='utf-8')
 return 0

if __name__=='__main__':sys.exit(main())
