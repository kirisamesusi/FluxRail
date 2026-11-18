"""FluxRail flush knob v7 trial. MIT. Local axis origin; all lengths in mm."""
from dataclasses import replace
from functools import lru_cache
from itertools import product
from pathlib import Path
import math
import sys

import cadquery as cq

SOURCE = Path(__file__).resolve().parent
ROOT = SOURCE.parent
sys.path.insert(0, str(SOURCE / 'baseline/knob-standard-shaft-v6-20261008'))
import build_standard as v6

B, H, K, Q = v6.B, v6.H, v6.K, v6.Q
box, cyl, ring = K.box, K.cyl, K.ring
Part = H.Part
MOUNT, CASE_MOUNT = v6.MOUNT, v6.CASE_MOUNT
HANGERS = list(product((-12.8, 12.8), (-6.6, 6.6)))
LID_Z = 17.1
LID_TOP = 19.1
HANGER_BASE_Z = 12.1
HANGER_WASHER_BOTTOM_Z = 11.8
GUIDE_OD = 17.1
GUIDE_BORE = 17.6
POCKET_OD = 22.5
POCKET_ROOF = 18.4
FLANGE_OD = 21.95
DECK_Z = 12.90
CUP_Z = 16.65
FLANGE_TOP = 17.90
DOWN_STOP = 16.40
MR_Z = 13.0
MR_BORE = 10.08
MR_OUTER_CONTACT_ID = 9.4
MR_INNER_CONTACT_OD = 6.85
UPPER_Z = 18.25
UPPER_BORE = 13.08
UPPER_OUTER_CONTACT_ID = 12.0
UPPER_INNER_CONTACT_OD = 7.25
MAGNET_Z = 7.25
SHAFT_Z = 10.35
SHAFT_TOP = 34.35
CAP_Z = 24.75
CAP_BORE_Z = CAP_Z + 3
CAP_SETSCREW_Z = CAP_Z + 6
CLAMP_TOP = 27.35
KEY_X = 5.66
KEY_WIDTH = 1.10
KEY_LENGTH = 2.20
KEY_TOP = 17.70

PARAMS = {
    'revision': 'flat-knob-v7-r1-2026-11-06', 'units': 'mm',
    'machineAxisXY': H.KC, 'serviceMountXY': MOUNT, 'caseMountXY': CASE_MOUNT,
    'localHangerXY': HANGERS, 'lid': [38, 38, 2],
    'hangerStack': {'columnZ': [HANGER_BASE_Z, LID_Z], 'topWasherCount': 0,
                    'bottomWasherZ': [HANGER_WASHER_BOTTOM_Z, HANGER_BASE_Z]},
    'lidZ': [LID_Z, LID_TOP], 'lidThroughDiameter': GUIDE_BORE,
    'lidUndersidePocketDiameter': POCKET_OD, 'lidUndersidePocketDepth': POCKET_ROOF-LID_Z,
    'upperBearing': {'spec': '686-2RS 6x13x5', 'z': [UPPER_Z, UPPER_Z+5],
                     'seatID': UPPER_BORE, 'guideOD': GUIDE_OD,
                     'outerAbutmentID': UPPER_OUTER_CONTACT_ID, 'innerAbutmentOD': UPPER_INNER_CONTACT_OD},
    'lowerBearing': {'spec': 'MR106ZZ 6x10x3', 'z': [MR_Z, MR_Z+3], 'seatID': MR_BORE,
                     'outerAbutmentID': MR_OUTER_CONTACT_ID, 'innerAbutmentOD': MR_INNER_CONTACT_OD},
    'wave': {'spec': '304, 3 waves, ID14 OD21 sheet0.4',
             'freeHeightAssumption': 2.0, 'installedHeight': CUP_Z-DECK_Z-2,
             'notMeasured': True, 'model': 'periodic sinusoidal envelope; no spring rate'},
    'POM': {'qtyPerKnob': 2, 'ID': 15, 'OD': 20, 'thickness': 1},
    'springSeatsZ': [DECK_Z, CUP_Z], 'flangeZ': [CUP_Z, FLANGE_TOP],
    'nominalDownHardStopTravel': round(CUP_Z-DOWN_STOP, 3),
    'nominalUpServiceStopTravel': round(POCKET_ROOF-FLANGE_TOP, 3),
    'shaft': {'diameter': 6, 'length': 24, 'z': [SHAFT_Z, SHAFT_TOP]},
    'magnet': {'diameter': 6, 'thickness': 2.5, 'polarization': 'diametrical',
               'z': [MAGNET_Z, MAGNET_Z+2.5]},
    'chipOffsetXYMagnitude': .166, 'chipTopZ': 6.65,
    'cap': {'diameter': 30, 'height': 25, 'bore': 6, 'bottomZ': CAP_Z,
            'nominalCavityDiameter': 20, 'nominalCavityDepth': 3,
            'nominalShaftGrip': round(SHAFT_TOP-CAP_BORE_Z, 3),
            'setScrewZAssumption': CAP_SETSCREW_Z},
}


