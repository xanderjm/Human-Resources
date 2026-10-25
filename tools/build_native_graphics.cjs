#!/usr/bin/env node
/** Offline editable vector assets. Requires the existing authoring-only sharp tool. */
const fs = require('node:fs/promises');
const path = require('node:path');
const crypto = require('node:crypto');
const sharp = require('./model-viewer/node_modules/sharp');
const ROOT = path.resolve(__dirname, '..');
const C = { navy:'#183348', mint:'#72f8d1', pale:'#c5fff0', ivory:'#fff1d6', coral:'#ff714f', amber:'#ffc449' };
const assets = [];
function add(id, width, height, body, options={}) { assets.push({id,width,height,body,...options}); }
function svg(width,height,body) { return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">${body}</svg>`; }
function radial(id,colour,stops) { return `<radialGradient id="${id}">${stops.map(([offset,opacity])=>`<stop offset="${offset}%" stop-color="${colour}" stop-opacity="${opacity}"/>`).join('')}</radialGradient>`; }
function line(x1,y1,x2,y2,stroke,width,extra='') { return `<path d="M${x1} ${y1}L${x2} ${y2}" stroke="${stroke}" stroke-width="${width}" fill="none" ${extra}/>`; }
const round='stroke-linecap="round" stroke-linejoin="round"';

add('fx-capture-ring',512,512,`<defs>${radial('a',C.mint,[[0,0],[77,0],[83,.06],[88,.21],[93,0],[100,0]])}</defs><circle cx="256" cy="256" r="246" fill="url(#a)"/><g fill="none" stroke="${C.mint}"><circle cx="256" cy="256" r="212" stroke-width="3" opacity=".58"/><circle cx="256" cy="256" r="226" stroke-width="6" stroke-dasharray="34 16" ${round}/></g>`,{use:'Ground decal; scale to capture radius. Alpha blend, clamp both axes. No ground baked in.'});
add('fx-crop-circle',512,512,`<g fill="none" stroke="white" stroke-width="12"><circle cx="256" cy="256" r="218" stroke-dasharray="81 8"/><circle cx="256" cy="256" r="180" stroke-width="18"/><circle cx="256" cy="256" r="134" stroke-dasharray="54 8"/><circle cx="256" cy="256" r="88" stroke-width="18"/></g><circle cx="256" cy="256" r="45" fill="white"/>`,{use:'Linear alpha mask tinted by the ground material; individual clamp decal, not a terrain tile.',previewTint:'#9aa37b',mask:true});
add('fx-capture-dot',128,128,`<defs>${radial('a',C.mint,[[0,.7],[28,.55],[65,.13],[100,0]])}</defs><circle cx="64" cy="64" r="55" fill="url(#a)"/><rect x="45" y="45" width="38" height="38" rx="13" fill="${C.mint}"/><rect x="51" y="49" width="26" height="15" rx="6" fill="${C.pale}" opacity=".7"/>`,{use:'Instance one dot along runtime capture paths; no trail curve baked in. Clamp both axes.'});
// No variation along U: exact RGBA equality at the two longitudinal edges, including derivatives.
add('fx-laser',512,128,`<defs><linearGradient id="a" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${C.mint}" stop-opacity="0"/><stop offset="32%" stop-color="${C.mint}" stop-opacity=".06"/><stop offset="46%" stop-color="${C.mint}" stop-opacity=".4"/><stop offset="50%" stop-color="${C.ivory}"/><stop offset="54%" stop-color="${C.mint}" stop-opacity=".4"/><stop offset="68%" stop-color="${C.mint}" stop-opacity=".06"/><stop offset="100%" stop-color="${C.mint}" stop-opacity="0"/></linearGradient></defs><rect width="512" height="128" fill="url(#a)"/>`,{use:'Repeat U (longitudinal X), clamp V. Stretch/instance strip between endpoints. Ends need separately authored runtime caps only if desired.',repeatAxis:'u',allowedEdgeContact:true});
const spark=Array.from({length:12},(_,i)=>{const angle=-Math.PI/2+i*Math.PI/6,r=i%2===0?180:42;return `${256+Math.cos(angle)*r},${256+Math.sin(angle)*r}`;}).join(' ');
add('fx-hit-spark',512,512,`<polygon points="${spark}" fill="${C.mint}" opacity=".3" transform="translate(-38.4 -38.4) scale(1.15)"/><polygon points="${spark}" fill="${C.ivory}"/>`,{use:'One restrained impact sprite; tintable, alpha blend, clamp both axes.'});
add('fx-exhaust-puff',256,256,`<defs>${radial('a','#ded3c1',[[0,.65],[45,.48],[75,.14],[100,0]])}</defs><circle cx="128" cy="128" r="106" fill="url(#a)"/><path d="M74 152C46 127 61 90 94 90C93 48 154 47 166 87C205 77 225 139 184 159C167 192 108 200 91 167C84 166 76 160 74 152" fill="#eee3d0" opacity=".32"/>`,{use:'Instance one puff with runtime scale/fade; clamp both axes. No exhaust path or rocket baked in.'});
add('fx-blob-shadow',512,256,`<defs><radialGradient id="a"><stop offset="0" stop-color="#203341" stop-opacity=".42"/><stop offset="45%" stop-color="#203341" stop-opacity=".30"/><stop offset="78%" stop-color="#203341" stop-opacity=".10"/><stop offset="100%" stop-color="#203341" stop-opacity="0"/></radialGradient></defs><ellipse cx="256" cy="128" rx="230" ry="104" fill="url(#a)"/>`,{use:'Reusable soft ground ellipse; multiply or alpha blend. Scale/tint per actor. Clamp both axes.'});
add('fx-threat-chevron',512,512,`<g fill="${C.coral}"><path d="M94 130L214 246Q224 256 214 266L94 382L50 337L134 256L50 175Z"/><path d="M284 130L404 246Q414 256 404 266L284 382L240 337L324 256L240 175Z"/></g>`,{use:'Rotate in presentation to point toward threat; clamp both axes. Shape carries meaning beyond colour.'});
add('fx-shield-shell',512,512,`<defs>${radial('a',C.mint,[[0,.015],[65,.025],[84,.08],[92,.22],[96,.55],[98,.05],[100,0]])}</defs><circle cx="256" cy="256" r="225" fill="url(#a)"/><g stroke="${C.pale}" fill="none" stroke-linecap="round"><path d="M92 201A182 182 0 0 1 180 99" stroke-width="4" opacity=".8"/><path d="M347 409A182 182 0 0 0 410 337" stroke-width="3" opacity=".45"/><ellipse cx="256" cy="256" rx="219" ry="62" stroke-width="1.5" opacity=".16"/></g>`,{use:'Billboard fallback/style reference only. Unit sphere GLB and HLSL rim helper included for view-dependent 3D field; renderer integration required. Clamp sprite axes.'});
add('ui-coin',512,512,`<circle cx="256" cy="263" r="189" fill="#9c6b31"/><circle cx="256" cy="249" r="189" fill="${C.amber}"/><path d="M104 230A153 153 0 0 1 337 124" stroke="${C.ivory}" stroke-width="14" fill="none" opacity=".8" ${round}/><circle cx="256" cy="249" r="137" stroke="#c48b32" stroke-width="8" fill="none"/><path d="M154 298L256 161L358 298Z" fill="#dfa43d" opacity=".45"/>`,{use:'Score symbol only; dynamic digits rendered separately. Clamp both axes.'});
add('ui-beam',512,512,`<g fill="none" stroke="${C.mint}" ${round}><ellipse cx="256" cy="154" rx="102" ry="26" stroke-width="18"/><path d="M194 196L163 323M318 196L349 323" stroke-width="13"/><ellipse cx="256" cy="285" rx="66" ry="20" stroke-width="13"/><ellipse cx="256" cy="345" rx="121" ry="33" stroke-width="18"/></g><path d="M211 136A45 45 0 0 1 301 136Z" fill="${C.pale}"/><path d="M240 220L272 220L291 302L221 302Z" fill="${C.mint}" opacity=".30"/>`,{use:'Standalone upgrade icon, no text/card. Clamp both axes.'});
add('ui-laser',512,512,`<path d="M111 401L302 210" stroke="${C.coral}" stroke-width="34" ${round}/><path d="M112 400L301 211" stroke="${C.ivory}" stroke-width="9" ${round}/><path d="M327 78L341 195L428 146L369 227L473 256L364 270L421 357L337 300L315 421L300 303L211 355L267 273L154 250L270 234L214 145L299 198Z" fill="${C.coral}"/><path d="M324 170L335 237L397 251L337 266L319 334L306 270L245 252L307 238Z" fill="${C.ivory}"/>`,{use:'Standalone upgrade icon, no text/card. Clamp both axes.'});
add('ui-shield',512,512,`<path d="M256 58L415 122L399 275Q383 386 256 456Q129 386 113 275L97 122Z" fill="${C.navy}" stroke="${C.mint}" stroke-width="19" stroke-linejoin="round"/><path d="M256 106L369 151L357 271Q345 350 256 404Q167 350 155 271L143 151Z" fill="${C.mint}"/><path d="M256 106L369 151L357 271Q345 350 256 404Z" fill="#3dc8bf"/>`,{use:'Health pip or upgrade icon. Clamp both axes; states tint/alpha can be applied by presentation.'});
add('ui-hud-panel',768,192,`<rect x="16" y="16" width="736" height="160" rx="60" fill="${C.navy}" fill-opacity=".92"/><path d="M76 26H692" stroke="#486275" stroke-width="3" stroke-linecap="round" opacity=".8"/>`,{use:'Nine-slice background; cap inset 80px horizontal, 40px vertical. No glyphs/digits.',sliceInsets:[80,40,80,40]});
add('ui-xp-track',768,128,`<rect x="16" y="24" width="736" height="80" rx="40" fill="${C.navy}"/><rect x="26" y="34" width="716" height="60" rx="30" fill="#102739"/><rect x="26" y="34" width="716" height="60" rx="30" fill="none" stroke="#486275" stroke-width="3"/>`,{use:'Track only. Independent fill.svg/fill.png uses same canvas and inner rect; clip fill horizontally by progress. Do not stretch end caps.',sliceInsets:[64,40,64,40],fill:`<rect x="30" y="38" width="708" height="52" rx="26" fill="${C.mint}"/><path d="M58 45H710" stroke="${C.pale}" stroke-width="6" ${round} opacity=".5"/>`});
add('ui-upgrade-card',512,640,`<rect x="24" y="24" width="464" height="592" rx="38" fill="${C.navy}"/><rect x="34" y="34" width="444" height="572" rx="30" fill="${C.ivory}"/><rect x="34" y="34" width="444" height="170" rx="30" fill="${C.navy}"/><rect x="34" y="170" width="444" height="34" fill="${C.navy}"/><path d="M68 224H444" stroke="#dccba9" stroke-width="3" ${round}/>`,{use:'Nine-slice card background; inset [64,224,64,64]px. Populate icon and dynamic text separately.',sliceInsets:[64,224,64,64]});
add('road-centre-dash',1024,256,`<path d="M81 82L150 80L157 77L300 81L420 78L536 81L650 77L780 80L920 78L944 85L941 169L866 174L723 171L600 175L474 171L350 175L231 171L92 174L78 166Z" fill="#f6e8cc"/><path d="M116 92L319 89L510 93L722 88L918 91" stroke="#fff6e3" stroke-width="8" stroke-linecap="round" opacity=".5"/><path d="M106 161L348 164L573 160L807 163L928 158" stroke="#d6c9af" stroke-width="3" stroke-linecap="round" opacity=".35"/>`,{use:'Individual isolated painted ivory road marking: visible footprint about 1m long ×0.12m wide. Instance at regular road centre intervals with transparent gaps. Clamp both axes; no asphalt baked in.',dimensions_m:[1,.001,.12]});

function hash(bytes) { return crypto.createHash('sha256').update(bytes).digest('hex'); }
async function savePNG(source,file) { await sharp(Buffer.from(source)).png().toFile(file); }
async function auditPNG(file,a) {
 const {data,info}=await sharp(file).ensureAlpha().raw().toBuffer({resolveWithObject:true});
 let minX=info.width,minY=info.height,maxX=-1,maxY=-1,minAlpha=255,maxAlpha=0, borderMax=0,visiblePixels=0;
 for(let y=0;y<info.height;y++) for(let x=0;x<info.width;x++) {
  const alpha=data[(y*info.width+x)*4+3]; minAlpha=Math.min(minAlpha,alpha); maxAlpha=Math.max(maxAlpha,alpha);
  if(x===0||y===0||x===info.width-1||y===info.height-1) borderMax=Math.max(borderMax,alpha);
  if(alpha>0) {minX=Math.min(minX,x);minY=Math.min(minY,y);maxX=Math.max(maxX,x);maxY=Math.max(maxY,y);visiblePixels++;}
 }
 let seam=null;
 if(a.repeatAxis==='u') {
  let max=0,derivative=0;
  for(let y=0;y<info.height;y++) for(let c=0;c<4;c++) {
   const get=x=>data[(y*info.width+x)*4+c];
   max=Math.max(max,Math.abs(get(0)-get(info.width-1)));
   derivative=Math.max(derivative,Math.abs((get(1)-get(0))-(get(info.width-1)-get(info.width-2))));
  }
  seam={axis:'u',max_rgba_edge_difference:max,max_derivative_difference:derivative,passed:max===0&&derivative===0};
 }
 if(minAlpha!==0||maxAlpha===0||(!a.allowedEdgeContact&&borderMax!==0)||(seam&&!seam.passed)) throw new Error(`PNG alpha or seam audit failed: ${a.id}`);
 return {dimensions_px:[info.width,info.height],rgba:true,alpha_range:[minAlpha,maxAlpha],alpha_nonzero_bbox:[minX,minY,maxX+1,maxY+1],transparent_margin_px:[minX,minY,info.width-maxX-1,info.height-maxY-1],border_alpha_max:borderMax,visible_pixels:visiblePixels,repeat_axis_check:seam,sha256:hash(await fs.readFile(file))};
}
async function build(a) {
 const relative=`assets/graphics/${a.id}`; const dir=path.join(ROOT,relative);
 const recordPath=path.join(ROOT,'assets/production/records',a.id,'native-audit.json');
 let previous=null;try{previous=JSON.parse(await fs.readFile(recordPath,'utf8'));}catch(error){if(error.code!=='ENOENT')throw error;}
 await fs.mkdir(dir,{recursive:true});
 const source=svg(a.width,a.height,a.body); if(/<image|<script|<text|href=|url\(https?:/i.test(source)) throw new Error(`External/text dependency: ${a.id}`);
 await fs.writeFile(path.join(dir,`${a.id}.svg`),source+'\n');
 await savePNG(source,path.join(dir,`${a.id}.png`));
 const alphaAudit=await auditPNG(path.join(dir,`${a.id}.png`),a);
 const refdir=path.join(ROOT,'assets/references',a.id); await fs.mkdir(refdir,{recursive:true});
 // A tintable white mask needs a neutral olive display tint only in its white-background reference.
 let refBody=a.previewTint?a.body.replaceAll('white',a.previewTint):a.body;
 if(a.id==='ui-xp-track') refBody+=a.fill.replace('width="708"','width="412"').replace('H710','H414');
 const previewSVG=svg(a.width,a.height,refBody);
 await sharp(Buffer.from(previewSVG)).resize({width:768,height:768,fit:'contain',background:'#ffffff'}).flatten({background:'#ffffff'}).png().toFile(path.join(refdir,'v1.png'));
 const variants=[];
 if(a.fill) {
  const fillSource=svg(a.width,a.height,a.fill);
  await fs.writeFile(path.join(dir,'fill.svg'),fillSource+'\n'); await savePNG(fillSource,path.join(dir,'fill.png'));
  variants.push({role:'independently usable XP fill',path:`${relative}/fill.png`,audit:await auditPNG(path.join(dir,'fill.png'),a)});
 }
 const metadata={schema_version:1,id:a.id,type:a.mask?'alpha-mask':'native-graphic',source:`${relative}/${a.id}.svg`,png:`${relative}/${a.id}.png`,reference:`assets/references/${a.id}/v1.png`,use:a.use,colour_space:a.mask?'linear alpha mask; ignore RGB':'sRGB colour, linear alpha',alpha_encoding:'straight/unassociated alpha',blend:'source-alpha / one-minus-source-alpha; emissive effects may use additive blend in renderer',sampler:{u:a.repeatAxis==='u'?'repeat':'clamp-to-edge',v:'clamp-to-edge'},...(a.sliceInsets?{nine_slice_insets_px:a.sliceInsets}:{}),...(a.dimensions_m?{dimensions_m:a.dimensions_m}:{}),variants};
 await fs.writeFile(path.join(dir,'asset.json'),JSON.stringify(metadata,null,2)+'\n');
 const recordDir=path.join(ROOT,'assets/production/records',a.id); await fs.mkdir(recordDir,{recursive:true});
 const audit={schema_version:1,id:a.id,status:'structural-passed-visual-pending',production:{status:'generated',file:metadata.png,source:metadata.source,metadata:`${relative}/asset.json`},reference:{status:'generated',path:metadata.reference,sha256:hash(await fs.readFile(path.join(refdir,'v1.png')))},qa:{status:'structural-passed-visual-pending'},alpha_audit:alphaAudit,variants,svg:{self_contained:true,text_baked:false,external_dependencies:false},usage:metadata.use,limitations:'Art production asset. Native runtime upload, blend, depth/occlusion and device readability are not qualified.'};
 if(previous?.alpha_audit?.sha256===alphaAudit.sha256 && previous?.reference?.sha256===audit.reference.sha256 && JSON.stringify(previous?.variants)===JSON.stringify(variants) && previous?.qa?.status==='art-review-passed') {
  audit.status=previous.status; audit.qa=previous.qa; audit.reference=previous.reference;
  if(previous.supplemental_geometry) audit.supplemental_geometry=previous.supplemental_geometry;
 }
 await fs.writeFile(path.join(recordDir,'native-audit.json'),JSON.stringify(audit,null,2)+'\n');
 return {...a,...metadata,audit};
}
async function shieldSupport() {
 const dir=path.join(ROOT,'assets/graphics/fx-shield-shell');
 const positions=[],normals=[],indices=[],segments=32,rings=16;
 for(let j=0;j<=rings;j++) for(let i=0;i<=segments;i++) {
  const phi=Math.PI*j/rings,theta=2*Math.PI*i/segments;
  const p=[Math.sin(phi)*Math.cos(theta),Math.cos(phi),Math.sin(phi)*Math.sin(theta)]; positions.push(...p); normals.push(...p);
 }
 for(let j=0;j<rings;j++) for(let i=0;i<segments;i++) {
  const a=j*(segments+1)+i,b=a+segments+1;
  // Outward winding, no zero-area triangles at sphere poles.
  if(j>0) indices.push(a,a+1,b); if(j<rings-1) indices.push(a+1,b+1,b);
 }
 const pos=Buffer.from(new Float32Array(positions).buffer),norm=Buffer.from(new Float32Array(normals).buffer),idx=Buffer.from(new Uint16Array(indices).buffer);
 const binary=Buffer.concat([pos,norm,idx]); const padded=Buffer.concat([binary,Buffer.alloc((4-binary.length%4)%4)]);
 const doc={asset:{version:'2.0',generator:'Human Resources native-graphic authoring'},scene:0,scenes:[{nodes:[0]}],nodes:[{name:'Shield_Unit_Sphere',mesh:0}],meshes:[{primitives:[{attributes:{POSITION:0,NORMAL:1},indices:2,material:0}]}],materials:[{name:'Shield_Fallback_Mint',pbrMetallicRoughness:{baseColorFactor:[.447,.973,.82,.10],metallicFactor:0,roughnessFactor:1},alphaMode:'BLEND',doubleSided:false}],buffers:[{byteLength:binary.length}],bufferViews:[{buffer:0,byteOffset:0,byteLength:pos.length,target:34962},{buffer:0,byteOffset:pos.length,byteLength:norm.length,target:34962},{buffer:0,byteOffset:pos.length+norm.length,byteLength:idx.length,target:34963}],accessors:[{bufferView:0,componentType:5126,count:positions.length/3,type:'VEC3',min:[-1,-1,-1],max:[1,1,1]},{bufferView:1,componentType:5126,count:normals.length/3,type:'VEC3'},{bufferView:2,componentType:5123,count:indices.length,type:'SCALAR'}]};
 let json=Buffer.from(JSON.stringify(doc));json=Buffer.concat([json,Buffer.alloc((4-json.length%4)%4,32)]);
 const header=Buffer.alloc(12);header.writeUInt32LE(0x46546c67);header.writeUInt32LE(2,4);header.writeUInt32LE(12+8+json.length+8+padded.length,8);
 const jh=Buffer.alloc(8);jh.writeUInt32LE(json.length);jh.writeUInt32LE(0x4e4f534a,4);const bh=Buffer.alloc(8);bh.writeUInt32LE(padded.length);bh.writeUInt32LE(0x004e4942,4);
 await fs.writeFile(path.join(dir,'unit-sphere.glb'),Buffer.concat([header,jh,json,bh,padded]));
 await fs.writeFile(path.join(dir,'shield-rim.hlsl'),`// Renderer-neutral material helper, not a compiled runtime pipeline.\n// Inputs: unit world-space normal and unit direction toward camera.\n// Return straight-alpha colour. Use alpha blend, depth test, no depth writes.\n// Draw one visible sphere surface; cull backfaces to avoid double opacity.\nfloat4 HumanResourcesShieldRim(float3 normalWS, float3 toCameraWS, float3 tint, float opacity)\n{\n    float rim = pow(1.0 - saturate(abs(dot(normalize(normalWS), normalize(toCameraWS)))), 3.5);\n    return float4(tint, saturate(opacity * (0.025 + rim * 0.55)));\n}\n`);
 const contract={unit_radius_m:1,pivot:[0,0,0],triangles:indices.length/3,vertices:positions.length/3,scale:'Uniform scale to desired field radius; centre on UFO assembly.',shader:'shield-rim.hlsl is an uncompiled renderer-neutral helper; bind normal/view direction/tint/opacity in production renderer.',fallback:'GLB has flat transparent fallback material only; use shader helper for camera-dependent rim.',not_in_runtime_package:true};
 await fs.writeFile(path.join(dir,'sphere-contract.json'),JSON.stringify(contract,null,2)+'\n');
}
async function contactSheet(items) {
 const tw=360,th=310,cols=4,rows=Math.ceil(items.length/cols),width=tw*cols,height=80+th*rows;
 const comps=[];
 for(let i=0;i<items.length;i++) {
  const a=items[i],x=(i%cols)*tw,y=80+Math.floor(i/cols)*th;
  const tile=Buffer.from(svg(tw,th,`<rect width="360" height="310" fill="#f3f0e8"/><rect x="10" y="10" width="340" height="252" rx="16" fill="${a.id.startsWith('fx-')?'#314957':'#d9dfd8'}"/><text x="18" y="282" font-family="sans-serif" font-size="16" font-weight="bold" fill="${C.navy}">${a.id}</text><text x="18" y="302" font-family="sans-serif" font-size="12" fill="#546674">${a.repeatAxis==='u'?'Repeat U; RGBA seam checked':'Isolated sprite / clamp UV'}</text>`));
  comps.push({input:await sharp(tile).png().toBuffer(),left:x,top:y});
  const src=path.join(ROOT,a.png),pic=await sharp(src).resize({width:300,height:218,fit:'inside'}).toBuffer({resolveWithObject:true});
  comps.push({input:pic.data,left:x+Math.round((tw-pic.info.width)/2),top:y+22+Math.round((218-pic.info.height)/2)});
 }
 const heading=Buffer.from(svg(width,80,`<rect width="${width}" height="80" fill="#f3f0e8"/><text x="24" y="34" font-family="sans-serif" font-size="25" font-weight="bold" fill="${C.navy}">HUMAN RESOURCES / NATIVE EFFECTS + UI</text><text x="24" y="61" font-family="sans-serif" font-size="16" fill="#546674">Editable vectors · transparent sprites · separate XP fill · no baked text in assets</text>`));
 comps.unshift({input:await sharp(heading).png().toBuffer(),left:0,top:0});
 await fs.mkdir(path.join(ROOT,'assets/production/reviews'),{recursive:true});
 await sharp({create:{width,height,channels:4,background:'#f3f0e8'}}).composite(comps).png().toFile(path.join(ROOT,'assets/production/reviews/native-graphics-contact-sheet.png'));
 // Repeated laser proof uses eight actual source PNGs with no overlap or seam concealment.
 const laser=items.find(a=>a.id==='fx-laser');const laserComps=[];
 for(let i=0;i<8;i++) laserComps.push({input:path.join(ROOT,laser.png),left:512*i,top:0});
 await sharp({create:{width:4096,height:128,channels:4,background:{r:0,g:0,b:0,alpha:0}}}).composite(laserComps).png().toFile(path.join(ROOT,'assets/production/reviews/laser-repeat-proof.png'));
}
(async()=>{
 const built=[];for(const a of assets) built.push(await build(a));
 await shieldSupport();
 await contactSheet([...built.slice(0,-1),{id:'ui-xp-fill (independent)',png:'assets/graphics/ui-xp-track/fill.png'},built.at(-1)]);
 const manifest={schema_version:1,assets:built.map(a=>({id:a.id,png:a.png,source:a.source,audit:`assets/production/records/${a.id}/native-audit.json`,status:a.audit.status})),asset_count:17,transparent_png_count:18,self_contained_svg_count:18,white_background_references:17,laser_repeat_axis:'u',laser_rgba_edge_difference:0,laser_derivative_difference:0,contact_sheet:'assets/production/reviews/native-graphics-contact-sheet.png',repeat_proof:'assets/production/reviews/laser-repeat-proof.png',runtime_qualified:false};
 await fs.writeFile(path.join(ROOT,'assets/graphics/native-graphics-manifest.json'),JSON.stringify(manifest,null,2)+'\n');
 await fs.writeFile(path.join(ROOT,'assets/graphics/README.md'),`# Native visual assets\n\nRebuild with \`node tools/build_native_graphics.cjs\` using the existing authoring-only Sharp installation. SVGs are editable vector masters for this edition; preserve hand edits in the generator before rebuilding. PNGs are transparent sRGB/straight-alpha exports. All assets are original project-owned vectors without external images, text or font dependencies.\n\nEach folder includes usage and sampling contracts in \`asset.json\`. Only the laser strip repeats its U axis. Its opposite edge RGBA values and first differences are exact matches. The other effects, icons and cards are isolated clamp sprites; road markings and capture dots repeat by instancing with transparent spacing. A terrain tile must never be substituted with one of these isolated sprites.\n\nXP trough and mint fill are independent files. Fill is clipped to progress by the future UI owner. HUD/card corners require nine-slice handling. Crop-circle RGB is white and alpha is a tintable linear mask; its white-background reference alone is olive tinted for visibility. The shield includes an uncompiled HLSL material helper and unit-sphere GLB: its 2D image is only a billboard fallback, with no UFO baked in.\n\nThese are authoring/production inputs, outside the runtime shipping allowlist. Runtime colour, blend, mip filtering, depth/occlusion, HUD placement and physical-device readability remain integration checks.\n`);
 console.log(JSON.stringify({built:built.length,transparent_pngs:built.length+1,contact_sheet:'assets/production/reviews/native-graphics-contact-sheet.png',repeat_proof:'assets/production/reviews/laser-repeat-proof.png'},null,2));
})().catch(error=>{console.error(error);process.exitCode=1;});
