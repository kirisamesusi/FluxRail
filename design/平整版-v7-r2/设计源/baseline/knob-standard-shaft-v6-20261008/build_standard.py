"""MIT. Ordinary shaft / PETG cartridge with bolted aluminium panel load path, mm."""
from pathlib import Path
from dataclasses import replace
from functools import lru_cache
import sys,json,math,itertools
import cadquery as cq
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'knob-coaxial-v5-20261008'))
import build_coaxial as C5
B,H,K,Q,S4=C5.B,C5.H,C5.K,C5.Q,C5.S4
box,cyl,ring=K.box,K.cyl,K.ring
BASE_ALLOWED=B.allowed
ROOT=Path(__file__).resolve().parent;OUT=ROOT/'generated';OUT.mkdir(exist_ok=True)
MOUNT=C5.MOUNT;FRAME=C5.FRAME;GUIDES=C5.GUIDES;LAMINATE=C5.LAMINATE
CASE_MOUNT=list(itertools.product((-11.5,11.5),repeat=2));ANCHOR=list(itertools.product((-22.5,22.5),(-5,5)))
SC=[(6.4*math.cos(math.radians(a)),6.4*math.sin(math.radians(a))) for a in (90,180,270)]
SC[1]=(-6.6,0)
global_shape=C5.global_shape;window=C5.window;cover_outline=C5.cover_outline
PARAMS={'revision':'commercial-parts-panel-supported-v6-20261009','units':'mm','panelTopZ':19.1,
 'topMaterial':'2mm aluminium recommended','printedMaterial':'PETG','cover':{'boundingSize':[38,38,2],'z':[17.1,19.1]},
 'serviceOpening':{'boundingSize':[32.6,32.6],'profile':'same v5 ears; use DXF, not a plain square'},
 'baseLaminate':{'lowerMM':2,'upperMM':3},'moduleBottom':{'z':[2,3.2],'nominalThickness':1.2},
 'sensorBoardTranslationXY':[0,0],'originalChipNominalOffsetMM':.166,'magnetZ':[7.25,9.75],'pcbZ':[3.2,4.8],'sensorTopZ':6.65,'nominalSurfaceAirGapMM':.6,
 'bearingBottomZ':[12.55,15.55],'bearingSpec':'MR106ZZ 6x10x3','bearingMain':2,
 'shaft':{'diameter':6,'length':24,'z':[10.35,34.35],'processing':'cut and deburr only; no lathe, flats, tapped holes or circlip groove','material':'precision stainless rod; magnetic behaviour and fit must be verified'},
 'shaftGrip':{'bore':6.1,'length':2,'method':'PETG split hub, tangential M2x8 and metal nut; slip/pull/creep unvalidated'},
 'felt':{'OD':21,'ID':18,'thickness':1,'eachCompressionRange':[0,.15],'contact':'upper AND lower axial faces'},
 'disc':{'OD':22,'ID':8.8,'thickness':1.5,'z':[8.85,10.35],'method':'flat brass laser profile with inner clamp notch'},
 'lowerPlate':{'layersMM':[1,.5],'z':[6.35,7.85],'method':'two flat laser sheets; lower layer has through relief'},
 'upperPlate':{'thicknessMM':1,'z':[11.35,12.35],'method':'laser outline and through holes, no countersink'},
 'crossBrace':{'material':'aluminium recommended; same profile acrylic only as unaccepted trial','thicknessMM':2,'z':[14.8,16.8],'sizeMM':[29.2,17],'method':'laser outline and through holes only'},
 'panelBackplate':{'material':'aluminium','sizeMM':[52,40,1.5],'z':[15.6,17.1]},
 'hangerSleeveMM':5,'hangerSleeveOD':3.2,'hangerSleeveThread':'M2 female through','hangerWasher':'DIN125 M2 OD5 ID2.2 T0.3','hangerTopScrew':'M2x4 countersunk','hangerBottomScrew':'M2x3 socket cap','hangerXY':FRAME,'serviceMountXY':MOUNT,'panelAnchorXY':ANCHOR,
 'loadPath':'bearings -> four through bolts / aluminium brace -> aluminium service cover -> metal backplate -> four main aluminium panel bolts',
 'gearTransmission':'none; coaxial','bodyRearExtensionMM':0,
 'limits':['CAD candidate only','2mm grip needs torque/pull/creep tests','PETG tolerances and layer adhesion unverified','cap cavity and washer stack need real dimensions','magnetic cross-talk unmeasured','FPC flex path unmodeled']}

