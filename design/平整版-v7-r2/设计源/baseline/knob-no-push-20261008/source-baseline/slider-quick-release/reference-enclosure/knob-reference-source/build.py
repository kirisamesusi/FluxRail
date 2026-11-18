"""FluxRail V4 prototype CAD, MIT. All dimensions millimetres.

Nominal geometry and commercial component envelopes are explicitly distinguished.
CAD passing does not establish magnetic performance, friction, fatigue or printer fit.
"""
from pathlib import Path
from dataclasses import dataclass
import json, math, argparse, itertools
import cadquery as cq

ROOT=Path(__file__).resolve().parent
P=json.loads((ROOT/'parameters.json').read_text(encoding='utf-8'))

def box(x,y,z0,z1,cx=0,cy=0):
    return cq.Workplane('XY').box(x,y,z1-z0,centered=(True,True,False)).translate((cx,cy,z0)).val()

def cyl(d,z0,z1,x=0,y=0):
    return cq.Workplane('XY',origin=(x,y,z0)).circle(d/2).extrude(z1-z0).val()

def ring(od,ind,z0,z1,x=0,y=0):
    return cyl(od,z0,z1,x,y).cut(cyl(ind,z0,z1,x,y))

def unite(shapes):
    s=shapes[0]
    for t in shapes[1:]:s=s.fuse(t)
    return s.clean()

def cone(bottom_d,top_d,z0,h,x=0,y=0):
    return cq.Solid.makeCone(bottom_d/2,top_d/2,h,cq.Vector(x,y,z0))

def cut_cs(shape,x,y,top,depth=1.2):
    # Match the screw's 90 degree cone; enlarge only the rim clearance.
    return shape.cut(cyl(2.4,-10,55,x,y)).cut(cone(2.4,3.8,top-1.0,.7,x,y)).cut(cyl(4.2,top-.3,top+.01,x,y))

def insert(x,y,z):
    # Commercial insert envelope, no invented knurl or thread topology.
    return ring(P['insert_od'],2.0,z,z+P['insert_length'],x,y)

def screw(x,y,seat,length,kind='socket'):
    if kind=='socket':
        shaft=cyl(2,seat-length,seat,x,y)
        head=cyl(3.8,seat,seat+2,x,y)
        socket=cq.Workplane('XY',origin=(x,y,seat+1)).polygon(6,1.5/math.cos(math.pi/6)).extrude(1.01).val()
        return shaft.fuse(head.cut(socket)).clean()
    # DIN965 countersunk overall length includes head; 0.3 rim plus 0.9 cone.
    head=cone(2.0,3.8,seat-1.2,0.9,x,y).fuse(cyl(3.8,seat-0.3,seat,x,y))
    drive=box(2.4,0.6,seat-0.5,seat+0.01,x,y).fuse(box(0.6,2.4,seat-0.5,seat+0.01,x,y))
    return cyl(2,seat-length,seat-1.2,x,y).fuse(head.cut(drive)).clean()

def rotate_x_axis(s):
    return s.rotate((0,0,0),(0,1,0),90)

@dataclass
class Part:
    id:str
    name:str
    category:str
    shape:cq.Shape
    group:str
    explode:tuple
    printable:bool=False
    reference:str=''

