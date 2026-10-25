"""Derive launcher-icon candidates from the approved existing UFO GLB.

Authoring-only. Uses the already bundled numpy/Pillow runtime and the existing
CPU triangle reviewer. No GLB, paint map, mobile project or runtime owner changes.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image, ImageDraw, PngImagePlugin
from normalize_glb import unpack, accessor, matrix
from render_prop_review import render

ROOT=Path(__file__).resolve().parents[1]
MASTER=ROOT/'assets/graphics/app-icon'
BUILD=ROOT/'build/app-icon'
SIZES={'mdpi':(108,48),'hdpi':(162,72),'xhdpi':(216,96),'xxhdpi':(324,144),'xxxhdpi':(432,192)}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(image,path):
    path.parent.mkdir(parents=True,exist_ok=True)
    info=PngImagePlugin.PngInfo();info.add(b'sRGB',b'\x00')
    image.save(path,pnginfo=info)


def model_parts(path):
    """Decode actual scene transforms and base-colour material factors."""
    doc,chunks=unpack(path);blob=next(b for t,b in chunks if t==0x004e4942)
    textures={};parts=[];names=[]
    def visit(index,parent):
        node=doc['nodes'][index];world=parent@matrix(node)
        if 'mesh' in node:
            for primitive in doc['meshes'][node['mesh']]['primitives']:
                pos=accessor(doc,blob,primitive['attributes']['POSITION'])
                uv=accessor(doc,blob,primitive['attributes']['TEXCOORD_0'])
                indices=accessor(doc,blob,primitive['indices']).reshape(-1,3)
                pos=(world@np.column_stack([pos,np.ones(len(pos))]).T).T[:,:3]
                mat=doc['materials'][primitive.get('material',0)]
                colour=mat.get('pbrMetallicRoughness',{})
                key=primitive.get('material',0)
                if key not in textures:
                    if 'baseColorTexture' in colour:
                        texture=doc['textures'][colour['baseColorTexture']['index']]
                        image=doc['images'][texture['source']];view=doc['bufferViews'][image['bufferView']]
                        start=view.get('byteOffset',0)
                        pixels=np.asarray(Image.open(io.BytesIO(blob[start:start+view['byteLength']])).convert('RGB')).astype(float)/255
                    else:pixels=np.ones((1,1,3))
                    # GLTF factors are linear. Retain actual paint and material
                    # factors; CPU review shading itself remains simple diffuse.
                    linear=np.where(pixels<=.04045,pixels/12.92,((pixels+.055)/1.055)**2.4)
                    linear*=np.array(colour.get('baseColorFactor',[1,1,1,1])[:3])
                    rgb=np.where(linear<=.0031308,linear*12.92,1.055*np.maximum(linear,0)**(1/2.4)-.055)
                    textures[key]=np.rint(np.clip(rgb*255,0,255)).astype(np.uint8)
                parts.append((pos,uv,indices,textures[key]));names.append(node.get('name',''))
        for child in node.get('children',[]):visit(child,world)
    for index in doc['scenes'][doc.get('scene',0)]['nodes']:visit(index,np.eye(4))
    if not parts:raise ValueError('Approved UFO scene contains no renderable primitives')
    return parts,names


def render_subject(recipe):
    model=ROOT/recipe['source_model']
    if digest(model)!=recipe['source_model_sha256']:
        raise ValueError('UFO source changed. Review it and explicitly update the icon recipe hash first.')
    parts,names=model_parts(model);direction=recipe['camera_direction'];size=recipe['render_size_px']
    colour=np.asarray(render(parts,direction,size))
    black=np.zeros((1,1,3),dtype=np.uint8);white=np.full((1,1,3),255,dtype=np.uint8)
    masks=[(p,u,i,black) for p,u,i,t in parts]
    alpha=np.any(np.asarray(render(masks,direction,size))!=[246,247,242],axis=2).astype(np.uint8)*255
    # Monochrome detail holes are the actual visible mint lenses, not newly
    # drawn symbols. Occlusion is retained through the same depth-buffer pass.
    idparts=[(p,u,i,white if name.startswith('Rim_Lens_') or name in ('Dome_Collar','Bearing') else black) for (p,u,i,t),name in zip(parts,names)]
    lens=np.asarray(render(idparts,direction,size)).mean(axis=2)>128
    lens&=alpha>0
    rgba=Image.fromarray(np.dstack([colour,alpha]),'RGBA')
    mono=Image.fromarray(np.dstack([np.full_like(colour,255),np.where(lens,0,alpha).astype(np.uint8)]),'RGBA')
    bbox=Image.fromarray(alpha).getbbox()
    return rgba.crop(bbox),mono.crop(bbox)


def placement(subject,size,extent=None,radius=None,layout=None):
    """Centre the visible mass; constrain either extent or all-pixel radius."""
    alpha=np.asarray((layout or subject).getchannel('A')).astype(float)
    yy,xx=np.nonzero(alpha);weights=alpha[yy,xx]
    centre=np.array([np.average(xx,weights=weights),np.average(yy,weights=weights)])
    if radius is not None:
        reach=np.sqrt((xx-centre[0])**2+(yy-centre[1])**2).max()
        scale=radius/reach
    else:scale=size*extent/max(subject.size)
    dims=np.maximum(1,np.rint(np.array(subject.size)*scale).astype(int))
    image=subject.resize(tuple(dims),Image.Resampling.LANCZOS)
    position=np.rint(np.array([size/2,size/2])-centre*scale).astype(int)
    canvas=Image.new('RGBA',(size,size));canvas.alpha_composite(image,tuple(position))
    return canvas


def flatten(foreground,background):
    canvas=Image.new('RGBA',foreground.size,background);canvas.alpha_composite(foreground)
    return canvas.convert('RGB')


def ios_catalogue(icon):
    catalogue=BUILD/'ios/Assets.xcassets';catalogue.mkdir(parents=True,exist_ok=True)
    (catalogue/'Contents.json').write_text(json.dumps({'info':{'author':'xcode','version':1}},indent=2)+'\n')
    app=catalogue/'AppIcon.appiconset';app.mkdir(exist_ok=True)
    save(icon,app/'AppIcon-1024.png')
    (app/'Contents.json').write_text(json.dumps({'images':[{'filename':'AppIcon-1024.png','idiom':'universal','platform':'ios','size':'1024x1024'}],
        'info':{'author':'xcode','version':1}},indent=2)+'\n')


def adaptive_resources(foreground,mono,icon,recipe):
    res=BUILD/'android/res'
    for density,(size,legacy) in SIZES.items():
        save(foreground.resize((size,size),Image.Resampling.LANCZOS),res/f'drawable-{density}/ic_launcher_foreground.png')
        save(mono.resize((size,size),Image.Resampling.LANCZOS),res/f'drawable-{density}/ic_launcher_monochrome.png')
        save(icon.resize((legacy,legacy),Image.Resampling.LANCZOS),res/f'mipmap-{density}/ic_launcher.png')
    path=res/'drawable/ic_launcher_background.xml';path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text('<?xml version="1.0" encoding="utf-8"?>\n<shape xmlns:android="http://schemas.android.com/apk/res/android" android:shape="rectangle"><solid android:color="'+recipe['background_srgb']+'" /></shape>\n')
    for version in (26,33):
        path=res/f'mipmap-anydpi-v{version}/ic_launcher.xml';path.parent.mkdir(parents=True,exist_ok=True)
        layers=['background','foreground']+(['monochrome'] if version==33 else [])
        children='\n'.join(f'  <{name} android:drawable="@drawable/ic_launcher_{name}" />' for name in layers)
        path.write_text('<?xml version="1.0" encoding="utf-8"?>\n<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'+children+'\n</adaptive-icon>\n')


def masked(image,shape):
    size=image.width;mask=Image.new('L',image.size);draw=ImageDraw.Draw(mask)
    if shape=='circle':draw.ellipse((0,0,size-1,size-1),fill=255)
    else:draw.rounded_rectangle((0,0,size-1,size-1),radius=size*(.22 if shape=='squircle' else .12),fill=255)
    result=image.convert('RGBA');result.putalpha(mask);return result


def contact_sheet(icon,adaptive,mono,background):
    sheet=Image.new('RGB',(1200,850),'#f3f0e8');draw=ImageDraw.Draw(sheet)
    draw.text((25,18),'HUMAN RESOURCES / EXISTING UFO LAUNCHER ICON CANDIDATE',fill='#203047')
    draw.text((25,40),'CPU-rendered approved model. Masks below are preview approximations; exported layers are unmasked.',fill='#536158')
    sample=masked(icon.resize((280,280),Image.Resampling.LANCZOS),'squircle');sheet.paste(sample,(25,85),sample)
    draw.text((350,85),'ACTUAL PIXEL SIZES / NO UPSCALING',fill='#203047')
    x=350
    for size in (20,29,40,60,80,120):
        image=masked(icon.resize((size,size),Image.Resampling.LANCZOS),'squircle');sheet.paste(image,(x,145),image)
        draw.text((x,280),str(size)+' px',fill='#536158');x+=size+28
    draw.text((25,400),'ANDROID ADAPTIVE MASK PREVIEWS / CENTRAL72DP VIEWPORT',fill='#203047')
    crop=(72,72,360,360)
    colour=flatten(adaptive,background).crop(crop)
    single=Image.new('RGBA',mono.size,'#d9ecdf');tinted=Image.new('RGBA',mono.size,'#244b46');tinted.putalpha(mono.getchannel('A'));single.alpha_composite(tinted);single=single.convert('RGB').crop(crop)
    for x,shape,label,source in [(25,'circle','Circle',colour),(315,'squircle','Squircle approximation',colour),(605,'roundrect','Rounded rectangle',colour),(895,'circle','Themed monochrome',single)]:
        image=masked(source.resize((250,250),Image.Resampling.LANCZOS),shape);sheet.paste(image,(x,445),image)
        draw.text((x,715),label,fill='#203047')
    x=25
    draw.text((25,760),'48PX MASK CHECKS',fill='#203047')
    for shape in ('circle','squircle','roundrect'):
        image=masked(colour.resize((48,48),Image.Resampling.LANCZOS),shape);sheet.paste(image,(x+210,755),image);x+=90
    path=MASTER/'small-size-review.png';save(sheet,path)


def validate(recipe):
    icon=Image.open(MASTER/'app-icon-1024.png')
    if icon.size!=(1024,1024) or icon.mode!='RGB':raise ValueError('iOS master must be opaque1024RGB')
    expected=tuple(int(recipe['background_srgb'][i:i+2],16) for i in (1,3,5))
    for xy in [(0,0),(0,1023),(1023,0),(1023,1023)]:
        if icon.getpixel(xy)!=expected:raise ValueError('iOS corners must be unmasked full-bleed background')
    if icon.info.get('srgb')!=0:raise ValueError('iOS PNG must declare sRGB')
    foreground=Image.open(MASTER/'adaptive-foreground-432.png')
    mono=Image.open(MASTER/'adaptive-monochrome-432.png')
    if foreground.mode!='RGBA' or mono.mode!='RGBA':raise ValueError('Adaptive layers require alpha')
    for image in (foreground,mono):
        alpha=np.asarray(image.getchannel('A'))
        if alpha.max()!=255 or alpha.min()!=0:raise ValueError('Adaptive alpha range must be0..255')
        if max(alpha[0].max(),alpha[-1].max(),alpha[:,0].max(),alpha[:,-1].max())!=0:raise ValueError('Adaptive subject touches layer edge')
    yy,xx=np.nonzero(np.asarray(foreground.getchannel('A')))
    radius_dp=float(np.sqrt((xx-216)**2+(yy-216)**2).max()/4)
    if radius_dp>33:raise ValueError(f'Android subject exceeds33dp safe radius: {radius_dp}')
    bbox=foreground.getchannel('A').getbbox();extent_dp=[(bbox[2]-bbox[0])/4,(bbox[3]-bbox[1])/4]
    if min(extent_dp)<48 or max(extent_dp)>66:raise ValueError(f'Android subject extent outside48..66dp: {extent_dp}')
    for density,(size,legacy) in SIZES.items():
        for name in ('foreground','monochrome'):
            path=BUILD/f'android/res/drawable-{density}/ic_launcher_{name}.png'
            image=Image.open(path)
            if image.size!=(size,size) or image.mode!='RGBA':raise ValueError(str(path)+' wrong size/mode')
        image=Image.open(BUILD/f'android/res/mipmap-{density}/ic_launcher.png')
        if image.size!=(legacy,legacy) or image.mode!='RGB':raise ValueError('Invalid legacy candidate')
    for path in (BUILD/'android/res').rglob('*.xml'):ET.parse(path)
    catalogue=json.loads((BUILD/'ios/Assets.xcassets/AppIcon.appiconset/Contents.json').read_text())
    for item in catalogue['images']:
        if not (BUILD/'ios/Assets.xcassets/AppIcon.appiconset'/item['filename']).is_file():raise ValueError('Catalogue image missing')
    files=sorted([MASTER/'recipe.json']+list(MASTER.glob('*.png'))+[p for directory in (BUILD/'ios/Assets.xcassets',BUILD/'android/res') for p in directory.rglob('*') if p.is_file()])
    report={'status':'passed_offline_checks','source_model':recipe['source_model'],'source_model_sha256':recipe['source_model_sha256'],
        'ios':{'size':[1024,1024],'mode':'RGB','alpha':False,'mask_baked':False,'catalogue':'single-size universal iOS; actool compilation separate'},
        'android':{'canvas_dp':108,'safe_radius_limit_dp':33,'actual_subject_radius_dp':radius_dp,'subject_extent_dp':extent_dp,'mask_baked':False,'colour_layers':'foreground/background','monochrome':'actual UFO silhouette with rim-lens and dome-bearing cutouts','resource_xml':'parsed; Android AAPT/package qualification separate'},
        'visual_review':{'status':'pending_root_review','contact_sheet':'assets/graphics/app-icon/small-size-review.png'},
        'files':[{'path':str(p.relative_to(ROOT)),'sha256':digest(p)} for p in files]}
    review=MASTER/'review.json'
    if review.exists():
        recorded=json.loads(review.read_text())
        if recorded.get('contact_sheet_sha256')==digest(MASTER/'small-size-review.png') and recorded.get('recipe_sha256')==digest(MASTER/'recipe.json'):
            report['visual_review']=recorded
    (BUILD/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--verify-only',action='store_true');args=parser.parse_args()
    recipe=json.loads((MASTER/'recipe.json').read_text());BUILD.mkdir(parents=True,exist_ok=True)
    if not args.verify_only:
        subject,mono_subject=render_subject(recipe)
        foreground=placement(subject,1024,extent=recipe['ios_subject_max_extent_fraction'])
        icon=flatten(foreground,recipe['background_srgb'])
        adaptive=placement(subject,432,radius=4*recipe['android_subject_safe_diameter_dp']/2)
        # Preserve the exact colour-layer placement despite monochrome holes.
        mono=placement(mono_subject,432,radius=4*recipe['android_subject_safe_diameter_dp']/2,layout=subject)
        save(foreground,MASTER/'ufo-foreground-1024.png');save(icon,MASTER/'app-icon-1024.png')
        save(Image.new('RGB',(1024,1024),recipe['background_srgb']),MASTER/'background-1024.png')
        save(adaptive,MASTER/'adaptive-foreground-432.png');save(mono,MASTER/'adaptive-monochrome-432.png')
        ios_catalogue(icon);adaptive_resources(adaptive,mono,icon,recipe);contact_sheet(icon,adaptive,mono,recipe['background_srgb'])
    report=validate(recipe);print(json.dumps({'status':report['status'],'files':len(report['files']),'android_safe_radius_dp':report['android']['actual_subject_radius_dp'],'review':report['visual_review']}))


if __name__=='__main__':main()