def hexnut(x,y,z,af=4):return cq.Workplane('XY',origin=(x,y,z)).polygon(6,af/math.cos(math.pi/6)).extrude(1.6).val().cut(cyl(2,z-.01,z+1.61,x,y)).clean()
def horizontal(s,x,y,z):return s.rotate((0,0,0),(1,0,0),90).translate((x,y,z))
def along_y(d,y0,y1,x,z):return horizontal(cyl(d,-y1,-y0),x,0,z)
def ultra(x,y,seat,length):return cyl(2,seat-length,seat,x,y).fuse(cyl(4,seat,seat+.6,x,y)).cut(cyl(1.1,seat+.12,seat+.61,x,y)).clean()
@lru_cache(maxsize=1)
def board_parts():return tuple(s.translate((0,0,.8)) for s in C5.board_parts())
def rotor_hub():
 s=cyl(8.4,7.25,10.35).cut(cyl(6.12,7.24,9.75)).fuse(cyl(17.6,10.35,12.35))
 s=s.fuse(box(4.6,6.8,8.35,12.35,6.1,0).intersect(cyl(17.6,8.34,12.36))).cut(cyl(6.1,10.35,12.36))
 s=s.cut(box(6.3,.7,10.34,12.36,6.05,0)).cut(box(5,.7,8.34,10.35,6.8,0))
 s=s.cut(along_y(2.4,-8,8,5.5,10.35)).cut(along_y(4.2,2.4,8,5.5,10.35))
 s=s.cut(along_y(2.2,3.8,25,5.5,10.35).rotate((5.5,3.8,10.35),(6.5,3.8,10.35),25))
 s=s.cut(horizontal(cq.Workplane('XY').polygon(6,4.15/math.cos(math.pi/6)).extrude(1.8).val(),5.5,-2.5,10.35))
 for x,y in SC:s=K.cut_cs(s,x,y,12.35)
 return s.fuse(ring(6.8,6.1,12.35,12.55)).clean()
def disc():
 s=ring(22,8.8,8.85,10.35).cut(box(6.8,12,8.84,10.36,6.3,0).intersect(cyl(17.8,8.83,10.37)))
 for x,y in SC:s=s.cut(cyl(2.4,8.84,10.36,x,y))
 return s.clean()
def plate(z0,z1):
 s=ring(22.4,18,z0,z1)
 for a in (-1,1):s=s.fuse(box(5.2,5,z0,z1,12*a,0)).cut(cyl(2.4,z0-.01,z1+.01,12*a,0))
 for x,y in GUIDES:s=s.fuse(box(3.4,3.4,z0,z1,x,y)).cut(box(1.3,1.9,z0-.01,z1+.01,x,y))
 for x,y in FRAME:s=s.cut(cyl(5.6,z0-.01,z1+.01,x,y))
 return s.clean()
def brace():
 s=box(29.2,17,14.8,16.8).cut(cyl(14.2,14.79,16.81))
 for x,y in FRAME:s=s.cut(cyl(3.4,14.79,16.81,x,y))
 for x,y in CASE_MOUNT:s=s.cut(cyl(7.2,14.79,16.81,x,y))
 for a in (-1,1):s=s.cut(cyl(3,14.79,16.81,9.8*a,-8.5))
 return s.clean()
