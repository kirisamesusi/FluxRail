"""FluxRail v7 r2 brake cartridge. MIT. Local axis origin, dimensions in mm."""
from dataclasses import replace
from functools import lru_cache

import build_r1_reference as r1

cq, SOURCE, ROOT = r1.cq, r1.SOURCE, r1.ROOT
B,H,K,Q,Part,v6 = r1.B,r1.H,r1.K,r1.Q,r1.Part,r1.v6
box,cyl,ring = r1.box,r1.cyl,r1.ring
MOUNT,CASE_MOUNT,HANGERS = r1.MOUNT,r1.CASE_MOUNT,r1.HANGERS
LID_TOP,LID_Z = r1.LID_TOP,r1.LID_Z
GUIDE_BORE,POCKET_OD,POCKET_ROOF = r1.GUIDE_BORE,r1.POCKET_OD,r1.POCKET_ROOF
GUIDE_OD,UPPER_BORE = r1.GUIDE_OD,r1.UPPER_BORE
CAP_Z,CAP_BORE_Z,SHAFT_TOP = r1.CAP_Z,r1.CAP_BORE_Z,r1.SHAFT_TOP
ADJ_X,ADJ_Y = 6,-14.9
TRAVEL=.60
POM_Z=8.4
DISC_Z=9.4
UPPER_POM_Z=10.4
SADDLE_Z=11.4
WAVE_Z=12.15
PUSH_Z=14.20
GUIDE_X=12.55
MR_Z=13.45
CUP_BASE_Z=16.85
KEY_TOP=18.10
wave=r1.wave

PARAMS={**r1.PARAMS,'revision':'flat-knob-v7-r2-drag-trial-2026-11-06',
        'nominalDownHardStopTravel':0,'nominalUpServiceStopTravel':0,
        'springSeatsZ':[WAVE_Z,PUSH_Z],
        'lowerBearing':{**r1.PARAMS['lowerBearing'],'z':[MR_Z,MR_Z+3]},
        'wave':{**r1.PARAMS['wave'],'freeHeightAssumption':2.0,
                'gapAtMinimumDrag':.05,'compressedHeightAtTravelLimit':1.45},
        'brake':{'screwXY':[ADJ_X,ADJ_Y],'screw':'M2x6 socket, lid M2x0.4 tapped',
                 'travelMM':TRAVEL,'rotor':['brass',20.4,1,DISC_Z],
                 'POMfaceZ':[DISC_Z,UPPER_POM_Z],'bearingCup':'fixed',
                 'friction':'keyed POM pair and axially floating rotor disc'}}


def pom(z):
    s=ring(20,15,z,z+1)
    for side in (-1,1):
        s=s.cut(box(2.3,1.8,z-.01,z+1.01,side*10.2,0))
    return s.clean()


def disk():
    s=ring(20.4,12.96,DISC_Z,DISC_Z+1)
    for side in (-1,1):
        s=s.cut(box(1.35,2.06,DISC_Z-.01,DISC_Z+1.01,side*6.63,0))
    return s.clean()


@lru_cache(maxsize=1)
def magnet_hub():
    s=cyl(8.4,7.25,8.85).fuse(cyl(12.60,8.65,12.60))
    s=s.fuse(cyl(8.4,12.60,MR_Z-.40)).fuse(cyl(r1.MR_INNER_CONTACT_OD,MR_Z-.40,MR_Z))
    for side in (-1,1):
        s=s.fuse(box(1.32,1.80,8.95,11.08,side*6.38,0))
    s=s.cut(cyl(6.12,7.24,9.75)).cut(cyl(6.1,10.35,MR_Z+.01))
    s=s.cut(v6.along_y(2.25,2.99,9,0,10.65))
    s=s.cut(r1.nut_pocket(0,5.77,10.65))
    # The nut enters axially from above before the MR106ZZ is installed.
    s=s.cut(box(4.9,1.76,10.65,12.62,0,4.89))
    return s.clean()


def pressure_guides(s,z0,z1):
    for sign in (-1,1):
        s=s.fuse(box(4.0,4.3,z0,z1,sign*12.15,0))
        s=s.cut(box(2.28,3.08,z0-.01,z1+.01,sign*GUIDE_X,0))
    return s


