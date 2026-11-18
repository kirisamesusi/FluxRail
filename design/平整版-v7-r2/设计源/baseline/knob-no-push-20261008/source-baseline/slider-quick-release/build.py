"""MIT. Shared quick-release base, INF saddle cap, compact ONGEKI-style grip.

Real CAD solids in mm. Supplier threads are nominal envelopes; no measured
SEGA handle CAD is available, so the tall grip is an original scaled concept.
"""
from pathlib import Path
from dataclasses import dataclass
import json,sys,importlib.util,math,struct
import cadquery as cq

ROOT=Path(__file__).resolve().parent
HOST=ROOT/'reference-enclosure' if (ROOT/'reference-enclosure').exists() else ROOT.parent/'enclosure'
spec=importlib.util.spec_from_file_location('host_enclosure',HOST/'build.py')
H=importlib.util.module_from_spec(spec);sys.modules[spec.name]=H;spec.loader.exec_module(H)
P=json.loads((ROOT/'parameters.json').read_text(encoding='utf-8'))
box,cyl,ring=H.box,H.cyl,H.ring
BT=H.BT;DECK=BT+P['deck_thickness'];LIFT=P['interface_lift'];INTERFACE=DECK+LIFT;ROOF=BT+P['adapter_top_above_block']+LIFT;AXIS=BT+5.9+LIFT
COLORS={**H.COLORS,'inf':(.58,.73,.78),'ongeki':(.27,.31,.36),'latch':(.83,.55,.17)}

@dataclass
class Part:
    id:str;name:str;category:str;shape:cq.Shape;ex:tuple=(0,0,0);printable:bool=False;group:str='slider';reference:str=''

def x_cyl(radius,length,x,y,z):
    return cq.Solid.makeCylinder(radius,length,cq.Vector(x,y,z),cq.Vector(1,0,0))

def x_box(length,w,h,x,y,z):return box(length,w,z-h/2,z+h/2,x+length/2,y)

def x_screw(d,length,seat,y,z):
    head_d,head_h,hex_af=(5.5,3,2.5) if d==3 else (3.8,2,1.5)
    shaft=x_cyl(d/2,length,seat,y,z)
    head=x_cyl(head_d/2,head_h,seat-head_h,y,z)
    socket=cq.Workplane('YZ',origin=(seat-head_h-.01,y,z)).polygon(6,hex_af/math.cos(math.pi/6)).extrude(head_h*.6).val()
    return shaft.fuse(head.cut(socket)).clean()

def x_nut(d,af,thickness,x,y,z):
    outer=cq.Workplane('YZ',origin=(x,y,z)).polygon(6,af/math.cos(math.pi/6)).extrude(thickness).val()
    return outer.cut(x_cyl(d/2,thickness+.02,x-.01,y,z)).clean()

def x_hex_bolt(seat,y,z):
    # DIN933 M3x8 maximum head envelope: AF5.5, k2.12 (Bollhoff data).
    head=cq.Workplane('YZ',origin=(seat-2.12,y,z)).polygon(6,5.5/math.cos(math.pi/6)).extrude(2.12).val()
    return head.fuse(x_cyl(1.5,8,seat,y,z)).clean()

def rail(x,clearance=0,roof=0,start=-8,end=8):
    z0=INTERFACE-.01 if clearance else INTERFACE
    z1=INTERFACE+P['dovetail_height']+roof
    lo=P['dovetail_root_width']/2+clearance;hi=P['dovetail_top_width']/2+clearance
    # XZ local plane extrudes toward -Y; hence origin at end.
    return cq.Workplane('XZ',origin=(x,end,0)).polyline([(-lo,z0),(lo,z0),(hi,z1),(-hi,z1)]).close().extrude(end-start).val()

def adapter():
    s=H.rounded(P['adapter_width'],P['adapter_length'],INTERFACE,ROOF,0,0,1.5)
    for x in P['rail_centres_x']:
        # Rear channel is open; remove the cap toward the keys (-Y).
        # The elevated interface clears the existing 31.5mm keycap envelope.
        s=s.cut(rail(x,P['fit_clearance_each_side'],P['fit_roof_clearance'],start=-8,end=15))
    # Rear lock recess, axis along -Y. Release moves the bolt toward +Y.
    s=s.cut(cq.Solid.makeCylinder(1.65,P['lock_dimple_depth']+.01,cq.Vector(0,13.01,AXIS),cq.Vector(0,-1,0)))
    return s.clean()

def rear(s):return s.rotate((0,0,0),(0,0,1),-90).translate((0,P['latch_rear_offset'],0))