def frame_halves():
 s=ring(14,9.4,12.4,18.55).fuse(ring(16,9.4,14.4,14.8))
 for y in (-5.5,5.5):s=s.fuse(box(29.2,4,13.2,14.8,0,y))
 for x,y in FRAME:s=s.fuse(cyl(5.6,11.8,14.8,x,y)).cut(cyl(3.4,11.79,14.81,x,y))
 s=s.cut(cyl(10.04,12.55,18.56))
 s=s.cut(cyl(9.4,12.39,18.96)).cut(cyl(22.8,11.59,12.36)).intersect(box(29.2,29.2,0,40))
 for x,y in GUIDES:s=s.cut(box(3.9,3.9,11.59,13.05,x,y))
 for a in (-1,1):s=s.cut(cyl(3,11.59,18.96,9.8*a,-8.5))
 return tuple(s.intersect(box(40,50,0,40,a*20.1,0)).clean() for a in (-1,1))
def board_keeper():
 s=box(20,3,3.2,5.2,0,-8.5).cut(box(16,4,3.19,4.95,0,-6.6))
 for a in (-1,1):s=K.cut_cs(s.fuse(cyl(4,5.2,6.2,9.8*a,-8.5)),9.8*a,-8.5,6.2)
 return s.clean()
def case():
 s=box(32,32,2,3.2).fuse(box(32,32,3.2,17.1).cut(box(29.6,29.6,3.19,17.11)))
 for x,y in MOUNT:s=s.cut(cyl(6.5,1.99,17.11,x,y))
 for x,y in CASE_MOUNT:s=s.fuse(cyl(6.8,3.2,17.1,x,y)).cut(cyl(3,14.1,17.11,x,y)).cut(cyl(2.4,13.09,14.11,x,y))
 for a in (-1,1):
  s=s.fuse(box(1.4,15,3.2,5.3,7.8*a,0).cut(box(16,15.5,3.19,4.95)))
  s=s.fuse(box(5.8,5.8,3.2,4.55,12*a,0)).cut(cyl(2.4,1.99,4.56,12*a,0))
  s=s.fuse(cyl(4,3.2,5.2,9.8*a,-8.5)).cut(cyl(1.6,3.19,5.21,9.8*a,-8.5))
 for x,y in GUIDES:s=s.fuse(box(1,1.6,3.2,12.5,x,y))
 s=s.cut(box(13,4,3.2,7.3,0,15)).cut(box(15.6,4,3.19,7.2,0,-15.5)).cut(board_keeper()).cut(cyl(22.8,6.2,8.05))
 for x,y in GUIDES:s=s.cut(box(3.9,3.9,6.2,8.05,x,y)).fuse(box(1,1.6,3.2,12.5,x,y))
 for x,y in GUIDES:s=s.cut(box(3.9,3.9,11.19,12.56,x,y)).fuse(box(1,1.6,3.2,12.5,x,y))
 for x,y in FRAME:s=s.cut(cyl(6,9.3,17.11,x,y))
 for a in (-1,1):s=s.cut(cyl(3,6.19,17.11,9.8*a,-8.5))
 s=s.cut(along_y(2.4,3.8,25,5.5,10.35).rotate((5.5,3.8,10.35),(6.5,3.8,10.35),25))
 for x,y in MOUNT:s=s.cut(cyl(6.5,1.99,17.11,x,y))
 cx,cy=H.KC[0];pts=[(cx-x,cy-y) for x,y in H.D['board_vertices_mm']]
 shadow=cq.Workplane('XY',origin=(0,0,1.99)).polyline(pts).close().offset2D(.25).extrude(40).val()
 return s.cut(shadow).clean()
