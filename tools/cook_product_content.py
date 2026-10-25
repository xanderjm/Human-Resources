"""Offline, explicit product asset selection and approved font atlas cooking.
Uses the existing art toolchain (NumPy/Pillow); none is linked in the game.
Generated files belong under ignored build directories, never assets/runtime.
"""
from pathlib import Path
import argparse, json, hashlib, shutil, subprocess, sys, struct
from PIL import Image, ImageDraw, ImageFont
from cook_style_preview import cook_glb, primitives, write_mesh
from cook_vehicle_content import split_mount, write_vehicle_header
ROOT=Path(__file__).resolve().parents[1]
MODELS=['ufo-modular', 'ufo-antenna', 'ufo-beam', 'ufo-laser', 'ufo-shield', 'ufo-rim-lens', 'shield-shell', 'cow', 'sheep', 'pig', 'farmer-body', 'straw-hat', 'infantry-rifleman-body', 'infantry-shotgunner-body', 'infantry-rocketeer-body', 'weapon-carbine', 'weapon-shotgun', 'weapon-rocket-launcher', 'jeep-chassis', 'jeep-wheel', 'jeep-minigun-head', 'jeep-minigun-fixed-post', 'jeep-minigun-yaw-cap', 'tank-hull', 'tank-turret', 'missile', 'tree-round-large', 'tree-round-small', 'bush-low', 'field-rock-a', 'field-rock-b', 'grass-tuft', 'crop-seedling', 'barn', 'silo', 'hay-bale', 'windmill-mast', 'windmill-rotor', 'resident-body', 'nightcap', 'house-coral', 'house-teal', 'streetlamp', 'mailbox-red', 'mailbox-teal', 'wheelie-bin', 'hedge-module', 'cactus-tall', 'cactus-short', 'cliff-module-a', 'cliff-module-b', 'desert-boulder-a', 'desert-boulder-b', 'desert-scrub', 'service-canopy', 'fuel-pump-red', 'fuel-pump-blue', 'road-bollard']
MATERIALS=['grass', 'dirt', 'soil', 'asphalt', 'pavement', 'sand']
ANIMATED_MODELS=['cow', 'pig', 'sheep', 'farmer-body', 'resident-body', 'infantry-rifleman-body', 'infantry-shotgunner-body', 'infantry-rocketeer-body']
LOCATIONS=['farmland', 'suburbia', 'desert']
FONTS=['HR-Beam-UI-SemiBold.ttf','HR-Abduction-Compact-Black-Italic.ttf']
# Graphic regions must remain disjoint, including their transparent margins:
# alpha compositing cannot erase a previously drawn neighbour through transparency.
ATLAS_GRAPHICS=[('ui-hud-panel',(0,1152),(768,192)),
                ('ui-xp-track',(0,1344),(512,128)),
                ('ui-upgrade-card',(512,1408),(512,640))]
def validate_graphic_regions(regions):
    for i, (_, (x,y), (width,height)) in enumerate(regions):
        if min(x,y) < 0 or min(width,height) <= 0 or x+width > 1024 or y+height > 2048:
            raise ValueError('Graphic exceeds the product atlas')
        for _, (other_x,other_y), (other_width,other_height) in regions[i+1:]:
            if x < other_x+other_width and other_x < x+width and y < other_y+other_height and other_y < y+height:
                raise ValueError('Graphic atlas regions overlap')

def validate_recipe(recipe):
    expected={'schema':1,'kind':'human-resources-locations','models':MODELS,
              'materials':MATERIALS,'fonts':FONTS,'locations':LOCATIONS,
              'animated_models':ANIMATED_MODELS}
    if recipe != expected:
        raise ValueError("Product recipe must explicitly match the reviewed consumed three-location content set")

def model_options(asset):
    if asset == 'ufo-modular':
        return {'source_path':'assets/models/ufo-modular/ufo-modular.glb',
                'exclude_nodes':('Antenna_Module',)}
    if asset in ('ufo-antenna','ufo-beam','ufo-laser','ufo-shield','ufo-rim-lens'):
        return {'source_path':'assets/models/ufo-modular/part-'+asset[4:]+'.glb'}
    if asset == 'shield-shell':
        # GLB transparency is a viewer fallback; production binds the supplied
        # camera-dependent rim contract with explicit linear alpha coverage.
        return {'source_path':'assets/graphics/fx-shield-shell/unit-sphere.glb',
                'opaque_material':True}
    return {}