def rounded_wire(w,l,z,r,cx=0,cy=0):
    solid=H.rounded(w,l,z,z+.05,cx,cy,r)
    return cq.Workplane(obj=solid).faces('<Z').wires().val()

def inf_cap():
    # Elongated rounded rectangle: narrow front/back sides and straight middle,
    # with a higher neck than revision1. This is not an elliptical disk.
    w=P['inf_width'];l=P['inf_length'];top=P['inf_top_global'];bottom=top-10
    profiles=[(bottom,w-5,l-4,5),(bottom+2,w,l,6),(top-2,w-.5,l-.5,6),(top,w-2,l-2,5.5)]
    head=cq.Solid.makeLoft([rounded_wire(a,b,z,r) for z,a,b,r in profiles],ruled=False)
    saddle=cq.Solid.makeCylinder(80,60,cq.Vector(0,-30,top-P['inf_saddle_depth']+80),cq.Vector(0,1,0))
    head=head.cut(saddle)
    waist=cq.Workplane('XY',origin=(0,0,ROOF-.1)).ellipse(11,8).workplane(offset=2.6).ellipse(9,6.5).workplane(offset=bottom+.1-(ROOF+2.5)).ellipse(13,8.5).loft().val()
    return adapter().fuse(waist).fuse(head).clean()

def ongeki_cap():
    # Two independently drawn orthographic silhouettes. XZ follows the narrow
    # front view without a ball; YZ follows the broad faceted side shell photo.
    # Intersecting their extrusions preserves BOTH outlines rather than making
    # another rounded box with the same outline on every side.
    top=P['ongeki_top_global'];sx=P['ongeki_width']/18;sy=P['ongeki_depth']/24
    def f(x,z):return (x*sx,top+z)
    def s(y,z):return (y*sy,top+z)
    front=(cq.Workplane('XZ',origin=(0,60,0)).moveTo(-5*sx,ROOF-.1)
           .lineTo(5*sx,ROOF-.1).lineTo(*f(5,-30)).lineTo(*f(9,-23))
           .lineTo(*f(9,-3)).lineTo(*f(6,0))
           .lineTo(*f(-6,0)).lineTo(*f(-9,-3))
           .lineTo(*f(-9,-23)).lineTo(*f(-5,-30)).close().extrude(120).val())
    # PLAYER is -Y (towards the keys); BACK is +Y. Correct the former reversal.
    # Side camera +X puts player -Y on LEFT. Use flat facets as requested;
    # successive slopes change gradually through the crown and lower taper.
    # The enlarged left crown and graded back replace abrupt isolated bends.
    side=(cq.Workplane('YZ',origin=(-9*sx,0,0)).moveTo(-10*sy,ROOF-.1)
          .lineTo(-2*sy,ROOF-.1).lineTo(*s(-2,-29))
          .lineTo(*s(-1.6,-26)).lineTo(*s(-.2,-23))
          .lineTo(*s(2.1,-20)).lineTo(*s(4.7,-17))
          .lineTo(*s(7.2,-14)).lineTo(*s(9.2,-11))
          .lineTo(*s(10.8,-8)).lineTo(*s(11.7,-6))
          .lineTo(*s(12,-4)).lineTo(*s(11.5,-2.5))
          .lineTo(*s(10.1,-1)).lineTo(*s(8,0))
          .lineTo(*s(-7.5,0)).lineTo(*s(-9.2,-.7))
          .lineTo(*s(-10.5,-2.1)).lineTo(*s(-11.4,-4.5))
          .lineTo(*s(-11.9,-8)).lineTo(*s(-12,-12)).lineTo(*s(-11.8,-16))
          .lineTo(*s(-11.2,-21)).lineTo(*s(-10.4,-26))
          .lineTo(*s(-10,-29))
          .close().extrude(18*sx).val())
    # Small planar bevels soften contact edges without replacing the faceted
    # outline by circular arcs. All surfaces above the adapter remain planes.
    side=cq.Workplane(obj=side).faces('|X').edges().chamfer(.8*min(sx,sy)).val()
    core=front.intersect(side).clean()
    return adapter().fuse(core).clean()