@lru_cache(maxsize=1)
def saddle():
    s=pressure_guides(ring(21.3,14.3,SADDLE_Z,WAVE_Z),SADDLE_Z,WAVE_Z)
    for sign in (-1,1):
        s=s.fuse(box(1.55,1.45,11.10,11.41,sign*9.9,0))
    return s.clean()


@lru_cache(maxsize=1)
def pressure_yoke():
    s=ring(22.45,14.32,PUSH_Z,15.35)
    for sign in (-1,1):
        s=s.fuse(box(3.75,4.3,12.80,16.45,sign*12.275,0)
                 .cut(cyl(21.20,12.79,16.46))
                 .cut(cyl(22.25,15.34,16.46)))
        s=s.cut(box(2.28,3.08,12.79,16.46,sign*GUIDE_X,0))
    s=s.fuse(box(4.3,4.3,PUSH_Z,15.35,4.6,-11.45))
    s=s.fuse(box(3.4,.75,13.69,14.75,6,-13.275))
    s=s.fuse(box(3.4,3.0,12.10,13.70,6,-14.35))
    return s.clean()


@lru_cache(maxsize=1)
def tray():
    s=r1.case()
    s=s.cut(box(4.2,4.5,11.45,17.11,ADJ_X,-15.35))
    deck=ring(23.25,14.7,7.5,POM_Z).fuse(ring(23.25,20.86,POM_Z,9.29))
    for sign in (-1,1):
        deck=deck.fuse(box(1.8,1.48,POM_Z,9.25,sign*9.98,0))
        s=s.fuse(box(4.8,5.4,3.2,8.4,sign*11.4,0))
        s=s.fuse(box(1.96,2.76,8.39,16.7,sign*GUIDE_X,0))
    return s.fuse(deck).clean()


@lru_cache(maxsize=1)
def carrier():
    s=cyl(13.2,12.70,CUP_BASE_Z)
    for sign in (-1,1):
        s=s.fuse(box(r1.KEY_WIDTH,r1.KEY_LENGTH,CUP_BASE_Z,KEY_TOP,sign*r1.KEY_X,0))
    for y in (-6.6,6.6):
        s=s.fuse(box(25.6,4.2,15.65,16.4,0,y))
    for x,y in HANGERS:
        s=s.fuse(cyl(5.5,r1.HANGER_BASE_Z,LID_Z,x,y))
    for x,y in HANGERS:
        s=s.cut(cyl(3.42,12.08,LID_Z+.01,x,y))
    for x,y in CASE_MOUNT:
        s=s.cut(cyl(7.24,12.08,LID_Z+.01,x,y))
    s=s.cut(cyl(r1.MR_OUTER_CONTACT_ID,12.68,CUP_BASE_Z+.01))
    s=s.cut(cyl(r1.MR_BORE,MR_Z,MR_Z+3))
    return tuple(s.intersect(box(40,50,0,40,side*20.06,0)).clean() for side in (-1,1))


@lru_cache(maxsize=1)
def bearing_cup():
    s=cyl(r1.FLANGE_OD,CUP_BASE_Z,POCKET_ROOF).fuse(cyl(GUIDE_OD,POCKET_ROOF,r1.UPPER_Z+5))
    s=s.cut(cyl(8.9,CUP_BASE_Z-.01,17.15)).cut(cyl(r1.UPPER_OUTER_CONTACT_ID,17.15,r1.UPPER_Z+.01))
    s=s.cut(cyl(UPPER_BORE,r1.UPPER_Z,r1.UPPER_Z+5+.02))
    for sign in (-1,1):
        s=s.cut(box(r1.KEY_WIDTH+.34,r1.KEY_LENGTH+.36,CUP_BASE_Z-.01,KEY_TOP+.04,sign*r1.KEY_X,0))
    return s.clean()


@lru_cache(maxsize=1)
def lid():
    return r1.lid().cut(cyl(1.60,LID_Z-.01,LID_TOP+.01,ADJ_X,ADJ_Y)).clean()