def build(push=0.0,compression=0.0,angle=0.0):
    parts=[]
    def add(id,name,cat,shape,group='fixed',ex=(0,0,0),printable=False,ref=''):
        if group in ('carriage','rotor','cap'):shape=shape.translate((0,0,-push))
        if group=='rotor':shape=shape.rotate((0,0,0),(0,0,1),angle)
        parts.append(Part(id,name,cat,shape,group,ex,printable,ref))
    # Local bottom-shell replacement tile, with downward-open insert pockets.
    width=P['case_width'];inner=width-2*P['case_wall']
    guide_inner=13+P['guide_clearance_each_side'];guide_width=inner/2-guide_inner
    tile=box(width,width,-P['bottom_tile_thickness'],0)
    tile=tile.cut(box(8,18,-1.6,0,0,9)) # underside FPC connector/cable channel
    floor=box(width,width,0,P['case_floor'])
    floor=floor.cut(box(8,18,0,0.8,0,9))
    walls=box(width,width,P['case_floor'],P['case_height']).cut(box(inner,inner,P['case_floor'],P['case_height']+.01))
    base=unite([floor,walls])
    mc=P['mount_center']; sc=P['spring_center']
    for sx,sy in itertools.product((-1,1),repeat=2):
        x,y=mc*sx,mc*sy
        # Corner column: 5.6 x 5.6, enough material around a short M2 insert.
        post=box(5.6,5.6,0.8,17.5,x,y)
        # Base mounting is reached from below; it does not share an access
        # bore with the upper-stop insert.
        post=post.cut(cyl(2.4,0,17.51,x,y))
        post=post.cut(cyl(P['insert_pilot'],0,5,x,y))
        post=post.cut(cyl(P['insert_pilot'],12.1,17.51,x,y))
        base=base.fuse(post)
        base=base.cut(cyl(P['insert_pilot'],-.01,5,x,y))
        tile=tile.cut(cyl(2.4,-5,0,x,y)).cut(cone(3.8,2.4,-4.7,.7,x,y)).cut(cyl(4.2,-5.01,-4.7,x,y))
        add(f'I_mount_{sx}_{sy}','底壳M2热熔嵌件','metal',insert(x,y,2),ex=(sx*8,sy*8,-15))
        mount_screw=screw(0,0,0,10,'cs').rotate((0,0,0),(1,0,0),180).translate((x,y,-5))
        add(f'S_mount_{sx}_{sy}','底壳M2×10沉头螺丝','metal',mount_screw,ex=(sx*8,sy*8,-5))
        add(f'I_stop_{sx}_{sy}','上限位M2热熔嵌件','metal',insert(x,y,12.1),ex=(sx*12,sy*12,8))
        stop=box(7.2,7.2,17.5,20.1,11.4*sx,11.4*sy)
        stop=stop.intersect(box(inner,inner,17.5,20.1))
        for side in (-1,1):stop=stop.cut(box(guide_width,19,17.49,18.5,(inner/2+guide_inner)/2*side,0))
        stop=stop.cut(cyl(2.4,17.4,20.2,x,y))
        base=base.cut(cyl(4.4,20.1,22.3,x,y))
        add(f'P_stop_{sx}_{sy}','可拆上限位块','petg',stop,ex=(sx*18,sy*18,24),printable=True)
        add(f'S_stop_{sx}_{sy}','限位块M2×8杯头螺丝','metal',screw(x,y,20.1,8),ex=(sx*18,sy*18,36))
        xx,yy=sc*sx,sc*sy
        seat=cyl(3.8,0.8,4.3,xx,yy).fuse(ring(3.8,2.4,4.3,7.6,xx,yy)).fuse(cyl(1.0,4.3,10.3,xx,yy))
        base=base.fuse(seat)
        # Hollow swept-volume envelope; spring manufacturer coil count unverified.
        spring=ring(P['spring_od'],P['spring_od']-2*P['spring_wire'],4.3,8.3-push,xx,yy)
        add(f'C_spring_{sx}_{sy}','现货压簧包络 Ø2×5 / 线径0.2','spring',spring,ex=(sx*14,sy*14,4),ref='stock spring SKU/rate require confirmation')
    assert inner/2>guide_inner
    for side in (-1,1):
        base=base.fuse(box(inner/2-guide_inner,19,5.0,18.5,(inner/2+guide_inner)/2*side,0))
    # PCB fixation through independent base and its local bottom-shell tile.
    for sx,sy in itertools.product((-1,1),repeat=2):
        x,y=5*sx,5*sy
        base=base.cut(cyl(2.4,-0.01,0.81,x,y))
        tile=tile.cut(cyl(2.4,-5,0,x,y)).cut(cyl(P['insert_pilot'],-5.01,-0.6,x,y))
        add(f'I_pcb_{sx}_{sy}','小板M2热熔嵌件','metal',insert(x,y,-3.6),ex=(sx*9,sy*9,-16))
        add(f'S_pcb_{sx}_{sy}','小板M2×6杯头螺丝','metal',screw(x,y,2.4,6),ex=(sx*9,sy*9,-3))
    # Cable opening is formed in wall as well as floor.
    base=base.cut(box(8,4,-0.01,3.0,0,14))
    add('P_tile','底壳安装样块30×30×5','petg',tile,ex=(0,0,-34),printable=True)
    add('P_case','固定外壳含回位座/下限位','petg',base.clean(),ex=(38,0,0),printable=True)
    # Actual FluxDial XY export. Component Z envelopes are separately documented.
    R=json.loads((ROOT.parent/'fluxdial-measured.json').read_text(encoding='utf-8'))
    pts=[(7.5-x,7.5-y) for x,y in R['outline_normalized']]
    pcb=cq.Workplane('XY',origin=(0,0,.8)).polyline(pts).close().extrude(1.6).val()
    for h in R['holes']:
        x,y=h['center'];pcb=pcb.cut(cyl(h['diameter'],.79,2.41,7.5-x,7.5-y))
    add('C_pcb','FluxDial 15×18×1.6小板','pcb',pcb,ex=(0,0,-18),ref='Measured source/FluxDial.dxf; thickness assumed 1.6; electrical DRC separate')
    x,y=R['u4_pad_pattern_centre']
    add('C_sensor','MT6701 SOP8最大包络含装配余量','sensor',box(6.2,5.1,2.4,4.25,7.5-x,7.5-y),ex=(0,0,-8),ref='MagnTek Rev1.8 max A1.75 plus 0.10 assembly allowance; total A includes stand-off')
    x,y=R['fpc_signal_row_centre'];rear=y-.625
    add('C_fpc','C55172973 FPC座最大静态包络','sensor',box(12.1,5.45,2.4,4.6,7.5-x,7.5-rear+5.45/2),ex=(0,8,-12),ref='LAILAN 6P1.0 body with drawing tolerance and 0.1 assembly allowance; latch sweep not verified')
    pads=[r for r in R['copper_hatches'] if abs(r['size'][0]-.8)<.001 and abs(r['size'][1]-.9)<.001]
    x=sum((p['bbox_normalized'][0]+p['bbox_normalized'][2])/2 for p in pads)/2
    y=sum((p['bbox_normalized'][1]+p['bbox_normalized'][3])/2 for p in pads)/2
    add('C_capacitor','C37电容包络（高度待确认）','sensor',box(2.2,1.3,2.4,3.5,7.5-x,7.5-y),ex=(0,0,-10),ref='XY from actual C37 lands; height1.1 assumed, exact part number not supplied')
    # Moving carrier: split-clamped bearing bores, not a load-bearing sensor PCB.
    carrier=ring(15.6,9.4,10.5,17.3)
    carrier=carrier.cut(cyl(P['bearing_seat_diameter'],10.7,13.7))
    carrier=carrier.cut(cyl(P['bearing_seat_diameter'],14.1,17.1))
    carrier=carrier.fuse(box(26,26,16.0,17.5).cut(cyl(15.2,17.3,17.51)).cut(cyl(10.04,15.9,17.3)))
    for sx,sy in itertools.product((-1,1),repeat=2):
        carrier=carrier.fuse(box(4.0,4.0,8.3,16.0,sc*sx,sc*sy))
        carrier=carrier.cut(box(12,12,0,30,15.15*sx,15.15*sy))
    for y in (-6.2,6.2):
        carrier=carrier.fuse(box(26,4.8,10,15,0,y))
    carrier=carrier.cut(cyl(9.4,10,15)).cut(cyl(P['bearing_seat_diameter'],10.7,13.7)).cut(cyl(P['bearing_seat_diameter'],14.1,17.1))
    for sx,sy in itertools.product((-1,1),repeat=2):
        # Clamp beams must not reintroduce a thin strip into the corner relief.
        carrier=carrier.cut(box(12,12,0,30,15.15*sx,15.15*sy))
    carrier=carrier.cut(cyl(15.8,8.2,10.4))
    for sx,sy in itertools.product((-1,1),repeat=2):
        carrier=carrier.fuse(box(2.0,3.0,17.5,26.2,11*sx,5.5*sy))
    for y in (-11,11):
        carrier=carrier.cut(cyl(2.4,15.5,17.51,0,y)).cut(cyl(5.4,17.1,17.51,0,y))
    for y in (-6.2,6.2):
        # Split clamp screws lie outside the bearings and the spring columns.
        horizontal=rotate_x_axis(cyl(2.4,-14,14)).translate((0,y,12.5))
        carrier=carrier.cut(horizontal)
        csk=rotate_x_axis(cone(3.8,2.4,-12.7,.7)).fuse(rotate_x_axis(cyl(4.2,-13.01,-12.7))).translate((0,y,12.5))
        carrier=carrier.cut(csk)
        pocket=rotate_x_axis(cyl(P['insert_pilot'],9,13.01)).translate((0,y,12.5))
        carrier=carrier.cut(pocket)
        add(f'I_carrier_{y}','滑架横向M2热熔嵌件','metal',rotate_x_axis(insert(0,0,9)).translate((0,y,12.5)),'carriage',(24,y*2,25))
        # Turn a conventional Z screw so its head opens on the left.
        h=screw(0,0,0,25,'cs').rotate((0,0,0),(0,1,0),-90).translate((-13,y,12.5))
        add(f'S_carrier_{y}','滑架M2×25沉头螺丝','metal',h,'carriage',(-24,y*2,25))
    carrier=carrier.clean()
    for sx,sy in itertools.product((-1,1),repeat=2):
        carrier=carrier.cut(cyl(1.6,8.2,26.3,sc*sx,sc*sy))
    left=carrier.intersect(box(40,60,0,40,-20.1,0))
    right=carrier.intersect(box(40,60,0,40,20.1,0))
    add('P_carrier_left','滑架左半含止转导向','petg',left,'carriage',(-22,0,20),True)
    add('P_carrier_right','滑架右半含止转导向','petg',right,'carriage',(22,0,20),True)
    for number,z in enumerate((10.7,14.1),1):
        add(f'C_bearing_{number}','MR106ZZ 6×10×3','metal',ring(10,6,z,z+3),'carriage',(0,0,25+number*9),ref='bearing external envelope only')
    add('P_inner_spacer','两轴承内圈间隔0.4','petg',ring(P['inner_spacer_od'],6.08,13.7,14.1),'rotor',(0,0,43),True)
    add('P_lower_spacer','止退夹至下轴承内圈垫0.4','petg',ring(P['inner_spacer_od'],6.08,10.2,10.6),'rotor',(0,0,22),True)
    # Retainer teeth are not specified. Conservative envelope is more truthful
    # than an invented tooth model and catches clearance to stationary features.
    retainer=ring(P['retainer_od_envelope'],6.0,P['retainer_bottom'],P['retainer_bottom']+P['retainer_height_envelope'])
    add('C_retainer','Ø6无槽轴用梅花止退夹包络','metal',retainer,'rotor',(0,0,13),ref='assembled OD<=15.3, height<=1.5; exact Taobao SKU unconfirmed')
    add('C_shaft','Ø6光轴35mm','metal',cyl(P['shaft_diameter'],P['shaft_bottom'],P['shaft_bottom']+P['shaft_length']),'rotor',(0,0,42),ref='requires h6/g6-compatible actual shaft; length35 buy/cut stock rod')
    cup=cyl(8.4,5.3,8.6).cut(cyl(6.12,5.9,8.61)).cut(cyl(6.08,8.4,8.61))
    add('P_magnet_cup','磁铁保护杯0.6底','petg',cup,'rotor',(0,0,5),True)
    add('C_magnet','Ø6×2.5径向充磁磁铁','magnet',cyl(6,5.9,8.4),'rotor',(0,0,-1),ref='MagnTek reference diameter/height; supplier magnetic grade unconfirmed')
    # Heat-set radial insert provides a real shaft lock rather than a loose disk.
    hz=P['hub_bottom'];ht=hz+P['hub_height'];mid=hz+P['hub_height']/2
    hub=ring(13,P['hub_bore'],hz,ht).fuse(ring(P['inner_spacer_od'],P['hub_bore'],17.1,hz))
    radial_pilot=rotate_x_axis(cyl(P['insert_pilot'],3.3,7.2)).translate((0,0,mid))
    radial_clear=rotate_x_axis(cyl(2.2,0,3.31)).translate((0,0,mid))
    hub=hub.cut(radial_pilot).cut(radial_clear)
    add('P_rotor_hub','金属盘驱动轮毂/轴锁紧','petg',hub,'rotor',(0,0,72),True)
    add('I_hub','轮毂径向M2热熔嵌件','metal',rotate_x_axis(insert(0,0,3.3)).translate((0,0,mid)),'rotor',(18,0,72))
    grub=rotate_x_axis(cyl(2,3,7)).translate((0,0,mid))
    add('S_hub','轮毂M2×4平端顶丝','metal',grub,'rotor',(24,0,72))
    dt=ht+P['washer_thickness']
    add('C_disc','M6大平垫摩擦盘6.4×18×1.8','metal',ring(P['washer_od'],P['washer_id'],ht,dt),'rotor',(0,0,87),ref='ISO7093-1 nominal dimensions, epoxy to hub central area')
    def plate(z0,lower):
        s=box(26,26,z0,z0+1.6).cut(cyl(14.8,z0-.01,z0+1.61))
        for sx,sy in itertools.product((-1,1),repeat=2):
            s=s.cut(box(14,14,z0-.01,z0+1.61,14.5*sx,14.5*sy))
            s=s.cut(box(2.4,3.4,z0-.01,z0+1.61,11*sx,5.5*sy))
        for y in (-11,11):
            if lower:
                s=s.fuse(cyl(5.0,z0-1.4,z0+1.6,0,y))
                s=s.cut(cyl(P['insert_pilot'],z0-1.4,z0+1.61,0,y))
                s=s.cut(cyl(2.4,z0-1.41,z0+1.61,0,y))
            else:s=cut_cs(s,0,y,z0+1.6)
        if lower:
            # A real radial tool opening to the hub's set screw at z19.65.
            s=s.cut(box(12,2.8,z0-.01,z0+1.61,12,0))
        return s.clean()
    lower_z=ht-P['felt_thickness']-1.6+compression;upper_z=dt+P['felt_thickness']-compression
    add('P_lower_plate','浮动下毛毡承板含嵌件座','petg',plate(lower_z,True),'carriage',(0,0,59),True)
    add('P_upper_plate','浮动上毛毡压板','petg',plate(upper_z,False),'carriage',(0,0,109),True)
    for y in (-11,11):
        add(f'I_adjust_{y}','阻尼下承板M2热熔嵌件','metal',insert(0,y,lower_z-1.4),'carriage',(0,y/2,57))
        add(f'S_adjust_{y}','阻尼M2×10沉头调节螺丝','metal',screw(0,y,upper_z+1.6,10,'cs'),'carriage',(0,y/2,126))
    add('F_lower','下羊毛毡环14.8×17.6×1','felt',ring(P['felt_od'],P['felt_id'],ht-P['felt_thickness']+compression,ht),'carriage',(0,0,80))
    add('F_upper','上羊毛毡环14.8×17.6×1','felt',ring(P['felt_od'],P['felt_id'],dt,dt+P['felt_thickness']-compression),'carriage',(0,0,98))
    cb=P['cap_bottom'];cf=cb+P['cap_recess_depth']
    cap=cyl(P['cap_diameter'],cb,cb+P['cap_height']).cut(cyl(P['cap_recess_diameter'],cb,cf)).cut(cyl(P['cap_bore_diameter'],cf,cf+P['cap_bore_depth_from_recess']))
    add('C_cap','已选铝合金帽30×25/孔6','metal',cap,'cap',(0,0,144),ref='vendor screenshot; E/G interpretation provisional, cap supplied setscrew unspecified')
    return parts