def nut_pocket(x, y, z):
    return v6.horizontal(
        cq.Workplane('XY').polygon(6, 4.18/math.cos(math.pi/6)).extrude(1.76).val(), x, y, z)


def clamp_cut(s, x, z, y):
    s = s.cut(v6.along_y(2.35, -14, 14, x, z))
    s = s.cut(v6.along_y(4.12, y, 14, x, z))
    # Nut slides in from its side through the full-depth hexagonal pocket.
    s = s.cut(nut_pocket(x, -y-.03, z))
    return s.cut(v6.along_y(4.6, -14, -y-1.79, x, z)).clean()


@lru_cache(maxsize=1)
def hub():
    s = cyl(8.4, MAGNET_Z, SHAFT_Z).fuse(cyl(17.6, 8.0, 11.72))
    s = s.fuse(cyl(8.4, 11.72, 12.60)).fuse(cyl(MR_INNER_CONTACT_OD, 12.60, MR_Z))
    s = s.cut(cyl(6.12, MAGNET_Z-.02, 9.75)).cut(cyl(6.10, SHAFT_Z, MR_Z+.01))
    s = s.cut(box(7, .78, 8.0, MR_Z+.01, 6.45, 0))
    # Recess around the low collar leaves room for the FPC and sensor package.
    return clamp_cut(s, 5.42, 9.38, 2.75)


@lru_cache(maxsize=1)
def top_clamp():
    z0 = UPPER_Z + 5
    s = cyl(14.0, z0+.70, CLAMP_TOP).fuse(cyl(UPPER_INNER_CONTACT_OD, z0, z0+.70))
    s = s.cut(cyl(6.10, z0-.01, CLAMP_TOP+.01)).cut(box(6,.78,z0-.01,CLAMP_TOP+.01,6,0))
    # The head and nut pockets break out at the top; the lower face stays solid.
    return clamp_cut(s, 4.7, 25.56, 2.3)


@lru_cache(maxsize=1)
def lower_seat():
    s = cyl(22.75, HANGER_BASE_Z, DECK_Z)
    s = s.fuse(cyl(13.2, DECK_Z, DOWN_STOP))
    s = s.fuse(cyl(14.6, DECK_Z, DECK_Z+.75))  # locate the lower POM, not the spring
    s = s.fuse(ring(22.75, 21.52, DECK_Z, DECK_Z+1.40))  # locating rim, not a load shoulder
    for sign in (-1, 1):
        s = s.fuse(box(KEY_WIDTH,KEY_LENGTH,DOWN_STOP,KEY_TOP,sign*KEY_X,0))
    for y in (-6.6, 6.6):
        s = s.fuse(box(25.6, 4, HANGER_BASE_Z, DECK_Z, 0,y))
    for x,y in HANGERS:
        s = s.fuse(cyl(5.5,HANGER_BASE_Z,LID_Z,x,y))
        s = s.fuse(box(3.1, 5.7, HANGER_BASE_Z, 15.5, x*.89,y))
    for sign in (-1, 1):
        # Preserve the two tools reaching the PCB retaining bar.
        s = s.cut(cyl(3.2,11.79,17,9.8*sign,-8.5))
    for x,y in HANGERS:
        s = s.cut(cyl(3.42, 11.79, LID_Z+.01,x,y))
    for x,y in CASE_MOUNT:
        s = s.cut(cyl(7.24, 11.79, LID_Z+.01,x,y))
    s = s.cut(cyl(MR_OUTER_CONTACT_ID,11.79,16.41)).cut(cyl(MR_BORE,MR_Z,MR_Z+3))
    return tuple(s.intersect(box(40,50,0,40,i*20.06,0)).clean() for i in (-1,1))


