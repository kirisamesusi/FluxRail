"""MIT. FluxRail enclosure prototype; mm, base underside Z=0.
Commercial parts are labelled envelopes. CAD evidence is not physical acceptance.
"""
from pathlib import Path
from dataclasses import dataclass
import json, math, sys, importlib.util, csv
import cadquery as cq
ROOT=Path(__file__).resolve().parent
P=json.loads((ROOT/'parameters.json').read_text(encoding='utf-8'))
D=json.loads((ROOT/'pcb-extracted.json').read_text(encoding='utf-8'))
D['board_vertices_mm']=D['outline_mm']
spec=importlib.util.spec_from_file_location('flux_knob_reference',ROOT/'knob-reference-source/build.py')
K=importlib.util.module_from_spec(spec);sys.modules[spec.name]=K;spec.loader.exec_module(K)
box,cyl,ring,screw=K.box,K.cyl,K.ring,K.screw
W=max(v[0] for v in D['board_vertices_mm']);H=max(v[1] for v in D['board_vertices_mm'])
ZPCB=P['base_thickness']+P['pcb_lower_standoff'];ZPT=ZPCB+P['pcb_thickness']
ZFOAM=ZPT+P['foam_thickness'];ZLOC=ZFOAM+P['locator_thickness'];ZTOP=ZLOC+P['top_thickness']
RB=ZPT+P['rail_upper_standoff']+P['rail_pad_thickness'];BT=RB+P['block_under_gap_assumption']+P['block_body_height_envelope']
RX,RY=D['rail_holes_mm'][1][:2];RL=RX-P['rail_length']/2;RR=RX+P['rail_length']/2
XMIN=RL+P['rail_stop_thickness']+P['block_length']/2;XMAX=RR-P['rail_stop_thickness']-P['block_length']/2
KC=[(17.5,H+P['knob_back_offset']),(W-17.5,H+P['knob_back_offset'])]
OUT=[-P['side_margin'],-P['front_margin'],W+P['side_margin'],H+P['rear_margin']]
PERIM=[(-4.5,-4.5),(W+4.5,-4.5),(-4.5,H+14.5),(W+4.5,H+14.5),(-6,45),(-6,95),(W+6,45),(W+6,95),(60,-6),(140,-6),(75,H+16),(125,H+16)]
COLORS={'petg':(.16,.42,.55),'metal':(.64,.67,.70),'brass':(.75,.55,.22),'acrylic':(.91,.91,.86),'foam':(.16,.17,.19),'pcb':(.10,.45,.27),'sensor':(.15,.15,.17),'magnet':(.48,.31,.53),'felt':(.70,.47,.22),'spring':(.55,.58,.62),'keycap':(.82,.83,.79)}
@dataclass
class Part:
    id:str;name:str;category:str;shape:cq.Shape;ex:tuple=(0,0,0);printable:bool=False;group:str='fixed';reference:str=''
def rounded(w,h,z0,z1,cx=0,cy=0,r=2):
    wp=cq.Workplane('XY').box(w,h,z1-z0,centered=(True,True,False))
    if r:wp=wp.edges('|Z').fillet(r)
    return wp.translate((cx,cy,z0)).val()
def outer(z0,z1):
    return rounded(OUT[2]-OUT[0],OUT[3]-OUT[1],z0,z1,(OUT[0]+OUT[2])/2,(OUT[1]+OUT[3])/2,P['outer_radius'])
def pcb_outline(z0,z1):
    return cq.Workplane('XY',origin=(0,0,z0)).polyline(D['board_vertices_mm']).close().extrude(z1-z0).val()
def hole(s,x,y,d=2.4,z0=-1,z1=65):return s.cut(cyl(d,z0,z1,x,y))
def cs(s,x,y,top):return K.cut_cs(s,x,y,top)
def hexcol(x,y,z0,length,male=False):
    s=cq.Workplane('XY',origin=(x,y,z0)).polygon(6,3/math.cos(math.pi/6)).extrude(length).val()
    s=hole(s,x,y,2,z0+length-3 if male else z0,z0+length+.01)
    if male:s=s.fuse(cyl(2,z0-3,z0,x,y))
    return s.clean()