def write_visibility_bounds(output, header):
    rows=[]
    for asset in sorted(MODELS+['cube']):
        data=(output/(asset+'.hrm')).read_bytes()
        magic,count,indices,width,height=struct.unpack_from('<4sIIII',data)
        if magic!=b'HRM1' or not 0<count<=32768:
            raise ValueError('Visibility bounds require bounded cooked HRM1: '+asset)
        vertices=list(struct.iter_unpack('<14f',data[20:20+count*56]))
        low=[min(v[axis] for v in vertices) for axis in range(3)]
        high=[max(v[axis] for v in vertices) for axis in range(3)]
        values=lambda point: '{'+','.join(f'{v:.9f}F' for v in point)+'}'
        rows.append('{'+json.dumps(asset)+','+values(low)+','+values(high)+'}')
    header.write_text('#pragma once\n#include <array>\n#include <string_view>\nnamespace human_resources::product::detail {\nstruct VisibilityMeshBounds { std::string_view id; std::array<float,3> minimum,maximum; };\ninline constexpr std::array<VisibilityMeshBounds,'+str(len(rows))+'> visibility_mesh_bounds{{'+','.join(rows)+'}};\n}\n')

def cook_animation_content(output, header):
    subprocess.run([sys.executable,str(ROOT/'tools/cook_character_animation.py'),'--output',str(output)],check=True)
    subprocess.run([sys.executable,str(ROOT/'tools/cook_infantry_animation.py'),'--output',str(output)],check=True)
    legacy=json.loads((output/'animation-provenance.json').read_text())
    infantry=json.loads((output/'infantry-animation-provenance.json').read_text())
    legacy['models']=[r for r in legacy['models'] if r['id'] in ANIMATED_MODELS]+infantry['models']
    legacy['tool_sha256'].update(infantry['tool_sha256'])
    legacy['infantry_source_sha256']=infantry['source_sha256']
    legacy['infantry_sockets']=infantry['sockets']
    (output/'animation-provenance.json').write_text(json.dumps(legacy,indent=2)+'\n')
    (output/'infantry-animation-provenance.json').unlink()
    for suffix in ('.hra','.hrm'):
        (output/('soldier-body'+suffix)).unlink(missing_ok=True)
    # Raw weapon-local muzzle points follow the same source socket delta as the
    # gun. They are generated, never repeated guessed hand/flash coordinates.
    equipment=[]
    for row in infantry['sockets']:
        points=[v['raw_local_m'] for k,v in row['markers'].items() if 'muzzle' in k or k=='socket_nose']
        if row['role']=='rocketeer': points=[[p[0],p[1],p[2]+.01] for p in points]
        if len(points)==1: points.append(points[0])
        vector=lambda p:'{'+','.join(f'{float(v):.9g}F' if '.' in f'{float(v):.9g}' or 'e' in f'{float(v):.9g}' else f'{float(v):.9g}.0F' for v in p)+'}'
        equipment.append('{'+json.dumps(row['id'])+','+json.dumps(row['weapon_id'])+','+vector(points[0])+','+vector(points[1])+','+('true' if row['role']=='shotgunner' else 'false')+'}')
    (header.parent/'infantry_equipment.generated.h').write_text('#pragma once\n#include <array>\n#include <string_view>\nnamespace human_resources::product {\nstruct InfantryEquipment { std::string_view body,weapon; std::array<float,3> muzzle,secondary_muzzle; bool double_muzzle; };\ninline constexpr std::array<InfantryEquipment,3> infantry_equipment{{'+','.join(equipment)+'}};\n}\n')