@lru_cache(maxsize=20)
def knob(compression=0,angle=0):
 ps=[]
 def add(id,name,cat,s,group='fixed',ex=(0,0,0),printable=False,ref=''):
  if group=='rotor':s=s.rotate((0,0,0),(0,0,1),angle)
  ps.append(H.Part(id,name,cat,s.clean(),ex,printable,group,ref))
 add('P_case','PETG一体底壳32x32底厚1.2','petg',case(),ex=(0,0,-25),printable=True,ref='底面和壁1.2；轴承独立悬挂到金属封盖，角柱只固定底壳')
 for id,name,s in zip(('C_pcb','C_sensor','C_fpc','C_capacitor'),('FluxDial15x18x1.6','MT6701 SOP8包络','C55172973排线座','电容包络'),board_parts()):add(id,name,'pcb' if id=='C_pcb' else 'sensor',s,ex=(0,0,-18),ref='沿用原PCB，芯片名义偏心0.166mm及排线需实物确认')
 add('P_board_keeper','PETG可拆退板挡条','petg',board_keeper(),ex=(0,-18,-8),printable=True)
 for a in (-1,1):add(f'S_keeper_{a}','挡条M2x3沉头螺丝','metal',K.screw(9.8*a,-8.5,6.2,3,'cs'),ex=(a*12,-18,0),ref='低载打印导孔，反复拆装保持力待试样')
 add('C_shaft','普通精密光轴6x24','metal',cyl(6,10.35,34.35),'rotor',(0,0,45),ref='仅截长去毛刺，无轴槽法兰螺纹；优先实測低磁性不锈钢棒，轴承配合需试装')
 add('P_rotor_hub','PETG开缝夹轴轮毂及磁铁杯','petg',rotor_hub(),'rotor',(0,0,30),True,'横向M2x8+金属螺母夹紧；2mm夹持长度需转矩、拉拔和蠕变试样')
 add('S_hub','夹轴M2x8内六角螺丝','metal',K.screw(0,0,0,8).rotate((0,0,0),(1,0,0),-90).translate((5.5,2.4,10.35)),'rotor',(20,10,20))
 add('N_hub','夹轴M2六角螺母AF4','metal',horizontal(hexnut(0,0,0),5.5,-2.6,10.35),'rotor',(20,-10,20))
 add('C_magnet','径向充磁磁铁6x2.5','magnet',cyl(6,7.25,9.75),'rotor',(0,0,-8),ref='仅磁铁胶接，轴与轮毂不用胶传扭')
 add('C_disc','激光黄铜摩擦盘22x8.8x1.5','brass',disc(),'rotor',(0,0,28),ref='内圈工具缺口，毛毡接触环完整；三孔(0,±6.4)、(-6.6,0)，非等分')
 for i,(x,y) in enumerate(SC):
  add(f'S_disc_{i}','盘连接M2x5沉头螺丝','metal',K.screw(x,y,12.35,5,'cs'),'rotor',(x*3,y*3,20))
  add(f'N_disc_{i}','盘连接M2六角螺母AF4','metal',hexnut(x,y,7.25),'rotor',(x*3,y*3,-18))
 for i,z in enumerate((12.55,15.55),1):add(f'C_bearing_{i}','MR106ZZ6x10x3','metal',ring(10,6,z,z+3),ex=(0,0,20+i*8))
 add('P_top_spacer','PETG帽内可换轴承止挡环-阶梯6.8与10-内孔6.1-高4','petg',ring(6.8,6.1,18.55,18.95).fuse(ring(10,6.1,18.95,22.55)),'rotor',(0,0,35),True,'大端朝下打印；细端只接触轴承内圈，实测外径不得超过6.9；帽内台阶名义留0.05间隙，顶丝锁帽后不得预压轴承，轴向间隙须试装调节')
 for id,s in zip(('P_carrier_left','P_carrier_right'),frame_halves()):add(id,'PETG双轴承分体座'+id[-4:],'petg',s,ex=(-22 if id.endswith('left') else 22,0,20),printable=True)
 add('A_brace','激光铝横向压板29.2x17x2','aluminum',brace(),ex=(0,0,35),ref='主方案2mm铝板，只有轮廓和通孔；同轮廓亚克力仅试装，孔密且近边，未接受为承力替代')
 for i,(x,y) in enumerate(FRAME):
  add(f'T_hanger_{i}','M2x5双通圆铜柱-外径3.2','brass',ring(3.2,1.6,11.8,16.8,x,y),ex=(x,y,45),ref='淘宝双通圆铜柱成品，内螺纹M2，外径3.2，高5；上下独立螺丝，上端啮合1.7，下端2.7，尖端间隔0.6；内孔CAD仅螺纹包络')
  add(f'W_hanger_{i}','M2标准平垫片-外5内2.2厚0.3','metal',ring(5,2.2,11.5,11.8,x,y),ex=(x,y,-12))
  add(f'W_brace_{i}','M2标准平垫片-外5内2.2厚0.3','metal',ring(5,2.2,16.8,17.1,x,y),ex=(x,y,40))
  add(f'N_hanger_{i}','铜柱下端M2x3内六角螺丝','metal',K.screw(x,y,11.5,3).rotate((x,y,11.5),(x+1,y,11.5),180),ex=(x,y,-15),ref='头在下，螺纹向上；拆模块后从下方松开，转矩以试样为准')
  add(f'S_lid_{i}','铜柱上端M2x4沉头螺丝','metal',K.screw(x,y,19.1,4,'cs'),ex=(x,y,70))
 for i,(x,y) in enumerate(CASE_MOUNT):
  add(f'I_case_{i}','底壳M2x3铜嵌件','metal',K.insert(x,y,14.1),ex=(x,y,-8))
  add(f'S_case_{i}','底壳连接M2x6沉头螺丝','metal',K.screw(x,y,19.1,6,'cs'),ex=(x,y,60))
 lower=plate(6.35+compression,7.35+compression).cut(box(12.7,34,6.34+compression,7.36+compression))
 for a,label in [(-1,'left'),(1,'right')]:add('C_lower_plate_'+label,'下压板黄铜1mm避让半片'+label,'brass',lower.intersect(box(30,40,0,20,a*21.35,0)),ex=(a*8,0,-15),ref='两半片全厚通切FPC避让，和0.5mm上层叠放，免铣盲槽')
 add('C_lower_face','下压板黄铜0.5mm接触层','brass',plate(7.35+compression,7.85+compression),ex=(0,0,-10),ref='整面盖住避让层，调节螺丝夹紧，无胶接')
 add('C_upper_plate','上压板黄铜22.4x18x1','brass',plate(11.35-compression,12.35-compression),ex=(0,0,50),ref='只激光輪廓/通孔，用平底超低头螺丝，不沉孔')
 for id,z0,z1 in [('F_lower',7.85+compression,8.85),('F_upper',10.35,11.35-compression)]:add(id,'双面羊毛毡18x21x1'+id[-5:],'felt',ring(21,18,z0,z1),ex=(0,0,-4 if id.endswith('lower') else 40))
 for a in (-1,1):
  x=12*a;add(f'N_adjust_{a}','阻尼M2六角螺母AF4','metal',hexnut(x,0,4.75+compression),ex=(a*20,0,-8))
  holder=box(5.4,5.4,4.55+compression,6.35+compression,x,0).cut(cq.Workplane('XY',origin=(x,0,4.75+compression)).polygon(6,4.15/math.cos(math.pi/6)).extrude(1.7).val()).cut(cyl(2.4,4.54,7,x,0))
  add(f'P_nut_holder_{a}','PETG可拆阻尼螺母托','petg',holder,ex=(a*22,0,-14),printable=True)
  add(f'S_adjust_{a}','阻尼M2x8超低头螺丝头4x0.6-T5','metal',ultra(x,0,12.35-compression,8),ex=(a*20,0,70),ref='用户淘宝截图M2x8，头径4、高0.6、T5；仅替代相同头径高度，不以商品示意图代替到货测量')
 lid=cover_outline(17.1,19.1).cut(cyl(14.6,17.09,19.11))
 for x,y in MOUNT+FRAME+CASE_MOUNT:lid=K.cut_cs(lid,x,y,19.1)
 add('A_lid','承力铝封盖38x38x2','aluminum',lid,'cover',(0,0,75),ref='4轴承悬挂+4底壳连接+4外周固定，顶板齐平')
 for i,(x,y) in enumerate(MOUNT):add(f'S_service_{i}','总成固定M2x6沉头螺丝','metal',K.screw(x,y,19.1,6,'cs'),'body_mount',(x,y,92))
 cap=cyl(30,19.6,44.6).cut(cyl(20,19.59,22.6)).cut(cyl(6,22.59,36.6))
 add('C_cap','通用旋钮帽30x25孔6','metal',cap,'cap',(0,0,100),ref='帽顶丝夹轴形成上端止挡；实际内腔20x3和孔深须核对')
 return ps