@lru_cache(maxsize=1)
def upper_cup():
    s = cyl(FLANGE_OD,CUP_Z,FLANGE_TOP).fuse(cyl(GUIDE_OD,FLANGE_TOP,UPPER_Z+5))
    # Retain the upper POM by its OD. The lip is above the top wave.
    s = s.fuse(ring(FLANGE_OD,20.42,CUP_Z-.65,CUP_Z))
    s = s.cut(cyl(UPPER_OUTER_CONTACT_ID,CUP_Z-.02,UPPER_Z+.01))
    s = s.cut(cyl(UPPER_BORE,UPPER_Z,UPPER_Z+5+.02))
    # Keep a broad land over the lower tower for the hard down stop, with
    # an inner recess for the shaft. Extend the floor inward only at that land.
    floor = ring(15.4,8.9,CUP_Z,CUP_Z+.50)
    s = s.fuse(floor)
    for sign in (-1,1):
        s = s.cut(box(KEY_WIDTH+.34,KEY_LENGTH+.36,CUP_Z-.02,KEY_TOP+.38,sign*KEY_X,0))
    # The lip and cup are machined/printed as one solid.
    return s.clean()


@lru_cache(maxsize=1)
def case():
    s = box(32,32,2,3.2).fuse(box(32,32,3.2,17.1).cut(box(29.6,29.6,3.19,17.11)))
    for x,y in CASE_MOUNT:
        s = s.fuse(cyl(6.8,3.2,17.1,x,y)).cut(cyl(3,14.1,17.11,x,y)).cut(cyl(2.4,13.09,14.11,x,y))
    for sign in (-1,1):
        s = s.fuse(box(1.4,15,3.2,5.3,7.8*sign,0).cut(box(16,15.5,3.19,4.95)))
        s = s.fuse(cyl(4,3.2,5.2,9.8*sign,-8.5)).cut(cyl(1.6,3.19,5.21,9.8*sign,-8.5))
    s = s.cut(box(13,4,3.2,7.3,0,15)).cut(box(15.6,4,3.19,7.2,0,-15.5)).cut(v6.board_keeper())
    for sign in (-1,1):
        s = s.cut(cyl(3.0,6.19,17.11,9.8*sign,-8.5))
    for x,y in HANGERS:
        s = s.cut(cyl(6.20,9.35,17.11,x,y))
    for x,y in MOUNT:
        s = s.cut(cyl(6.5,1.99,17.11,x,y))
    cx,cy=H.KC[0]
    pts=[(cx-x,cy-y) for x,y in H.D['board_vertices_mm']]
    shadow=cq.Workplane('XY',origin=(0,0,1.99)).polyline(pts).close().offset2D(.25).extrude(40).val()
    return s.cut(shadow).clean()


@lru_cache(maxsize=1)
def lid():
    s = v6.cover_outline(LID_Z,LID_TOP)
    s = s.cut(cyl(GUIDE_BORE,LID_Z-.01,LID_TOP+.01))
    s = s.cut(cyl(POCKET_OD,LID_Z-.01,POCKET_ROOF))
    for x,y in MOUNT+HANGERS+CASE_MOUNT:
        s = K.cut_cs(s,x,y,LID_TOP)
    return s.clean()


def wave(z, height):
    """Sewn, periodic spline representation; dimensions are axial projection."""
    def edge(r, lift):
        pts=[]
        for i in range(48):
            a=math.tau*i/48
            h=z+lift+(height-.4)*(1-math.cos(3*a))/2
            pts.append(cq.Vector(r*math.cos(a),r*math.sin(a),h))
        return cq.Edge.makeSpline(pts,periodic=True)
    ia,oa,ib,ob = edge(7,0),edge(10.5,0),edge(7,.4),edge(10.5,.4)
    faces=[cq.Face.makeRuledSurface(*edges) for edges in ((ia,oa),(ib,ob),(ia,ib),(oa,ob))]
    return cq.Solid.makeSolid(cq.Shell.makeShell(faces))