def save_parts(parts):
    for folder in ('parts-step','print-stl','print-step'): (ROOT/folder).mkdir(exist_ok=True)
    assembly=cq.Assembly(name='FluxRail_knob_v4')
    exploded=cq.Assembly(name='FluxRail_knob_v4_exploded')
    seen={}
    materials={'petg':(.15,.4,.55),'metal':(.7,.7,.72),'felt':(.75,.45,.18),'pcb':(.15,.5,.2),'sensor':(.2,.2,.2),'magnet':(.5,.25,.5),'spring':(.55,.55,.6)}
    bom=[]
    for p in parts:
        assert p.shape.isValid(),p.id
        cq.exporters.export(p.shape,str(ROOT/'parts-step'/f'{p.id}.step'))
        color=cq.Color(*materials[p.category])
        assembly.add(p.shape,name=p.id,color=color)
        exploded.add(p.shape.translate(p.explode),name=p.id,color=color)
        b=p.shape.BoundingBox()
        bom.append(dict(id=p.id,name=p.name,material=p.category,printable=p.printable,
                        reference=p.reference,bounds_mm=[round(b.xlen,3),round(b.ylen,3),round(b.zlen,3)],
                        volume_mm3=round(p.shape.Volume(),3),solid_count=len(p.shape.Solids())))
        if p.printable:
            # Independent local coordinates; base flat on printer bed. Preserve
            # assembly STEP separately so editable parts retain their placement.
            s=p.shape
            if p.id=='P_carrier_left':s=s.rotate((0,0,0),(0,1,0),90)
            if p.id=='P_carrier_right':s=s.rotate((0,0,0),(0,1,0),-90)
            if p.id in ('P_rotor_hub','P_lower_plate'):s=s.rotate((0,0,0),(1,0,0),180)
            pb=s.BoundingBox();s=s.translate((-pb.xmin,-pb.ymin,-pb.zmin))
            cq.exporters.export(s,str(ROOT/'print-stl'/f'{p.id}.stl'),tolerance=.03,angularTolerance=.12)
            cq.exporters.export(s,str(ROOT/'print-step'/f'{p.id}.step'))
    assembly.export(str(ROOT/'assembled.step'))
    exploded.export(str(ROOT/'exploded.step'))
    (ROOT/'bom.json').write_text(json.dumps(bom,ensure_ascii=False,indent=2),encoding='utf-8')
    # Store exact mesh vertices from OCCT, not artist approximations.
    meshes=[]
    for p in parts:
        v,f=p.shape.tessellate(.12,.22)
        meshes.append(dict(id=p.id,name=p.name,category=p.category,group=p.group,ex=p.explode,
                           v=[[round(a.x,3),round(a.y,3),round(a.z,3)] for a in v],f=f))
    (ROOT/'meshes.json').write_text(json.dumps(meshes,ensure_ascii=False,separators=(',',':')),encoding='utf-8')

