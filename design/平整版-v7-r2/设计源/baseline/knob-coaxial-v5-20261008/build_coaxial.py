"""MIT. Coaxial no-PUSH service cartridge, two-sided wool felt, millimetres."""
from pathlib import Path
from dataclasses import replace
from functools import lru_cache
import sys,json,math,itertools
import cadquery as cq
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'knob-service-v4-20261008'))
import build_service as S4
B,H,K,Q=S4.B,S4.H,S4.K,S4.Q
BASE_ALLOWED=B.allowed
H.OUT[3]-=3.0
box,cyl,ring=K.box,K.cyl,K.ring
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'generated';OUT.mkdir(exist_ok=True)
MOUNT=[(a*15.6,b*15.6) for a,b in itertools.product((-1,1),repeat=2)]
FRAME=[(a*12.5,b*5.5) for a,b in itertools.product((-1,1),repeat=2)]
LID_INNER=FRAME[1:]
SC=[(7.0*math.cos(math.radians(a)),7.0*math.sin(math.radians(a))) for a in (30,150,270)]
LAMINATE=S4.LAMINATE
GUIDES=[(a*9.0,9.0) for a in (-1,1)]
PARAMS={'revision':'coaxial-dual-felt-service-v5-20261008','units':'mm','panelTopZ':19.1,
 'cover':{'boundingSize':[38,38,2],'centerXY':[0,0],'z':[17.1,19.1]},
 'serviceOpening':{'boundingSize':[32.6,32.6],'centerXY':[0,0],'upperBaseOnlyZ':[2,5],'profile':'square minus four body insert ears'},
 'baseLaminate':{'lowerMM':2,'upperMM':3,'retiredKnobMountHoles':16,'remainingBodyBlindHoles':19},
 'moduleBottom':{'size':[32,32,.4],'centerXY':[0,0],'z':[2,2.4]},
 'bodyRearExtensionMM':0,'sensorOffsetXY':[0,0],'sensorBoardRotationDeg':0,
 'serviceMountXY':MOUNT,'bearingMain':2,'bearingSecondary':0,'bearingSpec':'MR106ZZ 6x10x3',
 'bearingBottomZ':[11.75,15.15],'magnetZ':[6.45,8.95],'pcbZ':[2.4,4.0],
 'sensorTopZ':5.85,'nominalSurfaceAirGapMM':.6,
 'shaft':'custom turned nonmagnetic brass: diameter6, integral magnet pocket8.4 and flange17.6; groove and 3 tapped M2 holes; not stock rod',
 'disc':{'OD':22,'ID':8.8,'thickness':1.4,'z':[8.15,9.55],'material':'nonmagnetic brass'},
 'felt':{'OD':21,'ID':18,'thickness':1,'eachCompressionRange':[0,.15],'contact':'two axial faces clamping disc'},
 'discFastening':'3 M2x3 countersunk screws at R7.0; nominal flange thread engagement1.6mm',
 'gearTransmission':'none, direct coaxial sensing','limits':['CAD candidate, not physical acceptance','magnetic cross-talk unmeasured','custom machined shaft required','thin plate/pocket and tray floor need trial','FPC service loop unmodeled']}

def cover_outline(z0,z1,gap=0):return box(38+2*gap,38+2*gap,z0,z1)
def window(z0,z1,margin=0):
 s=box(32.6+2*margin,32.6+2*margin,z0,z1)
 for x,y in MOUNT:s=s.cut(cyl(6.0,z0-.01,z1+.01,x,y))
 return s.clean()
def global_shape(s,i,cx,cy):return s.rotate((0,0,0),(0,0,1),180).translate((cx,cy,0))
@lru_cache(maxsize=1)
def board_parts():
 R=json.loads((B.ENGINE/'enclosure/fluxdial-measured.json').read_text(encoding='utf-8'))
 pts=[(7.5-x,7.5-y) for x,y in R['outline_normalized']]
 pcb=cq.Workplane('XY',origin=(0,0,2.4)).polyline(pts).close().extrude(1.6).val()
 for q in R['holes']:
  x,y=q['center'];pcb=pcb.cut(cyl(q['diameter'],2.39,4.01,7.5-x,7.5-y))
 x,y=R['u4_pad_pattern_centre'];chip=box(6.2,5.1,4,5.85,7.5-x,7.5-y)
 x,y=R['fpc_signal_row_centre'];rear=y-.625
 fpc=box(12.1,5.45,4,6.2,7.5-x,7.5-rear+5.45/2)
 pads=[r for r in R['copper_hatches'] if abs(r['size'][0]-.8)<.001 and abs(r['size'][1]-.9)<.001]
 x=sum((p['bbox_normalized'][0]+p['bbox_normalized'][2])/2 for p in pads)/2
 y=sum((p['bbox_normalized'][1]+p['bbox_normalized'][3])/2 for p in pads)/2
 return pcb.clean(),chip,fpc,box(2.2,1.3,4,5.1,7.5-x,7.5-y)