def bearing(od,z,width,inner_od,outer_id,ex,add,index):
    # Three separate reference solids so support contact can be checked per ring.
    # Seals/shield setbacks are reference envelopes, not seller measurements.
    p='C_upper_' if index else 'C_lower_'
    moving='cup' if index else 'fixed'
    add(p+'outer','686-2RS 外圈' if index else 'MR106ZZ 外圈','metal',
        ring(od,outer_id,z,z+width),moving,ex)
    add(p+'inner','686-2RS 内圈' if index else 'MR106ZZ 内圈','metal',
        ring(inner_od,6,z,z+width),'upper_inner' if index else 'lower_inner',ex)
    add(p+'seal','686-2RS 密封包络' if index else 'MR106ZZ 防尘盖包络','seal',
        ring(outer_id,inner_od,z+.12,z+width-.12),moving,ex)


@lru_cache(maxsize=1)
def cap():
    s = cyl(30,CAP_Z,CAP_Z+25).cut(cyl(20,CAP_Z-.01,CAP_BORE_Z))
    s = s.cut(cyl(6,CAP_BORE_Z-.01,CAP_Z+17))
    return s.cut(v6.along_y(3.18,2.9,15.1,0,CAP_SETSCREW_Z)).clean()


@lru_cache(maxsize=64)
def knob(angle=0, down=0, shim=0, detached=False):
    ps=[]
    def add(id,name,cat,s,group='fixed',ex=(0,0,0),pr=False,note=''):
        if group in ('rotor','cap'):
            s=s.rotate((0,0,0),(0,0,1),angle)
        if group in ('cup','upper_inner','pom_top') or (not detached and group in ('rotor','cap')):
            s=s.translate((0,0,-down))
        ps.append(Part(id,name,cat,s.clean(),ex,pr,group,note))

    add('P_case','PETG 底壳（v7 避让）','petg',case(),ex=(0,0,-36),pr=True)
    for id,name,s in zip(('C_pcb','C_sensor','C_fpc','C_capacitor'),
                         ('FluxDial 15×18×1.6','MT6701','FPC 插座','电容'), v6.board_parts()):
        add(id,name,'pcb' if id=='C_pcb' else 'sensor',s,ex=(0,0,-29))
    add('P_board_keeper','PETG 退板挡条（复用）','petg',v6.board_keeper(),ex=(0,-15,-18),pr=True)
    for a in (-1,1):
        add(f'S_keeper_{a}','M2×3 挡条沉头螺丝','metal',K.screw(9.8*a,-8.5,6.2,3,'cs'),ex=(a*8,-15,-8))
    add('C_shaft','Ø6×24 精密不锈钢光轴','metal',cyl(6,SHAFT_Z,SHAFT_TOP),'rotor',(0,0,62))
    add('P_rotor_hub','PETG 夹轴磁铁杯与下止挡','petg',hub(),'rotor',(0,0,-12),True)
    add('C_magnet','Ø6×2.5 径向充磁磁铁','magnet',cyl(6,MAGNET_Z,MAGNET_Z+2.5),'rotor',(0,0,-19))
    for label,x,z,y,offset in [('hub',5.42,9.38,2.75,-12),('top',4.7,25.56,2.3,80)]:
        if label=='top' and detached: continue
        add(f'S_{label}','M2×8 杯头夹轴螺丝','metal',
            K.screw(0,0,0,8).rotate((0,0,0),(1,0,0),-90).translate((x,y,z)),
            'rotor',(12,17,offset))
        add(f'N_{label}','M2 六角螺母 AF4','metal',v6.horizontal(v6.hexnut(0,0,0),x,-y-.06,z),
            'rotor',(12,-17,offset))
    bearing(10,MR_Z,3,MR_INNER_CONTACT_OD,MR_OUTER_CONTACT_ID,(0,0,9),add,0)
    bearing(13,UPPER_Z,5,UPPER_INNER_CONTACT_OD,UPPER_OUTER_CONTACT_ID,(0,0,58),add,1)
    for label,s,sign in zip(('left','right'),lower_seat(),(-1,1)):
        add('P_carrier_'+label,'PETG 固定座 '+('左半' if sign<0 else '右半'),'petg',s,ex=(sign*23,0,2),pr=True)
    add('P_cup','PETG 防转浮动杯','petg',upper_cup(),'cup',(0,0,48),True)
    if shim:
        add('W_tune','试装可选调节垫 ID15 OD20','brass',ring(20,15,DECK_Z,DECK_Z+shim))
    add('W_pom_1','白色 POM 15×20×1','pom',ring(20,15,DECK_Z+shim,DECK_Z+shim+1),ex=(0,0,20))
    add('W_wave','304 三波峰波簧 14×21×0.4','spring',wave(DECK_Z+shim+1,CUP_Z-down-DECK_Z-shim-2),ex=(0,0,28))
    add('W_pom_2','白色 POM 15×20×1','pom',ring(20,15,CUP_Z-1,CUP_Z),'pom_top',(0,0,37))
    for i,(x,y) in enumerate(HANGERS):
        add(f'T_hanger_{i}','M2×5 双通圆铜柱 Ø3.2','brass',ring(3.2,1.6,HANGER_BASE_Z,LID_Z,x,y),ex=(x*.6,y,37))
        add(f'W_hanger_{i}','M2 下平垫 2.2×5×0.3','metal',ring(5,2.2,HANGER_WASHER_BOTTOM_Z,HANGER_BASE_Z,x,y),ex=(x*.5,y,-11))
        z=HANGER_WASHER_BOTTOM_Z
        add(f'N_hanger_{i}','M2×3 铜柱下端杯头','metal',K.screw(x,y,z,3).rotate((x,y,z),(x+1,y,z),180),ex=(x*.5,y,-19))
        add(f'S_lid_{i}','M2×4 铜柱上端沉头','metal',K.screw(x,y,LID_TOP,4,'cs'),ex=(x*.65,y,55))
    for i,(x,y) in enumerate(CASE_MOUNT):
        add(f'I_case_{i}','M2×3 热熔铜嵌件','metal',K.insert(x,y,14.1),ex=(x*.5,y*.5,-19))
        add(f'S_case_{i}','M2×6 底壳沉头','metal',K.screw(x,y,LID_TOP,6,'cs'),ex=(x*.5,y*.5,60))
    add('A_lid','v7 铝封盖 38×38×2','aluminum',lid(),'cover',(0,0,68))
    for i,(x,y) in enumerate(MOUNT):
        add(f'S_service_{i}','M2×6 外圈维修沉头','metal',K.screw(x,y,LID_TOP,6,'cs'),'body_mount',(x,y,80))
    if not detached:
        add('P_top_clamp','PETG 一体肩上夹箍','petg',top_clamp(),'rotor',(0,0,77),True)
        add('C_cap','成品旋钮帽 Ø30×25 孔6','metal',cap(),'cap',(0,0,101))
        add('C_cap_set','帽随附顶丝（包络）','metal',v6.along_y(3,3,6,0,CAP_SETSCREW_Z),'cap',(0,23,101))
    return ps