def backplate():
 s=box(52,40,15.6,17.1).cut(window(15.59,17.11))
 for x,y in MOUNT+ANCHOR:s=s.cut(cyl(2.4,15.59,17.11,x,y))
 cx,cy=H.KC[0]
 for p in C5.environment():
  if p.id.startswith('S_rim_'):
   b=p.shape.BoundingBox();x=cx-(b.xmin+b.xmax)/2;y=cy-(b.ymin+b.ymax)/2
   if abs(x)<30 and abs(y)<30:
    for a in (-1,1):s=s.cut(cyl(4.4,15.59,17.11,a*abs(x),y))
 return s.clean()
def nut_holder(x,y):
 s=cyl(6,13.7,15.6,x,y).cut(cq.Workplane('XY',origin=(x,y,14)).polygon(6,4.15/math.cos(math.pi/6)).extrude(1.61).val()).cut(cyl(2.4,13.69,15.61,x,y))
 return s.clean()
@lru_cache(maxsize=1)
def environment():
 ps=[]
 for p in C5.environment():
  if p.id.startswith('I_service_'):continue
  s=p.shape
  if p.id=='A_top':
   for i,(cx,cy) in enumerate(H.KC,1):
    for x,y in ANCHOR:s=K.cut_cs(s,cx-x,cy-y,19.1)
  if p.id=='A_locator' or p.id.startswith('A_acrylic_') or p.id=='A_foam':
   for i,(cx,cy) in enumerate(H.KC,1):s=s.cut(global_shape(box(52.4,40.4,13.09,17.11),i,cx,cy))
  ps.append(replace(p,shape=s.clean()))
 for i,(cx,cy) in enumerate(H.KC,1):
  ps.append(H.Part(f'A_backplate_{i}','顶板承力背板52x40x1.5','aluminum',global_shape(backplate(),i,cx,cy)))
  for j,(x,y) in enumerate(MOUNT+ANCHOR):
   ps.append(H.Part(f'N_panel_{i}_{j}','背板M2螺母','metal',global_shape(hexnut(x,y,14),i,cx,cy),reference='金属螺母直接接触背板；PETG杯仅防转和防掉'))
   ps.append(H.Part(f'P_panelnut_{i}_{j}','PETG背板防掉螺母杯6x1.9','petg',global_shape(nut_holder(x,y),i,cx,cy),printable=True,reference='杯口胶接/胶带固定到背板下表面，仅定位防掉，不用胶承担旋钮载荷'))
  for j,(x,y) in enumerate(ANCHOR):ps.append(H.Part(f'S_panel_{i}_{j}','背板连接主顶板M2x6沉头螺丝','metal',global_shape(K.screw(x,y,19.1,6,'cs'),i,cx,cy)))
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
 if any(i in pair for i in ('A_base_lower','A_base_upper')):
  other=b.id if a.id in ('A_base_lower','A_base_upper') else a.id
  return other.startswith(('S_rim_','B_pcb_col_','B_rail_lower_','S_laminate_'))
 for prefix in ('','K1_','K2_'):
  ids={i[len(prefix):] for i in pair if i.startswith(prefix)} if prefix else pair
  if len(ids)==2 and 'P_case' in ids and any(i.startswith(('I_case_','S_keeper_')) for i in ids):return True
  for j in range(4):
   if ids in ({f'T_hanger_{j}',f'N_hanger_{j}'},{f'T_hanger_{j}',f'S_lid_{j}'}):return True
 return False