def panel_cutouts(s,z0,z1,locator=False):
    for x,y in D['switch_centres_mm']:
        s=s.cut(box(14.1 if locator else 18.6,14.1 if locator else 18.6,z0-.01,z1+.01,x,y))
        if locator:s=s.cut(box(16.2,16.2,z1-.5,z1+.01,x,y))
    s=s.cut(rounded(106,29.4,z0-.01,z1+.01,RX,RY,2))
    for x,y in KC:s=s.cut(box(31.2,31.2,z0-.01,z1+.01,x,y))
    for x,y in PERIM:s=hole(s,x,y)
    return s
def knob_adapt(push=0,compression=0,angle=0):
    result=[]
    for idx,(cx,cy) in enumerate(KC):
        for p in K.build(push,compression,angle):
            if p.id=='P_tile' or p.id.startswith(('I_pcb_','I_mount_','I_stop_','S_mount_')):continue
            s=p.shape
            if p.id.startswith('P_stop_'):
                original=s
                for lift in (1,P['knob_stop_top_extension']):s=s.fuse(original.translate((0,0,lift)))
                s=s.clean()
            if p.id.startswith('S_stop_'):
                fields=p.id.split('_');x=float(fields[-2])*K.P['mount_center'];y=float(fields[-1])*K.P['mount_center']
                s=screw(x,y,20.1+P['knob_stop_top_extension'],25)
            if p.id.startswith('S_pcb_'):
                fields=p.id.split('_');x=float(fields[-2])*5;y=float(fields[-1])*5
                s=screw(x,y,2.4,P['knob_pcb_screw_length'])
            if p.id=='P_case':
                # V2: flat metal base; route FPC above it via the printed side window.
                s=s.fuse(box(8,18,0,.8,0,9)).cut(box(8,4,1,5.5,0,14)).clean()
                # Clear the actual 15x18 board tongue, keeping the .8 mm floor.
                r=json.loads((ROOT/'fluxdial-measured.json').read_text(encoding='utf-8'))
                pts=[(7.5-x,7.5-y) for x,y in r['outline_normalized']]
                clearance=cq.Workplane('XY',origin=(0,0,.8)).polyline(pts).close().offset2D(.3).extrude(1.9).val()
                s=s.cut(clearance).clean()
                for x in (-12.2,12.2):
                    for y in (-12.2,12.2):s=s.cut(cyl(4.4,20.1,22.3+P['knob_stop_top_extension'],x,y))
            s=s.rotate((0,0,0),(0,0,1),180).translate((cx,cy,P['base_thickness']))
            name='旋钮贯穿固定M2×25圆柱头螺丝' if p.id.startswith('S_stop_') else '小板M2×5杯头螺丝' if p.id.startswith('S_pcb_') else p.name
            result.append(Part(f'K{idx+1}_{p.id}',name,p.category,s,(0,0,p.explode[2]*.5+20),p.printable,f'knob{idx+1}_{p.group}',p.reference))
    return result