B.COLORS.update(pom=(.95,.95,.89), spring=(.38,.53,.67), seal=(.19,.22,.25))


def knobs_global(angle=0,down=0):
    return [replace(p,id=f'K{i}_{p.id}',shape=v6.global_shape(p.shape,i,cx,cy),group=f'knob{i}_{p.group}')
            for i,(cx,cy) in enumerate(H.KC,1) for p in knob(angle,down)]


def full(mode='inf', slide=None, angle=0, down=0, key=0):
    x=H.RX if slide is None else slide
    env=v6.environment()
    env=[replace(p,shape=p.shape.translate((0,0,-key))) if p.group=='keys' else p for p in env]
    env=[replace(p,shape=p.shape.translate((x-H.RX,0,0))) if p.id=='M_block' else p for p in env]
    slider=[Part(p.id,p.name,p.category,p.shape.translate((x,H.RY,0)),p.ex,p.printable,'slider',p.reference)
            for p in Q.build(mode)]
    return env+slider+knobs_global(angle,down)


def allowed(a,b):
    return v6.allowed(a,b)


def audit(parts):
    old=B.allowed
    B.allowed=allowed
    try:
        return B.audit(parts)
    finally:
        B.allowed=old


if __name__=='__main__':
    import json
    check=audit(knob())
    print(json.dumps(check,indent=2),flush=True)
    raise SystemExit(bool(check['invalid'] or check['unintendedIntersections']))