def plate(z0,z1,lower=False):
 s=ring(22.4,18,z0,z1)
 for a in (-1,1):s=s.fuse(box(6,5,z0,z1,12*a,0))
 for x,y in GUIDES:
  s=s.fuse(box(3.4,3.4,z0,z1,x,y)).cut(box(1.3,1.9,z0-.01,z1+.01,x,y))
 for a in (-1,1):
  if lower:s=s.cut(cyl(2.4,z0-.01,z1+.01,12*a,0))
  else:s=K.cut_cs(s,12*a,0,z1)
 if lower:
  f=board_parts()[2].BoundingBox()
  s=s.cut(box(f.xlen+.6,34,z0-.01,6.35,(f.xmin+f.xmax)/2,0))
 return s.clean()

def shaft():
 s=cyl(8.4,6.45,9.55).cut(cyl(6.12,6.44,8.95))
 s=s.fuse(cyl(17.6,9.55,11.55)).fuse(cyl(6,11.55,33.8))
 s=s.cut(ring(6.1,5.7,18.3,19.0))
 for x,y in SC:s=s.cut(cyl(1.6,9.54,11.56,x,y))
 return s.clean()

def case():
 s=box(32,32,2,2.4).fuse(box(32,32,2.4,17.1).cut(box(30,30,2.39,17.11)))
 for x,y in MOUNT:s=s.cut(cyl(6.5,1.99,17.11,x,y))
 for x,y in FRAME:
  s=s.fuse(cyl(5.6,2.4,7.2,x,y)).cut(cyl(3,4.2,7.21,x,y)).cut(cyl(2.4,2.39,4.2,x,y))
 # Side grooves retain the PCB; rear FPC stays inside the cartridge.
 for a in (-1,1):
  rail=box(1.4,15,2.4,4.5,7.8*a,0)
  rail=rail.cut(box(16,15.5,2.39,4.15,0,0))
  s=s.fuse(rail)
 for x,y in GUIDES:s=s.fuse(box(1.0,1.6,2.4,11.7,x,y))
 # Lower plate supports and small clip retention for the M2 nuts.
 for a in (-1,1):
  s=s.fuse(box(5.8,5.8,2.4,3.95,12*a,0)).cut(cyl(2.4,1.99,4.01,12*a,0))
 # Removable PCB keeper fastens to two posts outside the board.
 for a in (-1,1):
  s=s.fuse(cyl(4.6,2.4,4.4,10*a,-10)).cut(cyl(1.6,2.39,4.41,10*a,-10))
 f=board_parts()[2].BoundingBox()
 s=s.cut(box(12.7,6.05,3.99,6.4,(f.xmin+f.xmax)/2,(f.ymin+f.ymax)/2))
 s=s.cut(box(13,4,2.4,6.5,0,15))
 s=s.cut(box(15.6,4,2.39,6.4,0,-15.5))
 s=s.cut(board_keeper())
 s=s.cut(cyl(22.8,5.5,7.35))
 for x,y in GUIDES:
  guide=box(1.0,1.6,2.4,11.7,x,y)
  s=s.cut(box(3.9,3.9,5.5,7.35,x,y)).fuse(guide)
 cx,cy=H.KC[0]
 pts=[(cx-x,cy-y) for x,y in H.D['board_vertices_mm']]
 shadow=cq.Workplane('XY',origin=(0,0,1.99)).polyline(pts).close().offset2D(.25).extrude(40).val()
 return s.cut(shadow).clean()

