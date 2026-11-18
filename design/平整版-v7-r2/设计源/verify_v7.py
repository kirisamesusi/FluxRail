"""R2 contact path, adjustment and machine CAD verification."""
from dataclasses import replace
import itertools
import json
import math
import sys

import build_v7 as m


def dump(file, rows):
    p=m.ROOT/'验证'/file
    p.parent.mkdir(exist_ok=True)
    p.write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def hit(a,b):
    x,y=a.shape.BoundingBox(),b.shape.BoundingBox()
    if any(min(getattr(x,k+'max'),getattr(y,k+'max'))-max(getattr(x,k+'min'),getattr(y,k+'min'))<1e-6 for k in 'xyz'):
        return 0
    return a.shape.intersect(b.shape).Volume()


def valid(parts):
    check=m.audit(parts)
    assert not check['invalid'] and not check['unintendedIntersections'],check
    return check


def local():
    rows=[]
    for turn,travel in itertools.product(range(0,360,45),(0,.30,.60)):
        rows.append(dict(angle=turn,travel=travel,**valid(m.knob(turn,travel))))
    dump('转动与行程.json',rows)
    print('LOCAL',len(rows),flush=True)


def contacts():
    states=[]
    for travel in (0,.05,.30,.60):
        p={x.id:x.shape for x in m.knob(adjust=travel)}
        ds=lambda a,b: round(p[a].distance(p[b]),5)
        assert ds('S_brake_adjust','P_pressure_yoke')<.001
        assert ds('W_pom_1','C_brake_disc')<.001 and ds('W_pom_2','C_brake_disc')<.001
        assert ds('W_wave','P_pom_saddle')<.002
        assert abs(ds('W_wave','P_pressure_yoke')-max(.05-travel,0))<.002
        assert ds('P_pom_saddle','W_pom_2')<.001
        assert ds('P_case','W_pom_1')<.001
        assert abs(ds('C_magnet','C_sensor')-.60)<.001
        assert ds('P_pressure_yoke','P_cup')>.04
        assert ds('C_brake_disc','P_rotor_hub')>.05
        assert ds('C_brake_disc','P_case')>.05
        assert ds('C_brake_disc','P_pom_saddle')>.5
        assert ds('P_cup','C_upper_seal')>.1
        assert ds('P_rotor_hub','C_lower_seal')>.1
        assert ds('P_top_clamp','C_upper_seal')>.1
        assert ds('P_cup','A_lid')<.001
        contact=[]
        for id,probe in [('W_pom_1',-.02),('W_pom_2',.02)]:
            area=p['C_brake_disc'].translate((0,0,probe)).intersect(p[id]).Volume()/.02
            assert area>130,(id,area)
            contact.append(round(area,2))
        # Move only the coupling hub. It can slide through the disc so brake
        # clamp load need not travel through the shaft or bearing inner races.
        for z in (-.20,.20):
            assert p['P_rotor_hub'].translate((0,0,z)).intersect(p['C_brake_disc']).Volume()<1e-6
        states.append(dict(travelMM=travel,magnetGapMM=ds('C_magnet','C_sensor'),
                           springGapMM=ds('W_wave','P_pressure_yoke'),POMContactAreasMM2=contact,
                           fixedCupClearanceMM=ds('P_pressure_yoke','P_cup'),
                           floatingDiscHubProbeMM=[-.20,.20]))
    dump('阻尼受力与净空.json',states)
    print('CONTACT',len(states),flush=True)


def service():
    ps={p.id:p for p in m.knob()}
    rows=[]
    driver=m.Part('driver','Ø1.8 vertical driver','metal',m.cyl(1.8,21.72,75,m.ADJ_X,m.ADJ_Y))
    assert not [p.id for p in ps.values() if hit(driver,p)>1e-5]
    rows.append(dict(test='driver enters with cap installed',driverDiameterMM=1.8,localXY=[m.ADJ_X,m.ADJ_Y]))
    board=['C_pcb','C_sensor','C_fpc','C_capacitor']
    for y in (0,2,5,9,13,18,25):
        for id in board:
            p=replace(ps[id],shape=ps[id].shape.translate((0,-y,0)))
            assert hit(p,ps['P_case'])<1e-5,(y,id,hit(p,ps['P_case']))
        rows.append(dict(test='PCB out of separate tray',translationY=-y))
    rotor=['P_rotor_hub','C_shaft','C_magnet','S_hub_grub','N_hub']
    stationary=['P_case','C_brake_disc','W_pom_1','W_pom_2','P_pom_saddle']
    for z in (0,.2,.5,1,2,4,8,15):
        for id in rotor:
            p=replace(ps[id],shape=ps[id].shape.translate((0,0,z)))
            for keep in stationary:
                assert hit(p,ps[keep])<1e-5,(z,id,keep,hit(p,ps[keep]))
        rows.append(dict(test='shaft-and-hub up after removing yoke and wave spring',translationZ=z))
    for id in ('P_pressure_yoke','P_pom_saddle','C_brake_disc'):
        for z in (0,.3,1,3,6,12,18):
            p=replace(ps[id],shape=ps[id].shape.translate((0,0,z)))
            assert hit(p,ps['P_case'])<1e-5,(id,z)
            rows.append(dict(test='brake part out of open tray',id=id,translationZ=z))
    dump('工具与拆装.json',rows)
    print('SERVICE',len(rows),flush=True)


def machine():
    rows=[]
    for mode,x,angle,travel,key in [('inf',m.H.XMIN,0,0,0),('inf',m.H.XMAX,90,.6,4),('ongeki',m.H.RX,45,.3,4)]:
        rows.append(dict(mode=mode,slider=x,angle=angle,travel=travel,**valid(m.full(mode,x,angle,travel,key))))
        print('MACHINE',mode,flush=True)
    dump('整机.json',rows)
    old={p.id:p.shape for p in m.r1.knobs_global()}
    new={p.id:p.shape for p in m.knobs_global()}
    unchanged=[]
    for side in (1,2):
        for id in ('C_pcb','C_sensor','C_fpc','C_magnet','C_shaft','C_cap',*[f'S_service_{i}' for i in range(4)]):
            k=f'K{side}_{id}'
            assert abs(old[k].Volume()-new[k].intersect(old[k]).Volume())<1e-5,k
            unchanged.append(k)
    env=[p for p in m.full() if not p.id.startswith(('K1_','K2_'))]
    paths=m.v6.S4.extraction(env,m.knobs_global())
    assert not any(r['collisions'] for r in paths),paths
    dump('原接口与整套上取.json',dict(matched=unchanged,lifts=paths))
    print('MACHINE extraction',len(paths),flush=True)


if __name__=='__main__':
    group=sys.argv[1] if len(sys.argv)>1 else 'all'
    for name,fn in [('local',local),('contacts',contacts),('service',service),('machine',machine)]:
        if group in ('all',name): fn()