def build(slide=None,push=0,compression=0,angle=0,key_press=0):
    parts=[]
    def add(id,name,cat,s,ex=(0,0,0),printable=False,group='fixed',ref=''):
        parts.append(Part(id,name,cat,s.clean(),ex,printable,group,ref))
    taps=[(*v[:2],'PCB') for v in D['pcb_holes_mm']]+[(*v[:2],'RAIL') for v in D['rail_holes_mm']]+[(x,y,'RIM') for x,y in PERIM]
    for cx,cy in KC:
        for dx in (-5,5):
            for dy in (-5,5):taps.append((cx+dx,cy+dy,'KNOB_PCB'))
        for dx in (-12.2,12.2):
            for dy in (-12.2,12.2):taps.append((cx+dx,cy+dy,'KNOB_FRAME'))
    base=outer(0,P['base_thickness'])
    point_height=.8/math.tan(math.radians(P['base_drill_point_angle_deg']/2))
    cylinder_bottom=P['base_thickness']-P['base_pilot_cylinder_depth']
    point_bottom=cylinder_bottom-point_height
    assert point_bottom>0
    for x,y,_ in taps:
        base=hole(base,x,y,1.6,cylinder_bottom,P['base_thickness']+.01)
        base=base.cut(K.cone(0,1.6,point_bottom,point_height,x,y))
    # No FPC milling pockets in the laser-cut base plate.
    add('A_base','5mm平底板；M2×0.4盲攻丝', 'metal',base,(0,0,-24),ref='35 blind pilots; target full thread depth 3.2; cylinder depth 3.8 + 118deg point 0.4807; no milling pockets; base underside intact')
    wall=P['acrylic_wall'];ix0,iy0,ix1,iy1=OUT[0]+wall,OUT[1]+wall,OUT[2]-wall,OUT[3]-wall
    rim=outer(5,ZFOAM).cut(rounded(ix1-ix0,iy1-iy0,4.99,ZFOAM+.01,(ix0+ix1)/2,(iy0+iy1)/2,2))
    rim=rim.cut(rounded(ix1-ix0+4,iy1-iy0+4,5.5,11.5,(ix0+ix1)/2,(iy0+iy1)/2,4))
    for x,y in KC:rim=rim.cut(box(32,60,4.99,ZFOAM+.01,x,y+14))
    rim=rim.cut(box(16,16,8.5,ZFOAM+.01,RX,-7))
    for x,y in PERIM:rim=hole(rim,x,y)
    rims=sorted(rim.Solids(),key=lambda s:s.Volume(),reverse=True)
    assert len(rims)==2,len(rims)
    for i,s in enumerate(rims):add(f'A_acrylic_{i+1}',('乳白亚克力主框','乳白亚克力后中段')[i],'acrylic',s,(0,0,0),ref='10 radial ×10.1 high; inner 2×6 LED recess; CNC thickness')
    pcb=pcb_outline(ZPCB,ZPT)
    for x,y,d in D['mount_holes_mm']:pcb=hole(pcb,x,y,d)
    for x,y in D['switch_centres_mm']:pcb=hole(pcb,x,y,4)
    add('E_main_pcb','主PCB板框与机械孔参考','pcb',pcb,(0,0,16),ref='DXF exact polygon; copper, component assembly and electrical isolation not modelled')
    for i,(x,y,d) in enumerate(D['pcb_holes_mm']):
        add(f'B_pcb_col_{i+1}','M2×5+3铜柱；对边3','brass',hexcol(x,y,5,5,True),(0,0,7))
        add(f'S_pcb_{i+1}','PCB M2×4圆柱头螺丝','metal',screw(x,y,ZPT,4),(0,0,23))
    for i,(x,y,d) in enumerate(D['rail_holes_mm']):
        add(f'B_rail_lower_{i+1}','轨下层M2×5+3铜柱','brass',hexcol(x,y,5,5,True),(0,0,7))
        add(f'B_rail_upper_{i+1}','轨上层M2双通5mm铜柱','brass',hexcol(x,y,ZPT,P['rail_upper_standoff']),(0,0,22),ref='AF3, through tapped M2, height adjustable parameter')
        add(f'S_rail_stud_{i+1}','连接上下铜柱M2×6螺柱','metal',cyl(2,7.8,13.8,x,y),(0,0,21))
        add(f'S_rail_{i+1}','导轨M2×6圆柱头螺丝','metal',screw(x,y,RB+2.4,6),(0,0,44))
    foam=pcb_outline(ZPT,ZFOAM)
    for x,y in D['switch_centres_mm']:foam=foam.cut(box(16,16,ZPT-.01,ZFOAM+.01,x,y))
    foam=foam.cut(rounded(106,29.4,ZPT-.01,ZFOAM+.01,RX,RY,2))
    foam=foam.cut(box(42,54,ZPT-.01,ZFOAM+.01,RX,50))
    foam=foam.cut(box(16,18,ZPT-.01,ZFOAM+.01,RX,0))
    for x,y in KC:foam=foam.cut(box(31.2,31.2,ZPT-.01,ZFOAM+.01,x,y)).cut(box(10,6,ZPT-.01,ZFOAM+.01,x,96))
    for x,y,_ in D['pcb_holes_mm']:foam=hole(foam,x,y,4.6)
    add('A_foam','3.5mm吸音棉；芯片和插座开窗','foam',foam,(0,0,32),ref='Actual populated PCB heights require measurement before foam cutting')
    locator=panel_cutouts(outer(ZFOAM,ZLOC),ZFOAM,ZLOC,True).cut(box(16,22,ZFOAM-.01,ZLOC+.01,RX,-1))
    add('A_locator','2mm定位板；卡扣处1.5mm','petg',locator,(0,0,48),True)
    top=panel_cutouts(outer(ZLOC,ZTOP),ZLOC,ZTOP)
    for x,y in PERIM:top=cs(top,x,y,ZTOP)
    add('A_top','2mm铝合金装饰顶板','metal',top,(0,0,65))
    for i,(x,y) in enumerate(PERIM):add(f'S_rim_{i+1}','外框M2×16沉头螺丝','metal',screw(x,y,ZTOP,P['rim_screw_length'],'cs'),(0,0,84))
    rail=box(P['rail_length'],7,RB,RB+4.8,RX,RY)
    for dx in (-45,-30,-15,0,15,30,45):
        rail=hole(rail,RX+dx,RY,2.3,RB-.01,RB+4.81).cut(cyl(4.2,RB+2.4,RB+4.81,RX+dx,RY))
    add('M_rail','MGN7导轨100×7×4.8','metal',rail,(0,0,33),ref='HIWIN cross-section envelope; KHRB rail SKU requires measurement')
    for i,x in enumerate((RL+5,RR-5)):
        pad=box(5,21,RB-1.5,RB,x,RY)
        sx=RL+1.5 if i==0 else RR-1.5
        bar=box(3,21,RB,BT-.3,sx,RY).cut(box(3.02,7.4,RB-.01,RB+5,sx,RY))
        end=hole(pad.fuse(bar),x,RY)
        add(f'P_rail_stop_{i+1}','导轨一体垫脚与端部限位','petg',end,(0,0,28),True)
    mid=hole(box(5,3.4,RB-1.5,RB,RX,RY),RX,RY)
    add('P_rail_midpad','中间导轨1.5mm垫脚','petg',mid,(0,0,28),True)
    x=RX if slide is None else slide
    block=box(22.5,17,RB+1.5,BT,x,RY).cut(box(22.52,7.4,RB+1.49,RB+5,x,RY))
    for dx in (-4,4):
        for dy in (-6,6):block=hole(block,x+dx,RY+dy,2,BT-2.5,BT+.01)
    add('M_block','MGN7C滑块22.5×17×7外包络','metal',block,(0,0,50),group='slider',ref='Body7 + assumed under-gap1.5; assembled8.5 envelope, HIWIN standard H8; actual KHRB H unknown')
    cap=rounded(26,27,BT,BT+2.4,x,RY,2).fuse(rounded(20,8,BT+2.4,BT+10.4,x,RY,3.9))
    for dx in (-4,4):
        for dy in (-6,6):cap=cs(cap,x+dx,RY+dy,BT+2.4)
    hy=D['hall_centres_mm'][1][1];bottom=P['magnet_bottom'];floor=bottom-.6
    # Three sides, open toward rail columns: avoid closed tray/column interference.
    tray=box(12.6,6.4,floor,bottom,x,hy-.6)
    for dx in (-5.7,5.7):tray=tray.fuse(box(1.2,6.4,bottom,bottom+3,x+dx,hy-.6))
    tray=tray.fuse(box(12.6,1.2,bottom,bottom+3,x,hy-3.2))
    arm=box(12.6,2.4,floor,BT+.1,x,RY-12)
    bridge=box(12.6,(hy-3.8)-(RY-10.8),floor,floor+1.6,x,((hy-3.8)+(RY-10.8))/2)
    cap=cap.fuse(arm).fuse(bridge).fuse(tray)
    add('P_slider_cap_tray','滑块帽＋下伸磁铁托（三面开口）','petg',cap,(0,0,72),True,'slider')
    add('M_slide_magnet','滑轨磁铁10×5×3','magnet',box(10,5,bottom,bottom+3,x,hy),(0,0,61),group='slider',ref='Existing module documentation size; polarity, grade and adhesive require physical calibration')
    for i,(dx,dy) in enumerate(((-4,-6),(-4,6),(4,-6),(4,6))):add(f'S_block_{i+1}','滑块帽M2×4沉头螺丝','metal',screw(x+dx,RY+dy,BT+2.4,4,'cs'),(0,0,91),group='slider')
    for i,(hx,hy) in enumerate(D['hall_centres_mm']):add(f'E_hall_{i+1}','三霍尔SOT23占位包络','sensor',box(3.05,3.05,ZPT,ZPT+P['hall_height_envelope'],hx,hy),(0,0,18),ref='XY from DXF silk; 1.5mm assembled height allowance, not verified sensitive centre')
    add('E_usb','USB-C插座保守占位','sensor',box(9,8,ZPT,ZPT+3.4,RX,3),(0,0,18),ref='Actual connector footprint and cable overmold unverified; 16×8.6 side opening')
    for i,(sx,sy) in enumerate(D['switch_centres_mm']):
        lower=box(14,14,ZPT,16.6,sx,sy).fuse(box(15.6,15.6,16.6,17.6,sx,sy)).fuse(box(14.6,14.6,17.6,23.2,sx,sy)).fuse(cyl(3.8,8.3,ZPT,sx,sy))
        lower=lower.cut(cyl(6.2,19,27,sx,sy))
        add(f'M_MX_{i+1}','MX机械轴固定外壳参考','sensor',lower,(0,0,58),ref='CHERRY interface; pin/clip/detail not manufacturer solid model')
        stem=box(3.8,1.2,23.2-key_press,26.8-key_press,sx,sy).fuse(box(1.2,3.8,23.2-key_press,26.8-key_press,sx,sy))
        add(f'M_stem_{i+1}','MX十字轴动态占位','sensor',stem,(0,0,60),group='keys')
        key=cq.Workplane('XY',origin=(sx,sy,22.5-key_press)).rect(18,18).workplane(offset=9).rect(15,15).loft().val()
        key=key.cut(box(15,15,22.49-key_press,28.5-key_press,sx,sy))
        socket=cyl(5.8,23.2-key_press,28.6-key_press,sx,sy)
        cross=box(3.95,1.35,23.19-key_press,26.9-key_press,sx,sy).fuse(box(1.35,3.95,23.19-key_press,26.9-key_press,sx,sy))
        key=key.fuse(socket.cut(cross))
        add(f'M_keycap_{i+1}','1U键帽18mm参考包络','keycap',key,(0,0,81),group='keys',ref='Profile not selected; 4mm key travel clearance envelope only')
    parts.extend(knob_adapt(push,compression,angle))
    return parts,taps