def audit(ps):
 old=B.allowed;B.allowed=allowed
 try:return B.audit(ps)
 finally:B.allowed=old
def export_models():
 steps=[];local=[replace(p,id='K1_'+p.id) for p in knob()]
 for name,ps,ex in [('旋钮总成',local,False),('旋钮爆炸',local,True),('整机-INF',full('inf'),False),('整机-音击',full('ongeki'),False)]:steps.append(B.save_step(ps,OUT/(name+'.step'),ex))
 rows=[B.mesh(p) for p in local];comp={p.id:p for p in knob(.15)}
 for r in rows:m=B.mesh(comp[r['id'][3:]]);r['vCompressed']=m['v'];r['fCompressed']=m['f']
 (OUT/'knob-meshes.json').write_text(json.dumps(rows,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
 rows=[B.mesh(p) for p in full('inf')]
 for r in rows:r['variant']='inf' if r['id']=='Q_cap_inf' else 'common'
 p=Q.build('ongeki')[-1];rows.append({**B.mesh(H.Part(p.id,p.name,p.category,p.shape.translate((H.RX,H.RY,0)),p.ex,p.printable,'slider',p.reference)),'variant':'ongeki'})
 comp={p.id:p for p in knobs_global(.15)}
 for r in rows:
  if r['id'] in comp:m=B.mesh(comp[r['id']]);r['vCompressed']=m['v'];r['fCompressed']=m['f']
 (OUT/'full-meshes.json').write_text(json.dumps(rows,ensure_ascii=False,separators=(',',':')),encoding='utf-8');(OUT/'assembly-check.json').write_text(json.dumps(steps,indent=2),encoding='utf-8')
def main():
 if '--export-only' in sys.argv:
  (OUT/'parameters.json').write_text(json.dumps(PARAMS,ensure_ascii=False,indent=2),encoding='utf-8');export_models();return 0
 r=audit(knob());(OUT/'local-check.json').write_text(json.dumps(r,indent=2),encoding='utf-8');print('LOCAL',len(knob()),r,flush=True)
 if r['invalid'] or r['unintendedIntersections']:return 2
 if '--local-only' in sys.argv:return 0
 records=[]
 for mode,x,c,a,k in [('inf',H.XMIN,0,0,0),('inf',H.XMAX,.15,120,4),('ongeki',H.RX,.075,240,4)]:
  r=audit(full(mode,x,c,a,k));records.append({'mode':mode,'slide':x,'compression':c,'angle':a,**r});print('FULL',mode,r['unintendedIntersections'],r['invalid'],flush=True)
  (OUT/'full-check.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
  if r['invalid'] or r['unintendedIntersections']:return 3
 env=[p for p in full() if not p.id.startswith(('K1_','K2_'))];ex=S4.extraction(env,knobs_global());(OUT/'extraction-check.json').write_text(json.dumps(ex,indent=2),encoding='utf-8');print('EXTRACTION',[(r['liftMM'],len(r['collisions'])) for r in ex],flush=True)
 if any(r['collisions'] for r in ex):return 4
 (OUT/'parameters.json').write_text(json.dumps(PARAMS,ensure_ascii=False,indent=2),encoding='utf-8');export_models();return 0
if __name__=='__main__':sys.exit(main())