def base_parts():
    parts=[]
    def add(id,name,cat,s,ex=(0,0,0),printable=False,ref=''):
        parts.append(Part(id,name,cat,s.clean(),ex,printable,reference=ref))
    # Reuse the verified magnetic tray and the mounting deck, removing the old grip.
    old=cq.importers.importStep(str(HOST/'parts-step'/'P_slider_cap_tray.step')).val().translate((-H.RX,-H.RY,0))
    base=old.intersect(box(70,70,0,DECK,0,0))
    raised=H.rounded(26,27,DECK,INTERFACE,0,0,1.5)
    for dx,dy in ((-4,-6),(-4,6),(4,-6),(4,6)):
        raised=raised.cut(cyl(4.6,DECK-.01,INTERFACE+.01,dx,dy))
    base=base.fuse(raised)
    for x in P['rail_centres_x']:base=base.fuse(rail(x))
    extension=box(10.4,24,BT,INTERFACE,-18.1,0)
    lug=box(6.9,24,INTERFACE,BT+9.4+LIFT,-19.75,0)
    latch=extension.fuse(lug)
    # Top-loaded DIN934 M3 nut pocket, held down by the removable keeper bridge.
    latch=latch.cut(x_box(2.6,6.5,BT+10+LIFT-AXIS+3.25,-21.9,0,(BT+10+LIFT+AXIS-3.25)/2))
    latch=latch.cut(x_cyl(1.7,9,-24,0,AXIS))
    for y in (-8.3,8.3):
        post=x_box(4.4,4.4,6,-27.6,y,AXIS)
        latch=latch.fuse(post)
        latch=latch.cut(x_cyl(1.2,10,-29,y,AXIS))
        # Vertical loading pocket for standard M2 nuts.
        latch=latch.cut(x_box(1.8,4.9,BT+10+LIFT-AXIS+2.45,-22.7,y,(BT+10+LIFT+AXIS-2.45)/2))
    base=base.fuse(rear(latch))
    add('Q_base','共用快拆底座＋原位置磁铁托','petg',base,(0,0,-15),True,'26x27 mounting deck; 8x12 MGN7C holes; dual dovetail; magnet position unchanged')
    for i,(dx,dy) in enumerate(((-4,-6),(-4,6),(4,-6),(4,6))):
        add(f'Q_mount_{i+1}','MGN7C固定M2×4沉头螺丝','metal',H.screw(dx,dy,DECK,4,'cs'),(dx*2,dy*2,18))
    add('Q_magnet','滑轨磁铁10×5×3（位置保持）','magnet',box(10,5,H.P['magnet_bottom'],H.P['magnet_bottom']+3,0,H.D['hall_centres_mm'][1][1]-H.RY),(0,-15,-8))
    add('Q_nut_M3','DIN934 M3六角螺母','metal',rear(x_nut(3,5.5,2.4,-21.8,0,AXIS)),(0,8,14))
    add('Q_lock_screw','DIN933 M3×8后侧锁紧螺栓','metal',rear(x_hex_bolt(-23.6,0,AXIS)),(0,12,0))
    wheel=x_cyl(6,3.4,-26.8,0,AXIS).fuse(x_cyl(4,2.7,-29.5,0,AXIS))
    recess=cq.Workplane('YZ',origin=(-25.9,0,AXIS)).polygon(6,5.8/math.cos(math.pi/6)).extrude(2.6).val()
    wheel=wheel.cut(recess).cut(x_cyl(1.7,2.7,-25.91,0,AXIS))
    add('Q_thumbwheel','后侧手拧钮Ø12／外露Ø8凸台','latch',rear(wheel),(0,20,0),True,'AF5.8 female hex captures stock AF5.5 bolt head; rear +Y release')
    keeper=x_box(1.2,26,15,-28.8,0,AXIS).cut(x_cyl(4.3,1.4,-28.9,0,AXIS))
    for y in (-8.3,8.3):
        keeper=keeper.cut(x_cyl(1.2,1.4,-28.9,y,AXIS))
        keeper=keeper.fuse(box(9.7,3, BT+9.4+LIFT,BT+10.6+LIFT,-23.95,y))
    keeper=keeper.fuse(box(3.1,19.6,BT+9.4+LIFT,BT+10.6+LIFT,-20.65,0))
    add('Q_keeper','后侧限退框＋螺母防脱盖','petg',rear(keeper),(0,25,15),True,'Allows 0.8mm axial release / 1.6 turns M3; M2 keeper screws stay fitted')
    for i,y in enumerate((-8.3,8.3)):
        add(f'Q_keeper_screw_{i+1}','限退框DIN912 M2×8螺丝','metal',rear(x_screw(2,8,-28.8,y,AXIS)),(y,34,0))
        add(f'Q_keeper_nut_{i+1}','DIN934 M2六角螺母','metal',rear(x_nut(2,4,1.6,-22.6,y,AXIS)),(y,8,14))
    return parts