def export():
    parts,taps=build()
    for folder in ('parts-step','print-stl','print-step','machining-step'):(ROOT/folder).mkdir(exist_ok=True)
    a=cq.Assembly(name='FluxRail_current_FluxDial');e=cq.Assembly(name='FluxRail_current_FluxDial_exploded');bom=[];meshes=[]
    for p in parts:
        assert p.shape.isValid(),p.id
        cq.exporters.export(p.shape,str(ROOT/'parts-step'/f'{p.id}.step'))
        a.add(p.shape,name=p.id,color=cq.Color(*COLORS[p.category]));e.add(p.shape.translate(p.ex),name=p.id,color=cq.Color(*COLORS[p.category]))
        b=p.shape.BoundingBox();bom.append(dict(id=p.id,name=p.name,material=p.category,printable=p.printable,group=p.group,reference=p.reference,bounds=[round(b.xlen,4),round(b.ylen,4),round(b.zlen,4)],solids=len(p.shape.Solids())))
        if p.printable:
            s=p.shape
            if 'carrier_left' in p.id:s=s.rotate((0,0,0),(0,1,0),90)
            if 'carrier_right' in p.id:s=s.rotate((0,0,0),(0,1,0),-90)
            if any(q in p.id for q in ('rotor_hub','lower_plate')):s=s.rotate((0,0,0),(1,0,0),180)
            sb=s.BoundingBox();s=s.translate((-sb.xmin,-sb.ymin,-sb.zmin))
            cq.exporters.export(s,str(ROOT/'print-stl'/f'{p.id}.stl'),tolerance=.03,angularTolerance=.12)
            cq.exporters.export(s,str(ROOT/'print-step'/f'{p.id}.step'))
        if p.id in ('A_base','A_top','A_acrylic_1','A_acrylic_2','A_locator','A_foam'):cq.exporters.export(p.shape,str(ROOT/'machining-step'/f'{p.id}.step'))
        v,f=p.shape.tessellate(.12,.28)
        meshes.append(dict(id=p.id,name=p.name,category=p.category,group=p.group,ex=p.ex,v=[[round(vv.x,4),round(vv.y,4),round(vv.z,4)] for vv in v],f=f))
    a.export(str(ROOT/'assembled.step'));e.export(str(ROOT/'exploded.step'))
    ka=cq.Assembly(name='FluxDial_knob');ke=cq.Assembly(name='FluxDial_knob_exploded');km=[]
    for p in parts:
        if not p.id.startswith('K1_'):continue
        shape=p.shape.translate((-KC[0][0],-KC[0][1],-P['base_thickness']))
        ka.add(shape,name=p.id,color=cq.Color(*COLORS[p.category]));ke.add(shape.translate(p.ex),name=p.id,color=cq.Color(*COLORS[p.category]))
        v,f=shape.tessellate(.12,.28)
        km.append(dict(id=p.id,name=p.name,category=p.category,group=p.group,ex=p.ex,v=[[round(vv.x,4),round(vv.y,4),round(vv.z,4)] for vv in v],f=f))
    ka.export(str(ROOT/'knob-assembled.step'));ke.export(str(ROOT/'knob-exploded.step'))
    (ROOT/'knob-meshes.json').write_text(json.dumps(km,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    (ROOT/'bom.json').write_text(json.dumps(bom,ensure_ascii=False,indent=2),encoding='utf-8')
    (ROOT/'meshes.json').write_text(json.dumps(meshes,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    with (ROOT/'base-hole-table.csv').open('w',encoding='utf-8-sig',newline='') as f:
        point=.8/math.tan(math.radians(P['base_drill_point_angle_deg']/2))
        writer=csv.writer(f);writer.writerow(['ID','X_mm','Y_mm','type','pilot_mm','thread','thickness_mm','total_blind_depth_mm','pilot_cylinder_depth_mm','drill_point_angle_deg','minimum_bottom_skin_mm','full_thread_depth_target_mm','operation'])
        for i,(x,y,t) in enumerate(taps):writer.writerow([i+1,f'{x:.6f}',f'{y:.6f}',t,1.6,'M2x0.4',5,round(P['base_pilot_cylinder_depth']+point,6),P['base_pilot_cylinder_depth'],P['base_drill_point_angle_deg'],round(5-P['base_pilot_cylinder_depth']-point,6),P['base_thread_depth_target'],'DRILL_FROM_TOP_BLIND_THEN_TAP_DO_NOT_LASER_CUT'])
    dims=dict(outer_mm=[OUT[2]-OUT[0],OUT[3]-OUT[1]],base_thickness=5,acrylic_height=ZFOAM-5,acrylic_wall=10,pcb_bottom=ZPCB,pcb_top=ZPT,locator_bottom=ZFOAM,locator_top=ZLOC,decorative_top=ZTOP,rail_bottom=RB,block_top=BT,slider_grip_top=BT+10.4,rail_travel_mm=XMAX-XMIN,slide_centres=[XMIN,RX,XMAX],knob_centres=KC,knob_body_top=31.2,knob_cap_top=58,base_taps=len(taps),components=len(parts),printed_instances=sum(p.printable for p in parts))
    (ROOT/'dimensions.json').write_text(json.dumps(dims,ensure_ascii=False,indent=2),encoding='utf-8')
    dims.update(revision=P['revision'],base_blind_depth_including_point=P['base_pilot_cylinder_depth']+point,base_minimum_skin=5-P['base_pilot_cylinder_depth']-point,base_thread_depth_target=P['base_thread_depth_target'],base_milled_pockets=0)
    (ROOT/'dimensions.json').write_text(json.dumps(dims,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(dims,ensure_ascii=False),flush=True)
if __name__=='__main__':export()
