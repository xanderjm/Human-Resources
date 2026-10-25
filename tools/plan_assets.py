"""Create the reviewed production inventory without altering generated assets."""
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets' / 'production'

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    assets = []
    def add(asset_id, title, category, kind, biomes, dimensions, faces, description, construction, dependencies=()):
        assets.append(dict(id=asset_id, title=title, category=category, kind=kind,
            biomes=biomes.split(','), dimensions_m=dimensions, face_limit=faces,
            description=description, construction=construction, dependencies=list(dependencies),
            reference={'status':'planned'}, production={'status':'planned'}, qa={'status':'pending'}))
    def model(i,t,c,b,d,f,desc,use,deps=()):
        add(i,t,c,'tripo_model',b,d,f,desc,use,deps)
    # Moving objects are separated wherever transforms or gameplay require them.
    model('ufo-body','UFO body','shared','all',[2.2,.7,2.2],3000,'Cobalt saucer, blue hemispherical dome, three mint painted lamps, crooked antenna. No beam, laser, pilot or ground.','Base ship with attachment sockets; fixed opaque painted dome.')
    model('ufo-laser-mount','UFO laser attachment','shared','all',[.3,.3,.65],1200,'Small cobalt bolt-on single barrel with mint inset muzzle and painted blue highlights.','Separate upgrade mesh attached to a saucer socket.')
    model('ufo-beam-emitter','UFO beam emitter attachment','shared','all',[.65,.16,.65],1200,'Small cobalt annular emitter with three mint insets; no drawn beam or glow cloud.','Separate upgrade under the saucer; capture ring remains an effect.')
    model('ufo-shield-emitter','UFO shield emitter attachment','shared','all',[.3,.4,.3],1000,'Compact cobalt mounting foot and mint lens on a stub stalk. No energy shield shell.','Repeatable upgrade socket prop; shield surface is an effect.')
    model('farmer-body','Farmer without hat','characters','all',[.6,1.6,.4],6000,'Warm peach comic farmer, navy overalls, cream shirt, brown bean boots. Bare head, empty hands, neutral relaxed A stance; no hat or tools.','Hat is separate for abduction; no rigging/animation in initial generation.',('straw-hat',))
    model('resident-body','Sleepy resident without cap','characters','suburb',[.65,1.6,.45],6000,'Warm peach resident, navy bathrobe, cream belt/trim, large slippers. Bare head, empty hands and relaxed neutral A stance.','Nightcap separate; shared human scale.',('nightcap',))
    model('soldier-body','Helmeted soldier','characters','all',[.6,1.6,.45],6000,'Comic warm peach soldier, navy uniform and helmet, large boots, empty hands, neutral A stance. No gun or vehicle.','Shared crew actor, never fused into jeep geometry.')
    model('straw-hat','Amber straw hat','characters','all',[.65,.25,.65],1000,'One amber broad brim straw hat, black/navy band, clearly open underside, no head or person.','Detachable shared farmer headwear and loose-hat effect prop.')
    model('nightcap','Mustard nightcap','characters','suburb',[.45,.45,.4],1000,'One mustard soft drooping nightcap with cream pompom, hollow head opening; no head.','Detachable resident cap.')
    model('cow','Pear-shaped cow','animals','all',[1,1.15,1.8],6000,'One standing pear-shaped ivory cow, large painterly indigo-black patches, coral nose, broad ears, four short legs, tiny horns and tail.','One intact neutral quadruped; no grass, collar, platforms or effects.')
    model('sheep','Cream sheep','animals','all',[.75,.8,1.1],4500,'One standing cream/gold scalloped-wool sheep, dark navy face and four short legs; broad painted wool masses.','One intact quadruped; wool is geometric masses with painterly texture, not hair.')
    model('pig','Salmon pig','animals','all',[.65,.65,1.05],4500,'One standing salmon-pink bean pig, four stub feet, little ears, broad rosy planes and tiny curly tail.','One intact quadruped; no ground or accessories.')
    model('jeep-chassis','Jeep chassis without wheels or crew','military','all',[1.65,1.25,3.1],2800,'Navy open-top jeep body with empty seats, open wheel wells, coral chevrons and windshield frame. NO wheels, tires, driver or gunner.','Four wheel instances and separate crew actors assembled later.',('jeep-wheel','soldier-body'))
    model('jeep-wheel','Reusable jeep wheel','military','all',[.22,.6,.6],900,'One chunky dark rubber wheel with navy hub, clearly defined axle centre. No axle or vehicle, only one wheel.','Four copies around X axle; spare copy optional.')
    model('tank-hull','Tracked tank hull without turret','military','all',[2.2,.95,3.2],2800,'Squat charcoal/navy tank hull with two continuous caterpillar track assemblies; flat central turret mounting platform. No turret, barrel or crew.','Turret mounts on top and rotates independently.',('tank-turret',))
    model('tank-turret','Tank turret and cannon','military','all',[1.3,.7,2.1],2400,'One low angular navy turret with coral double chevron, short thick cannon and slim antenna. No hull or tracks.','Single articulated turret assembly with cannon included, central yaw pivot.')
    model('missile','Military missile','military','all',[.22,.22,.85],1000,'One small navy missile with coral/red nose, four simple tail fins, dark exhaust opening. No flame, smoke, platform or trail.','Shared projectile; exhaust is a separate effect.')
    # Organic silhouettes and modular vegetation; identical instances fill larger groups.
    for i,t,d,desc in [
        ('tree-round-large','Large rounded tree',[2.6,3.2,2.6],'One olive/sage faceted rounded crown and short warm brown trunk; broad painterly leaf patches. No plants or ground at base.'),
        ('tree-round-small','Small rounded tree',[1.6,2.0,1.6],'One compact irregular olive/sage rounded crown on short warm wood trunk; visibly different crown silhouette from large tree. No ground.'),
        ('bush-round','Rounded bush',[.85,.7,.85],'One squat painterly sage/olive bush with a few broad foliage lobes, no pot or ground.'),
        ('bush-low','Low spreading bush',[1.25,.5,.75],'One low irregular spreading sage bush, clear small-game silhouette, no rocks or ground.'),
        ('hedge-module','Straight hedge module',[2,.85,.7],'One neatly trimmed long sage hedge block with slightly rounded irregular top; no soil bed or pavement.'),
        ('field-rock-a','Field rock A',[.65,.45,.55],'One low angular blue-grey field stone with broad painted slate planes, no vegetation.'),
        ('field-rock-b','Field rock B',[.45,.6,.4],'One taller asymmetric blue-grey field stone, silhouette different from rock A, no ground.'),
        ('cactus-tall','Tall branching cactus',[1.1,2.5,.7],'One tall sage-green faceted cactus with two asymmetric upward arms and broad painted rib highlights. No ground, pot or stones.'),
        ('cactus-short','Short cactus',[.75,1.25,.65],'One short squat faceted sage-green cactus with one small side arm; no ground, pot or rocks.'),
        ('cliff-module-a','Tall desert outcrop',[3.5,4,2.5],'One connected chunky blush/sandstone cliff outcrop with mauve shadow planes, broad low-poly forms, no scattered stones or floor slab.'),
        ('cliff-module-b','Low desert outcrop',[3,2,2],'One connected broad low stepped sandstone/mauve outcrop, visibly different from tall cliff, no separate debris or ground.'),
        ('desert-boulder-a','Sandstone boulder A',[1.25,.9,1.0],'One broad rounded-faceted sandstone boulder with mauve painterly shadows, no plants or stones attached.'),
        ('desert-boulder-b','Sandstone boulder B',[.65,.65,.7],'One smaller angular asymmetric sandstone boulder with mauve painted faces, no ground.'),
        ('grass-tuft','Sage grass tuft',[.35,.3,.35],'One connected small stylised sage grass tuft of broad faceted blades, no soil base or other objects.'),
        ('desert-scrub','Dry desert scrub',[.65,.45,.65],'One connected dry straw/olive scrub tuft of broad stylised pointed leaves, no soil, cactus or rock.'),
        ('crop-seedling','Crop seedling',[.3,.3,.3],'One small green crop seedling with broad oval leaves and short central stem, no dirt bed, pot, other plants or ground.')]:
        b='desert' if i.startswith(('cactus','cliff','desert')) else ('farm' if i=='crop-seedling' else 'farm,suburb')
        model(i,t,'vegetation-rocks',b,d,1800 if 'cliff' not in i else 2800,desc,'Repeat/rotate/scale this individual asset; rows and clusters are assembled, not fused into the model.')
    for i,t,b,d,f,desc,use in [
        ('barn','Farm barn','farm',[5,3.8,5],2800,'Cream timber barn with muted terracotta roof, open dark doorway, painted planks and tile strokes. No grass, bushes, fence, hay or terrain.','Static building, separate surrounding props.'),
        ('silo','Farm silo','farm',[2,4,2],2200,'One round blue-grey metal farm silo, conical cap, broad brushed painted panels, flat bottom. No ground or props.','Standalone farm landmark.'),
        ('windmill-mast','Windmill mast without rotor','farm',[1,3.5,1],2200,'One simple dark painted metal tapered lattice windmill tower with top rotor mounting spindle. No rotor blades, ground or plants.','Separate rotor mounted on spindle.'),
        ('windmill-rotor','Windmill rotor','farm',[1.8,1.8,.15],1400,'One radial farm windmill rotor of broad slate-grey vanes and central hub, all joined; no tower, ground or scenery.','Separate rotating element; no animation generation.'),
        ('hay-bale','Round hay bale','farm',[1,1,1.1],1600,'One wheat-gold cylindrical hay bale, hand-painted circular straw rings on ends and broad curved straw strokes, no ground.','Reusable standalone bale.'),
        ('house-coral','Coral-roof house','suburb',[5,3.5,4],2800,'One small cream timber bungalow with coral pitched roof, simple warm painted windows, door and short step. No lamp, hedge, flowers, lawn, mailbox, fence or pavement.','Place alongside modular pavement; window glow is optional material metadata.'),
        ('house-teal','Teal-roof house','suburb',[5,3.5,4],2800,'One small cream timber bungalow in same family with petrol-teal pitched roof, warm painted windows, door and short step. No surrounding props, shrubbery, grass or ground.','Second building variation; common scale/paint treatment.'),
        ('streetlamp','Streetlamp','suburb',[.45,2.8,.45],1600,'One slender charcoal streetlamp with simple warm amber lantern, square foot only. No pavement slab, people or ground.','Repeat at pavement edge; no light object baked into model.'),
        ('mailbox-red','Red mailbox','suburb',[.4,1.1,.35],1200,'One small muted terracotta-red rural mailbox on a dark narrow post, closed box, painted mail slot. No ground, shrubs or house.','Standalone fixed mailbox.'),
        ('mailbox-teal','Teal mailbox','suburb',[.4,1.1,.35],1200,'One small muted petrol-teal mailbox on dark narrow post, same scale as red mailbox, no surrounding props or ground.','Colour/silhouette variation.'),
        ('wheelie-bin','Wheelie bin','suburb',[.6,1,.6],1600,'One charcoal-blue wheelie rubbish bin with closed lid and two small wheels, subtle painted wear. No litter, ground or other bins.','Single static prop; wheels and lid fixed.'),
        ('service-canopy','Roadside service canopy','desert',[6,3.5,4],2600,'One simple flat muted-rust roof slab with cream fascia and four cream posts. Open below. No fuel pumps, ground platform, road, plants, rocks or cars.','Place pump instances under it independently.'),
        ('fuel-pump-red','Red fuel pump','desert',[.65,1.6,.45],1800,'One small red vintage rectangular fuel pump, dark simple display, thick black hose fixed close to side and nozzle. No labels, ground, canopy or other pumps.','Standalone pump module.'),
        ('fuel-pump-blue','Blue fuel pump','desert',[.65,1.6,.45],1800,'One small petrol-blue vintage fuel pump in same family, dark display and close-fitting thick black hose/nozzle. No text, surrounding scenery or ground.','Second colour variant.'),
        ('road-bollard','Desert roadside bollard','desert',[.22,.85,.22],700,'One short square warm-grey wooden roadside bollard with slightly lighter painted top, no ground, road or other posts.','Repeat along roadside.')]:
        model(i,t,'buildings-props',b,d,f,desc,use)
    # Measured modules: exact dimensions and attachment surfaces take priority over generated geometry.
    for i,t,b,d,desc,use,deps in [
        ('fence-post','Timber fence post','farm',[.16,1.1,.16],'One plain square warm-brown timber fence post, painted grain, bevelled top, no ground or rail.','Exact box post with bottom pivot; repeat at 2m centres.',('texture-wood',)),
        ('fence-rail','Timber fence rail','farm',[2,.12,.09],'One straight horizontal warm timber rail, squared ends, broad painted wood strokes, no posts or ground.','Exact 2m rail, two instances per span; rotate for corners.',('texture-wood',)),
        ('ground-tile','Universal ground tile','all',[16,0,16],'One isolated flat square painterly ground tile on white, no props or terrain border.','16m square plane, shared UV layout; choose biome material.',('texture-grass','texture-sand')),
        ('path-straight','Farm path straight','farm',[3,0,16],'One isolated straight beige dirt path ribbon with soft painted edges, no grass, rocks or props.','16m straight flat ribbon; overlays grass with small height offset.',('texture-dirt',)),
        ('path-corner','Farm path bend','farm',[9,0,9],'One isolated quarter-circle beige dirt path ribbon of consistent width, no surrounding grass or props.','90 degree bend, 6m centre radius, 3m width.',('texture-dirt',)),
        ('soil-bed','Tilled crop bed','farm',[1,0,4],'One flat long dark brown painted tilled-soil rectangle, no seedlings, fence, rocks or props.','4m repeatable soil strip; seedlings remain separate objects.',('texture-soil','crop-seedling')),
        ('road-straight','Street straight','suburb,desert',[6,0,16],'One isolated flat slate asphalt road rectangle, no kerbs, cars, markings or scenery.','6m x 16m surface; biome material plus separate markings.',('texture-asphalt',)),
        ('road-corner','Street corner','suburb,desert',[12,0,12],'One isolated quarter-circle asphalt road surface, consistent lane width, no props or pavement.','90 degree road bend, 9m centre radius, 6m width.',('texture-asphalt',)),
        ('road-junction','Street T junction','suburb,desert',[16,0,16],'One isolated flat T-shaped asphalt junction surface, no markings, kerbs or props.','6m road mouths on 16m grid; rotate for all junction orientations.',('texture-asphalt',)),
        ('pavement-straight','Pavement straight and kerb','suburb',[1.5,.16,16],'One straight pale-stone sidewalk slab with raised outer kerb, painted block joints, no streetlamp or other props.','16m long, 1.5m wide, 0.16m high, continuous edge joins.',('texture-pavement',)),
        ('pavement-corner','Curved pavement and kerb','suburb',[13.5,.16,13.5],'One 90-degree curved pale-stone pavement and kerb segment, consistent width, no road or scenery.','Quarter annulus adjacent to road-corner; matching 1.5m width.',('texture-pavement','road-corner')),
        ('pavement-end','Pavement termination','suburb',[1.5,.16,.4],'One short pale-stone pavement end-cap with bevelled kerb finish, no other object.','Caps exposed pavement ends without open edges.',('texture-pavement',))]:
        add(i,t,'measured-modules','procedural_model',b,d,0,desc,use,deps)
    for i,t,desc,b in [
        ('texture-grass','Painterly sage grass','Soft dusty sage grass with broad restrained olive painted strokes, no distinct plants, stones or cast shadows.','farm,suburb'),
        ('texture-dirt','Painterly beige dirt','Warm quiet beige dirt, broad subtle painted tones, no pebbles or grass silhouettes.','farm'),
        ('texture-soil','Painterly tilled soil','Quiet warm dark brown soil with broad lengthwise painted furrow shapes, no plants.','farm'),
        ('texture-asphalt','Painterly slate asphalt','Quiet slate-blue asphalt, broad subtle blue-grey brush patches; no markings, cracks, litter or cast shadows.','suburb,desert'),
        ('texture-pavement','Painterly pale stone','Warm pale stone, soft broad brush variation, regular large slab joints; no objects.','suburb'),
        ('texture-sand','Painterly ochre sand','Quiet pale ochre/peach sand with broad warm/cool brushed patches, no rocks, footprints or plants.','desert'),
        ('texture-wood','Painterly warm timber','Warm brown timber with broad simple lengthwise grain brushstrokes, no knots larger than small scale, no objects.','farm,desert')]:
        add(i,t,'materials','tileable_texture',b,[1,0,1],0,desc,'Generate white-background flat material reference, then a separate seamless albedo tile. 1024px texture, no PBR maps.')
    for i,t,desc in [
        ('fx-capture-ring','Capture radius ring','Mint thin circular ground decal with restrained segmented strokes and soft edge.'),
        ('fx-crop-circle','Crop circle stamp','Faint flattened-grass concentric ring pattern, neutral mask tinted by ground material.'),
        ('fx-capture-dot','Capture trail dot','One mint emissive rounded dot; runtime paths repeat this dot, not a frozen curved mesh.'),
        ('fx-laser','Laser strip','Thin mint core and soft edge, repeatable short strip.'),
        ('fx-hit-spark','Small interception spark','Small mint/ivory six-ray starburst, restrained size.'),
        ('fx-exhaust-puff','Projectile exhaust puff','One soft pale warm-grey simple puff; particles repeat it.'),
        ('fx-blob-shadow','Soft actor shadow','One soft charcoal ellipse on transparent background; tint/scale at placement.'),
        ('fx-threat-chevron','Military double chevron','Two bold coral angular chevrons, no backing panel.'),
        ('fx-shield-shell','Shield shell','Subtle mint spherical field surface with thin edge treatment; no saucer baked in.'),
        ('ui-coin','Score coin','Simple amber circular score coin, no digits baked in.'),
        ('ui-beam','Beam upgrade icon','Mint UFO intake/rings symbol, no text or card baked in.'),
        ('ui-laser','Laser upgrade icon','Coral laser/starburst symbol, no text or card baked in.'),
        ('ui-shield','Shield icon','Mint simple shield outline/fill used for both health pips and upgrade.'),
        ('ui-hud-panel','HUD capsule','Navy rounded capsule background, no score or level digits baked in.'),
        ('ui-xp-track','XP meter track','Navy rounded XP meter trough and separate mint fill element; no text baked in.'),
        ('ui-upgrade-card','Upgrade card background','Compact ivory/navy rounded card background, no icon, text or level digits baked in.')]:
        add(i,t,'effects-ui','native_graphic','all',None,0,desc,'Native reusable vector/shader/mesh asset with a white-background reference preview; transparent render where applicable. Never send to Tripo.')
    ids=[x['id'] for x in assets]
    assert len(ids)==len(set(ids)), 'Duplicate asset ID'
    for a in assets:
        assert all(x in ids for x in a['dependencies']), (a['id'],a['dependencies'])
        assert a['face_limit']<=8000
    manifest=dict(version=1,status='inventory-reviewed-production-pending',
        previous_goal_turn='progress: produced and saved the approved painterly style guide; no individual model jobs existed at audit',
        style_guide='art/style-guide/ufo-asset-style-guide.png',
        style_frames=['art/style-frames/balanced-locations/01-sage-farmland-balanced.png','art/style-frames/balanced-locations/02-slate-suburbia-balanced.png','art/style-frames/balanced-locations/03-mauve-desert-balanced.png'],
        units='metres',coordinates='Right-handed, Y up, +Z nominal forward; grounded pivots at base centre; sockets described separately.',
        quality_gates=['independent white-background reference visually approved','dry-run valid:true and exact requested params before Tripo submission','task IDs retained; timed-out jobs resumed, never resubmitted','downloaded GLTF inspected for geometry, UVs, images and texture dimensions','normalised scale/pivot and modular joins checked','multi-angle render visually reviewed against source/reference','accepted only after structural and visual QA'],
        scope='Visual model, terrain, material, effect and UI art assets for the three agreed locations. No engine gameplay, audio, rigging or animation generation.',
        assets=assets)
    target=OUT/'inventory.json'
    if target.exists():
        raise SystemExit('Inventory exists; revise it deliberately rather than reset production state.')
    target.write_text(json.dumps(manifest,indent=2)+'\n')
    counts=Counter(x['kind'] for x in assets)
    plan=['UFO ASSET PRODUCTION PLAN — REVIEWED INVENTORY','',f'{len(assets)} deliverables: '+', '.join(f'{v} {k}' for k,v in counts.items()),'',
      'BUILDING THE LEVELS',
      'Use a 16m scrolling grid with a broad central playable area. Ground planes and measured path/road/pavement modules repeat. Freeform models decorate edges and act as targets or threats.',
      'Farmland: grass ground + dirt straight/bend strips, independent soil beds with seedling instances, fence posts and rails at 2m centres, barns/silos/windmills and separate hay/trees/rocks.',
      'Suburbia: 6m streets with bends and T junctions, 1.5m sidewalks with matching corners/end caps, independent houses/lamp/mailbox/bin/hedge placement, quiet road space for gameplay.',
      'Desert: sand ground with recoloured road modules, boundary cliffs/boulders/cacti, separate canopy and fuel pumps, individual roadside bollards and scrub.',
      'Shared cast and military remain the same across all biomes. Match the painterly style guide rather than the earlier textureless frames.',
      '', 'REVIEW CHANGES MADE BEFORE PRODUCTION',
      '- Farmer hat and resident cap separated so headwear can move independently; reference images must not include those items on their bodies.',
      '- Jeep driver and all wheels separated from chassis. The soldier is shared; do not generate a new driver fused into the vehicle.',
      '- Tank hull and turret separated for independent aiming; cannon remains attached to turret. Tracks belong to hull.',
      '- Windmill rotor separated from mast. Canopy separated from fuel pumps. Plants/hedges separated from buildings. Crops assembled from seedling instances and soil beds.',
      '- Ground, roads, pavement and fencing use exact measured geometry; variants are made through reusable modules/materials instead of baked mini-scenes.',
      '- Faint crop-circle decal is separate from live capture ring. Trail is instanced dots, not a frozen curve. Score/level text is dynamic; no digits baked into UI art.',
      '- Added bare-head body references, upgrade attachment meshes, road bends/junctions, pavement termination, tilled soil, reusable shadow, shield shell and UI primitives missing from the sheet.',
      '- Existing round tree, cactus, rock, mailbox, house and fuel-pump variations explicitly enumerated. No helicopters or new animal species added.',
      '', 'PIPELINE',
      'Generate one isolated white-background image per inventory item/variation using the approved sheet and relevant biome frame. Inspect it; regenerate if components, silhouettes, colours or isolation are wrong.',
      'For Tripo model items, run the user-prescribed direct CLI dry run, validate JSON and then execute with --yes. Textures enabled; PBR disabled; original_image alignment; UVs; P1; GLTF conversion at 1024px. No rigging/retexturing.',
      'Measured modules and native graphics are built to exact specs from the approved references. Seamless albedo tiles are generated separately from their white-backed material references.',
      'Review downloaded geometry, UVs, texture size, dimensions and multi-angle renders. Remake failures. Keep original downloads and all task IDs. Publish a local review gallery with every reference, final asset, QA outcome and download.',
      '', 'INDIVIDUAL ASSETS']
    for a in assets:
        plan.extend([f"{a['id']} | {a['title']} | {a['kind']} | {','.join(a['biomes'])}",f"  Make: {a['description']}",f"  Assemble: {a['construction']}"])
    (OUT/'construction-plan.txt').write_text('\n'.join(plan)+'\n')
    print(json.dumps({'total':len(assets),'counts':dict(counts),'inventory':str(target)},indent=2))

if __name__=='__main__':
    main()