def board_keeper():
 keeper=box(20,3,2.4,4.4,0,-8.5).cut(box(16,4,2.39,4.15,0,-6.6))
 for a in (-1,1):keeper=K.cut_cs(keeper.fuse(cyl(4.6,4.4,5.4,10*a,-10)),10*a,-10,5.4)
 return keeper.clean()

def frame_halves():
 s=ring(14,9.4,11.6,18.15)
 for z in (11.75,15.15):s=s.cut(cyl(10.04,z,z+3.01))
 for y in (-5.5,5.5):s=s.fuse(box(30,4,12.4,14,0,y).cut(cyl(9.4,12.39,14.01)))
 for x,y in FRAME:
  s=s.fuse(cyl(5.6,7.2,17.1,x,y))
  s=s.cut(cyl(2.4,7.19,17.11,x,y)).cut(cyl(4.2,8.8,11.0,x,y)).cut(cyl(3,14.1,17.11,x,y))
  # Mounting-screw seat inside a lower side-access pocket.
  s=s.cut(box(4.2,8,8.8,11.0,x,y))
 for z in (11.75,15.15):s=s.cut(cyl(10.04,z,z+3.01))
 s=s.intersect(box(29.6,29.6,0,40)).cut(cyl(22.8,7.19,11.56))
 for x,y in GUIDES:s=s.cut(box(3.9,3.9,7.19,12.25,x,y))
 # A 0.2 split gap permits bearing insertion and bench separation.
 return tuple(s.intersect(box(40,50,0,40,a*20.1,0)).clean() for a in (-1,1))