@lru_cache(maxsize=24)
def knob(angle=0,adjust=0,detached=False):
    shapes={'P_case':tray(),'P_rotor_hub':magnet_hub(),'P_carrier_left':carrier()[0],
            'P_carrier_right':carrier()[1],'P_cup':bearing_cup(),'A_lid':lid()}
    retired={'W_pom_1','W_pom_2','W_wave','S_hub','N_hub'}
    names={'P_case':'PETG 底壳与 POM 制动座','P_rotor_hub':'PETG 磁铁杯与浮动盘驱动键',
           'P_cup':'PETG 固定上轴承杯','A_lid':'v7 r2 铝封盖'}
    ps=[]
    def add(id,name,material,s,group='fixed',ex=(0,0,0),printable=False):
        if group in ('rotor','cap'):
            s=s.rotate((0,0,0),(0,0,1),angle)
        ps.append(Part(id,name,material,s.clean(),ex,printable,group,''))
    for p in r1.knob(detached=detached):
        if p.id not in retired:
            shape=shapes.get(p.id,p.shape)
            if p.id.startswith('C_lower_'):
                shape=shape.translate((0,0,MR_Z-r1.MR_Z))
            add(p.id,names.get(p.id,p.name),p.category,shape,p.group,p.ex,p.printable)
    add('S_hub_grub','磁铁杯 M2×3 内六角紧定螺钉','metal',v6.along_y(2,3,6,0,10.65),'rotor',(0,19,-10))
    add('N_hub','磁铁杯 M2 AF4 螺母','metal',v6.horizontal(v6.hexnut(0,0,0),0,5.70,10.65),
        'rotor',(0,19,-14))
    add('W_pom_1','开防转缺口的 POM 15×20×1','pom',pom(POM_Z),ex=(0,0,12))
    add('C_brake_disc','黄铜浮动摩擦盘 Ø20.4×1','brass',disk(),'rotor',(0,0,22))
    add('W_pom_2','开防转缺口的 POM 15×20×1','pom',pom(UPPER_POM_Z),ex=(0,0,30))
    add('P_pom_saddle','PETG 上 POM 防转托环','petg',saddle(),ex=(0,0,38),printable=True)
    wave_h=min(2.0,PUSH_Z-adjust-WAVE_Z)
    add('W_wave','304 三波峰波簧 14×21×0.4','spring',wave(WAVE_Z,wave_h),ex=(0,0,43))
    add('P_pressure_yoke','PETG 导向压环','petg',pressure_yoke().translate((0,0,-adjust)),ex=(0,0,49),printable=True)
    add('S_brake_adjust','外部调阻尼 M2×6 杯头','metal',K.screw(ADJ_X,ADJ_Y,LID_TOP+TRAVEL-adjust,6),ex=(0,-8,84))
    return ps


def knobs_global(angle=0,adjust=0):
    return [replace(p,id=f'K{i}_{p.id}',shape=v6.global_shape(p.shape,i,cx,cy),group=f'knob{i}_{p.group}')
            for i,(cx,cy) in enumerate(H.KC,1) for p in knob(angle,adjust)]


def full(mode='inf',slide=None,angle=0,adjust=0,key=0):
    x=H.RX if slide is None else slide
    env=v6.environment()
    env=[replace(p,shape=p.shape.translate((0,0,-key))) if p.group=='keys' else p for p in env]
    env=[replace(p,shape=p.shape.translate((x-H.RX,0,0))) if p.id=='M_block' else p for p in env]
    slider=[Part(p.id,p.name,p.category,p.shape.translate((x,H.RY,0)),p.ex,p.printable,'slider',p.reference)
            for p in Q.build(mode)]
    return env+slider+knobs_global(angle,adjust)


def allowed(a,b):
    for prefix in ('','K1_','K2_'):
        ids={p.id.removeprefix(prefix) for p in (a,b) if p.id.startswith(prefix)}
        if ids in ({'A_lid','S_brake_adjust'},{'P_rotor_hub','S_hub_grub'}):
            return True
    return r1.allowed(a,b)


def audit(parts):
    old=B.allowed
    B.allowed=allowed
    try:
        return B.audit(parts)
    finally:
        B.allowed=old


if __name__=='__main__':
    for a,t in [(0,0),(31,.3),(90,.6)]:
        ps=knob(a,t)
        r=audit(ps)
        print(len(ps),a,t,r['unintendedIntersections'],r['invalid'],flush=True)