def overlap_checks(parts):
    overlaps=[]
    for a,b in itertools.combinations(parts,2):
        aa,bb=a.shape.BoundingBox(),b.shape.BoundingBox()
        if any(getattr(aa,k+'max')<=getattr(bb,k+'min')+1e-6 or getattr(bb,k+'max')<=getattr(aa,k+'min')+1e-6 for k in ('x','y','z')):continue
        vol=a.shape.intersect(b.shape).Volume()
        if vol>1e-5:
            # Inserts intentionally replace polymer during heat installation;
            # treat only their defined pilot interference as planned contact.
            pair={a.id,b.id}
            allowed=any((i.startswith(('I_mount_','I_stop_')) and 'P_case' in pair) or
                        (i.startswith('I_pcb_') and 'P_tile' in pair) or
                        (i.startswith('I_carrier_') and 'P_carrier_right' in pair) or
                        (i=='I_hub' and 'P_rotor_hub' in pair) or
                        (i.startswith('I_adjust_') and 'P_lower_plate' in pair) for i in pair)
            overlaps.append(dict(a=a.id,b=b.id,volume_mm3=round(vol,5),allowed_heat_install=allowed))
    return overlaps

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--check-only',action='store_true');args=parser.parse_args()
    parts=build()
    if not args.check_only:save_parts(parts)
    overlaps=overlap_checks(parts)
    (ROOT/'initial-overlaps.json').write_text(json.dumps(overlaps,ensure_ascii=False,indent=2),encoding='utf-8')
    for item in overlaps:print(item)
    print(f'{len(parts)} components; {sum(p.printable for p in parts)} printed instances; {sum(not x["allowed_heat_install"] for x in overlaps)} unintended overlaps')