@lru_cache(maxsize=20)
def knob(compression=0,angle=0):
 ps=[]
 def add(id,name,cat,s,group='fixed',ex=(0,0,0),printable=False,ref=''):
  if group=='rotor':s=s.rotate((0,0,0),(0,0,1),angle)
  ps.append(H.Part(id,name,cat,s.clean(),ex,printable,group,ref))
 add('P_case','同轴可抽取旋钮底壳32x32','cosmetic',case(),ex=(0,0,-25),printable=True,ref='0.4mm底面、板边导槽和检修排线口；薄壁待试打印')
 for id,name,s in zip(('C_pcb','C_sensor','C_fpc','C_capacitor'),('FluxDial15x18x1.6小板','MT6701 SOP8包络','C55172973排线座包络','电容包络'),board_parts()):add(id,name,'pcb' if id=='C_pcb' else 'sensor',s,ex=(0,0,-18),ref='原PCB XY接口；芯片偏心0.166mm及绝缘按原PCB交接复核')
 keeper=board_keeper()
 add('P_board_keeper','可拆PCB前侧挡条','petg',keeper,ex=(0,-18,-8),printable=True)
 for a in (-1,1):add(f'S_keeper_{a}','挡条M2x3沉头螺丝','metal',K.screw(10*a,-10,5.4,3,'cs'),ex=(a*12,-18,0),ref='螺丝拧入打印导孔，名义2mm牙；反复拆装保持力待试样')
 add('C_shaft','定制黄铜主轴及磁铁杯法兰','brass',shaft(),'rotor',(0,0,45),ref=PARAMS['shaft'])
 add('C_magnet','Ø6x2.5径向充磁磁铁','magnet',cyl(6,6.45,8.95),'rotor',(0,0,-8),ref='胶接在定制轴端盲孔；牌号、胶层和磁场待确认')
 for i,z in enumerate((11.75,15.15),1):add(f'C_bearing_{i}','MR106ZZ6x10x3','metal',ring(10,6,z,z+3),ex=(0,0,20+i*8))
 for id,z0,z1 in [('P_lower_spacer',11.55,11.75),('P_inner_spacer',14.75,15.15),('P_top_spacer',18.15,18.3)]:add(id,'轴承内圈垫'+str(round(z1-z0,2)),'brass',ring(6.8,6.08,z0,z1),'rotor',(0,0,35),ref='非磁性金属薄垫，实际轴承内圈接触宽度须核对')
 clip=ring(9.8,5.7,18.3,19).cut(box(3,8,18.29,19.01,0,5))
 add('C_retainer','Ø6轴用卡簧0.7包络','metal',clip,'rotor',(0,0,60),ref='暂用外径9.8的开口包络；须按采购卡簧图确认槽径、厚度和耳部')
 for id,s in zip(('P_carrier_left','P_carrier_right'),frame_halves()):add(id,'可拆双轴承座'+id[-4:],'petg',s,ex=(-22 if id.endswith('left') else 22,0,20),printable=True)
 for i,(x,y) in enumerate(FRAME):
  add(f'I_fixed_{i}','底壳M2x3铜嵌件','metal',K.insert(x,y,4.2),ex=(x,y,-8))
  add(f'S_fixed_{i}','轴承座M2x5螺丝','metal',K.screw(x,y,8.8,5),ex=(x,y,25))
 for i,(x,y) in enumerate(LID_INNER):
  add(f'I_lid_{i}','封盖连接M2x3铜嵌件','metal',K.insert(x,y,14.1),ex=(x,y,50))
  add(f'S_lid_{i}','封盖连接M2x8沉头螺丝','metal',K.screw(x,y,19.1,8,'cs'),ex=(x,y,80))
 d=ring(22,8.8,8.15,9.55)
 for x,y in SC:d=K.cut_cs(d,x,y,9.55) if False else d.cut(cyl(2.4,8.14,9.56,x,y)).cut(cyl(4.2,8.14,8.45,x,y)).cut(K.cone(3.8,2.4,8.45,.7,x,y))
 add('C_disc','双面摩擦黄铜盘22x8.8x1.4','brass',d,'rotor',(0,0,28),ref='三处R7.0下装孔；頭部避开毛毡接触环，M2x3连接轴端法兰')
 for i,(x,y) in enumerate(SC):add(f'S_disc_{i}','摩擦盘M2x3沉头螺丝','metal',K.screw(0,0,0,3,'cs').rotate((0,0,0),(1,0,0),180).translate((x,y,8.15)),'rotor',(x*3,y*3,-18))
 add('C_lower_plate','双面阻尼下压板22.4x18及插座避让','brass',plate(5.75+compression,7.15+compression,True),ex=(0,0,-15),ref='1.4mm主体，FPC上方最薄0.8mm；精加工按STEP，接触平面需去毛刺')
 add('C_upper_plate','双面阻尼上压板22.4x18','brass',plate(10.55-compression,11.55-compression),ex=(0,0,50),ref='1mm非磁性金属板；保持上下两面羊毛毡夹盘')
 for id,z0,z1 in [('F_lower',7.15+compression,8.15),('F_upper',9.55,10.55-compression)]:add(id,'羊毛毡环18x21x1 '+('下' if id.endswith('lower') else '上'),'felt',ring(21,18,z0,z1),ex=(0,0,-4 if id.endswith('lower') else 40),ref='双面接触；单侧0..0.15压缩为几何候选，不是已测手感')
 for a in (-1,1):
  x=12*a;nut=cq.Workplane('XY',origin=(x,0,4.15+compression)).polygon(6,4/math.cos(math.pi/6)).extrude(1.6).val().cut(cyl(2,4.14,5.91,x,0))
  add(f'N_adjust_{a}','阻尼M2六角螺母AF4厚1.6','metal',nut,ex=(a*20,0,-8))
  holder=box(5.4,5.4,3.95+compression,5.75+compression,x,0).cut(cq.Workplane('XY',origin=(x,0,4.15+compression)).polygon(6,4.15/math.cos(math.pi/6)).extrude(1.7).val()).cut(cyl(2.4,3.94,6,x,0))
  add(f'P_nut_holder_{a}','下压板防转螺母托','petg',holder,ex=(a*22,0,-14),printable=True,ref='随下压板浮动，拧紧后由螺母夹持，不用胶')
  add(f'S_adjust_{a}','阻尼M2x8沉头调节螺丝','metal',K.screw(x,0,11.55-compression,8,'cs'),ex=(a*20,0,70))
 lid=cover_outline(17.1,19.1).cut(cyl(14.6,17.09,19.11))
 for x,y in MOUNT+LID_INNER:lid=K.cut_cs(lid,x,y,19.1)
 add('A_lid','38x38齐平可取总成封盖','aluminum',lid,group='cover',ex=(0,0,75))
 for i,(x,y) in enumerate(MOUNT):add(f'S_service_{i}','总成固定M2x6沉头螺丝','metal',K.screw(x,y,19.1,6,'cs'),group='body_mount',ex=(x,y,92))
 cap=cyl(30,19.6,44.6).cut(cyl(20,19.59,22.6)).cut(cyl(6,22.59,36.6))
 add('C_cap','通用旋钮帽30x25孔6','metal',cap,group='cap',ex=(0,0,100),ref='沿用通用帽规格，顶丝及实物孔深需试装')
 return ps

