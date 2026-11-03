"""Create a derived presentation mesh; retain the source PCB geometry and placement."""
import bpy,bmesh,math,sys,json
from pathlib import Path
args=sys.argv[sys.argv.index('--')+1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(Path(args[0]).resolve()))
report=[]
for obj in list(bpy.context.scene.objects):
 if obj.type!='MESH':continue
 bpy.context.view_layer.objects.active=obj;obj.select_set(True)
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 # Work in source meters; 0.18 mm edge treatment is below connector pitch.
 bm=bmesh.new();bm.from_mesh(obj.data)
 bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0000003)
 bmesh.ops.dissolve_limit(bm,angle_limit=.008,verts=list(bm.verts),edges=list(bm.edges),use_dissolve_boundaries=False,delimit={'MATERIAL'})
 bm.to_mesh(obj.data);bm.free();obj.data.update()
 bevel=obj.modifiers.new('Presentation micro bevel','BEVEL');bevel.width=.00018;bevel.segments=4;bevel.limit_method='ANGLE';bevel.angle_limit=.5;bevel.use_clamp_overlap=True
 try:bpy.ops.object.modifier_apply(modifier=bevel.name)
 except Exception as e:print('bevel skipped',obj.name,str(e),flush=True)
 for p in obj.data.polygons:p.use_smooth=True
 normal=obj.modifiers.new('Weighted surface normals','WEIGHTED_NORMAL');normal.keep_sharp=True;normal.weight=50
 try:bpy.ops.object.modifier_apply(modifier=normal.name)
 except Exception:pass
 report.append({'object':obj.name,'faces':len(obj.data.polygons)})
 obj.select_set(False)
 print(obj.name,len(obj.data.polygons),flush=True)
bpy.ops.export_scene.gltf(filepath=str(Path(args[1]).resolve()),export_format='GLB',export_apply=True,export_yup=True)
Path(args[1]+'.report.json').write_text(json.dumps(report,indent=2))
print('Styled mesh exported',flush=True)