def cook(output, header):
    recipe=json.loads((ROOT/"assets/runtime/game-content.json").read_text())
    validate_recipe(recipe)
    output.mkdir(parents=True,exist_ok=True);header.parent.mkdir(parents=True,exist_ok=True)
    inventory={a['id']:a for a in json.loads((ROOT/'assets/production/inventory.json').read_text())['assets']}
    vehicle_sources=split_mount(header.parent/'vehicle-source')
    write_vehicle_header(header.parent/'vehicle_assembly.generated.h')
    records=[]
    for asset in MODELS:
        options=model_options(asset)
        if asset in vehicle_sources: options['source_path']=str(vehicle_sources[asset])
        record=cook_glb(asset,output,inventory,**options)
        if asset in vehicle_sources:
            # Provenance names the durable original and reproducible subset;
            # a checkout-specific generated path is never an art authority.
            record['derived_glb_sha256']=record['source_sha256']
            record['source']='assets/models/jeep-minigun-mount/jeep-minigun-mount.glb'
            record['source_sha256']=hashlib.sha256((ROOT/record['source']).read_bytes()).hexdigest()
            record['source_subset']=asset
        records.append(record)
    cook_animation_content(output,header)
    primitives(output)
    # Wood is not used by any of the three consumed scenery kits.
    (output/'ground-wood.hrm').unlink(missing_ok=True)
    fonts=ROOT/'assets/fonts/human-resources-v3/ttf'
    image=Image.new('RGBA',(1024,2048));draw=ImageDraw.Draw(image)
    metrics=[]
    for bank,filename in enumerate(['HR-Beam-UI-SemiBold.ttf','HR-Abduction-Compact-Black-Italic.ttf']):
        font=ImageFont.truetype(str(fonts/filename),48)
        for code in range(32,127):
            index=code-32;x=(index%16)*64;y=bank*512+(index//16)*80
            draw.text((x+4,y+8),chr(code),font=font,fill='white',stroke_width=0)
            metrics.append(float(font.getlength(chr(code))))
    # Owner-approved graphic assets retain their shapes and icon silhouettes.
    for i,name in enumerate(['ui-coin','ui-shield','ui-beam','ui-laser']):
        graphic=Image.open(ROOT/'assets/graphics'/name/(name+'.png')).convert('RGBA')
        image.alpha_composite(graphic.resize((128,128),Image.Resampling.LANCZOS),(i*128,1024))
    validate_graphic_regions(ATLAS_GRAPHICS)
    for name,position,size in ATLAS_GRAPHICS:
        graphic=Image.open(ROOT/'assets/graphics'/name/(name+'.png')).convert('RGBA')
        image.alpha_composite(graphic.resize(size,Image.Resampling.LANCZOS),position)
    # One white texel is the solid rectangle source, isolated from glyph filtering.
    draw.rectangle((1000,1000,1023,1023),fill='white')
    write_mesh(output/'ui-atlas.hrm',[[0,0,0,0,1,0,0,0,1,1,1,1,0,0]],[0,0,0],image)
    write_visibility_bounds(output,header.with_name('product_mesh_bounds.generated.h'))
    recipes=json.loads((ROOT/'design/balance/endless-upgrade-system.json').read_text())['evolutions']
    header.write_text('#pragma once\n#include <array>\n#include <string_view>\nnamespace human_resources::product {\ninline constexpr std::array<float,190> glyph_advances{'+','.join(f'{n:.5f}F' for n in metrics)+'};\ninline constexpr std::array<std::string_view,6> recipe_effects{'+','.join(json.dumps(row['effect']) for row in recipes)+'};\n}\n')
    shutil.copyfile(ROOT/'assets/fonts/human-resources-v3/LICENSE.txt',output/'FONT-LICENSE.txt')
    (output/'content-provenance.json').write_text(json.dumps({'schema':1,'models':records,'vehicle_contract_sha256':hashlib.sha256((ROOT/'assets/production/vehicle-weapons.json').read_bytes()).hexdigest(),'vehicle_split_tool_sha256':hashlib.sha256((ROOT/'tools/cook_vehicle_content.py').read_bytes()).hexdigest(),'font_source':'assets/fonts/human-resources-v3/ttf','locations':LOCATIONS,'materials':MATERIALS,'purpose':'Human Resources consumed three-location playable content; source art retained; device review pending'},indent=2)+'\n')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--header',type=Path,required=True);a=p.parse_args();cook(a.output,a.header)