@lru_cache(maxsize=1)
def environment():
 ps=[];base=B.environment()
 for p in base:
  s=p.shape
  if p.id=='A_base':
   for cx,cy in H.KC:
    for off in (5,12.2):
     for dx,dy in itertools.product((-off,off),repeat=2):s=s.fuse(cyl(1.6,0,5,cx+dx,cy+dy))
   for i,(cx,cy) in enumerate(H.KC,1):s=s.cut(global_shape(window(2,5.01),i,cx,cy))
   lower=s.intersect(H.outer(0,2));upper=s.intersect(H.outer(2,5))
   for x,y in LAMINATE:
    lower=lower.cut(cyl(2.4,-.01,2.01,x,y)).cut(cyl(4.2,-.01,.3,x,y)).cut(K.cone(3.8,2.4,.3,.7,x,y));upper=upper.cut(cyl(1.6,1.99,5.01,x,y))
   ps.extend([replace(p,id='A_base_lower',name='复合底板2mm下板',shape=lower.clean()),replace(p,id='A_base_upper',name='同轴复合底板3mm上板',shape=upper.clean())]);continue
  if p.id=='A_top':
   for i,(cx,cy) in enumerate(H.KC,1):s=s.cut(global_shape(cover_outline(17.09,19.11,.15),i,cx,cy))
  elif p.id.startswith('A_acrylic_') or p.id in ('A_locator','A_foam'):
   for i,(cx,cy) in enumerate(H.KC,1):s=s.cut(global_shape(window(-.1,20),i,cx,cy))
   if p.id=='A_locator':
    for i,(cx,cy) in enumerate(H.KC,1):
     rail=box(38,38,13.5,17.1).cut(window(13.49,17.11))
     for x,y in MOUNT:rail=rail.cut(cyl(3,14.1,17.11,x,y)).cut(cyl(2.4,13.49,17.11,x,y))
     rail=global_shape(rail,i,cx,cy).intersect(H.outer(13.5,17.1))
     for other in base:
      if other.id in ('M_rail','P_rail_stop_1','P_rail_stop_2') or other.id.startswith(('B_rail_upper_','S_rail_','S_pcb_','S_rim_')):
       b=other.shape.BoundingBox();rail=rail.cut(box(b.xlen+.5,b.ylen+.5,13.49,17.11,(b.xmin+b.xmax)/2,(b.ymin+b.ymax)/2))
     s=s.fuse(rail)
   else:
    for i,(cx,cy) in enumerate(H.KC,1):s=s.cut(global_shape(box(38,38,13.49,17.11),i,cx,cy))
   for i,(cx,cy) in enumerate(H.KC,1):
    for x,y in MOUNT:s=s.cut(global_shape(cyl(2.4,13.09,17.11,x,y),i,cx,cy))
  ps.append(replace(p,shape=s.clean()))
 for i,(cx,cy) in enumerate(H.KC,1):
  for n,(x,y) in enumerate(MOUNT):ps.append(H.Part(f'I_service_{i}_{n}','总成固定M2x3铜嵌件','metal',global_shape(K.insert(x,y,14.1),i,cx,cy)))
 for n,(x,y) in enumerate(LAMINATE):ps.append(H.Part(f'S_laminate_{n}','复合底板M2x4沉头螺丝','metal',K.screw(0,0,0,4,'cs').rotate((0,0,0),(1,0,0),180).translate((x,y,0)),(0,0,-20)))
 return ps
def knobs_global(compression=0,angle=0):return [replace(p,id=f'K{i}_{p.id}',shape=global_shape(p.shape,i,cx,cy),group=f'knob{i}_{p.group}') for i,(cx,cy) in enumerate(H.KC,1) for p in knob(compression,angle)]
def full(mode='inf',slide=None,compression=0,angle=0,key=0,env=None):
 env=environment() if env is None else env;x=H.RX if slide is None else slide
 env=[replace(p,shape=p.shape.translate((0,0,-key))) if p.group=='keys' else p for p in env]
 env=[replace(p,shape=p.shape.translate((x-H.RX,0,0))) if p.id=='M_block' else p for p in env]
 return env+[H.Part(p.id,p.name,p.category,p.shape.translate((x,H.RY,0)),p.ex,p.printable,'slider',p.reference) for p in Q.build(mode)]+knobs_global(compression,angle)