def build(mode='inf'):
    parts=base_parts()
    if mode!='base':
        s=inf_cap() if mode=='inf' else ongeki_cap()
        parts.append(Part('Q_cap_'+mode,'INF加高窄侧浅弧帽36×24' if mode=='inf' else '音击分面握把18×24／渐变斜面／周边倒角0.8',mode,s,(0,-25,38),True,reference='User front/side reference silhouettes; rear latch; forward removal; not measured OEM CAD'))
    return parts

def mesh(part):
    v,f=part.shape.tessellate(.08,.2)
    return dict(id=part.id,name=part.name,category=part.category,group=part.group,ex=part.ex,printable=part.printable,reference=part.reference,v=[[round(q.x,5),round(q.y,5),round(q.z,5)] for q in v],f=f)

def clean_stl(path):
    # OCCT can emit a zero-area triangle at a loft seam. Remove only triangles
    # whose float32 vertices coincide; do not simplify or change real surfaces.
    raw=path.read_bytes();n=struct.unpack_from('<I',raw,80)[0];triangles=[]
    for i in range(n):
        row=raw[84+50*i:134+50*i];p=struct.unpack_from('<12f',row)[3:]
        if len({tuple(p[j:j+3]) for j in (0,3,6)})==3:triangles.append(row)
    path.write_bytes(raw[:80]+struct.pack('<I',len(triangles))+b''.join(triangles))

def export():
    for name in ('parts-step','print-step','print-stl','assemblies'):(ROOT/name).mkdir(exist_ok=True)
    catalog={p.id:p for mode in ('base','inf','ongeki') for p in build(mode)}
    rows=[]
    for p in catalog.values():
        assert p.shape.isValid() and len(p.shape.Solids())==1,(p.id,'invalid or disconnected')
        cq.exporters.export(p.shape,str(ROOT/'parts-step'/f'{p.id}.step'))
        b=p.shape.BoundingBox();rows.append(dict(id=p.id,name=p.name,category=p.category,printable=p.printable,bounds=[b.xlen,b.ylen,b.zlen],reference=p.reference))
        if p.printable:
            s=p.shape
            if p.id in ('Q_thumbwheel','Q_keeper'):s=s.rotate((0,0,0),(1,0,0),-90)
            sb=s.BoundingBox();s=s.translate((-sb.xmin,-sb.ymin,-sb.zmin))
            cq.exporters.export(s,str(ROOT/'print-step'/f'{p.id}.step'))
            cq.exporters.export(s,str(ROOT/'print-stl'/f'{p.id}.stl'),tolerance=.025,angularTolerance=.1)
    for mode,title in [('base','01-共用底座含磁铁螺丝'),('inf','02-INF短帽装配'),('ongeki','03-音击握把装配')]:
        a=cq.Assembly(name=mode);e=cq.Assembly(name=mode+'_exploded');parts=build(mode)
        for p in parts:
            color=cq.Color(*COLORS[p.category]);a.add(p.shape,name=p.id,color=color);e.add(p.shape.translate(p.ex),name=p.id,color=color)
        a.export(str(ROOT/'assemblies'/f'{title}.step'));e.export(str(ROOT/'assemblies'/f'{title}-爆炸.step'))
        (ROOT/f'meshes-{mode}.json').write_text(json.dumps([mesh(p) for p in parts],ensure_ascii=False,separators=(',',':')),encoding='utf-8')
        print(mode,len(parts),'parts',flush=True)
    (ROOT/'parts.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
    dimensions={}
    for mode in ('base','inf','ongeki'):
        b=cq.Compound.makeCompound([p.shape for p in build(mode)]).BoundingBox()
        dimensions[mode]=dict(assembly_xyz=[b.xlen,b.ylen,b.zlen],top_z=b.zmax,top_above_panel=b.zmax-H.ZTOP)
    (ROOT/'dimensions.json').write_text(json.dumps(dimensions,indent=2),encoding='utf-8')
    # One small, separately labelled fit coupon; NOT part of the real assembly.
    sample=adapter().intersect(box(8,18,INTERFACE,ROOF,-9,0))
    b=sample.BoundingBox();sample=sample.translate((-b.xmin,-b.ymin,-b.zmin))
    cq.exporters.export(sample,str(ROOT/'print-stl'/'FIT-仅测试母槽.stl'),tolerance=.025,angularTolerance=.1)
    male=rail(-9).fuse(box(8,16,INTERFACE-2.4,INTERFACE,-9,0));b=male.BoundingBox();male=male.translate((-b.xmin,-b.ymin,-b.zmin))
    cq.exporters.export(male,str(ROOT/'print-stl'/'FIT-仅测试公轨.stl'),tolerance=.025,angularTolerance=.1)
    for path in (ROOT/'print-stl').glob('*.stl'):clean_stl(path)

if __name__=='__main__':export()