def allowed(a,b):
 if BASE_ALLOWED(a,b):return True
 pair={a.id,b.id}
 if 'A_locator' in pair and any(i.startswith('I_service_') for i in pair):return True
 if any(i in pair for i in ('A_base_lower','A_base_upper')):
  other=b.id if a.id in ('A_base_lower','A_base_upper') else a.id
  return other.startswith(('S_rim_','B_pcb_col_','B_rail_lower_','S_laminate_'))
 for prefix in ('','K1_','K2_'):
  ids={i[len(prefix):] for i in pair if i.startswith(prefix)} if prefix else pair
  if len(ids)!=2:continue
  if 'C_shaft' in ids and any(i.startswith('S_disc_') for i in ids):return True
  if 'P_case' in ids and any(i.startswith(('I_fixed_','S_keeper_')) for i in ids):return True
  if any(i.startswith('P_carrier_') for i in ids) and any(i.startswith('I_lid_') for i in ids):return True
 return False
def audit(ps):
 old=B.allowed;B.allowed=allowed
 try:return B.audit(ps)
 finally:B.allowed=old
def export_models():
 steps=[];local=[replace(p,id='K1_'+p.id) for p in knob()]
 for name,ps,ex in [('旋钮总成',local,False),('旋钮爆炸',local,True),('整机-INF',full('inf'),False),('整机-音击',full('ongeki'),False)]:steps.append(B.save_step(ps,OUT/(name+'.step'),ex))
 rows=[B.mesh(p) for p in local];compressed={p.id:p for p in knob(.15)}
 for r in rows:
  m=B.mesh(compressed[r['id'][3:]]);r['vCompressed']=m['v'];r['fCompressed']=m['f']
 (OUT/'knob-meshes.json').write_text(json.dumps(rows,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
 rows=[B.mesh(p) for p in full('inf')]
 for r in rows:r['variant']='inf' if r['id']=='Q_cap_inf' else 'common'
 p=Q.build('ongeki')[-1];rows.append({**B.mesh(H.Part(p.id,p.name,p.category,p.shape.translate((H.RX,H.RY,0)),p.ex,p.printable,'slider',p.reference)),'variant':'ongeki'})
 comp={p.id:p for p in knobs_global(.15)}
 for r in rows:
  if r['id'] in comp:m=B.mesh(comp[r['id']]);r['vCompressed']=m['v'];r['fCompressed']=m['f']
 (OUT/'full-meshes.json').write_text(json.dumps(rows,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
 (OUT/'assembly-check.json').write_text(json.dumps(steps,indent=2),encoding='utf-8')
 return 0
def main():
 r=audit(knob());(OUT/'local-check.json').write_text(json.dumps(r,indent=2),encoding='utf-8');print('LOCAL',len(knob()),r,flush=True)
 if r['invalid'] or r['unintendedIntersections']:return 2
 if '--local-only' in sys.argv:return 0
 records=[]
 for mode,x,c,a,k in [('inf',H.XMIN,0,0,0),('inf',H.XMAX,.15,120,4),('ongeki',H.RX,.075,240,4)]:
  r=audit(full(mode,x,c,a,k));records.append({'mode':mode,'slide':x,'compression':c,'angle':a,**r});print('FULL',mode,len(r['unintendedIntersections']),r['invalid'],flush=True)
  (OUT/'full-check.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
  if r['invalid'] or r['unintendedIntersections']:return 3
 env=[p for p in full() if not p.id.startswith(('K1_','K2_'))];ex=S4.extraction(env,knobs_global());(OUT/'extraction-check.json').write_text(json.dumps(ex,indent=2),encoding='utf-8');print('EXTRACTION',[(r['liftMM'],len(r['collisions'])) for r in ex],flush=True)
 if any(r['collisions'] for r in ex):return 4
 (OUT/'parameters.json').write_text(json.dumps(PARAMS,ensure_ascii=False,indent=2),encoding='utf-8')
 return export_models()
if __name__=='__main__':sys.exit(main())
