import * as THREE from 'three';
import {GLTFLoader} from 'three/addons/loaders/GLTFLoader.js';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';

const $=s=>document.querySelector(s),stage=$('#stage');
const renderer=new THREE.WebGLRenderer({antialias:true,preserveDrawingBuffer:true});
renderer.setPixelRatio(Math.min(devicePixelRatio,2));
renderer.outputColorSpace=THREE.SRGBColorSpace;
renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=1.0;
stage.prepend(renderer.domElement);
const scene=new THREE.Scene();scene.background=new THREE.Color('#e9eee9');
const camera=new THREE.PerspectiveCamera(34,1,.01,100);
let renderCamera=camera;
const controls=new OrbitControls(camera,renderer.domElement);controls.enableDamping=true;
scene.add(new THREE.HemisphereLight(0xffffff,0x7d9b8a,1.7));
const sun=new THREE.DirectionalLight(0xffffff,2.1);sun.position.set(-3,5,4);scene.add(sun);
const fill=new THREE.DirectionalLight(0xbce8ff,.65);fill.position.set(3,2,-4);scene.add(fill);
const grid=new THREE.GridHelper(5,25,0xb7c9bc,0xd3ded4);grid.position.y=-.29;scene.add(grid);
const loader=new GLTFLoader(),cache=new Map(),rest=new Map();
let model=null,rotor=null,spinning=false,angle=0,explosion=0,wire=false,request=0;
const reducedMotion=matchMedia('(prefers-reduced-motion: reduce)').matches;
const paths={base:'/assets/models/ufo-modular/ufo-modular.glb',
    upgraded:'/assets/models/ufo-modular/ufo-modular-upgraded.glb',
    original:'/assets/models/ufo-body/ufo-body.glb'};
function resize(){const r=stage.getBoundingClientRect();renderer.setSize(r.width,r.height);camera.aspect=r.width/r.height;camera.updateProjectionMatrix();}
new ResizeObserver(resize).observe(stage);
function view(name){
    if(!model)return;
    grid.visible=name!=='bottom';
    const b=new THREE.Box3().setFromObject(model),center=b.getCenter(new THREE.Vector3());
    const dir=new THREE.Vector3(...({three:[1,.70,1.3],top:[0,1,.001],bottom:[0,-1,.001]}[name])).normalize();
    const right=new THREE.Vector3().crossVectors(new THREE.Vector3(0,1,0),dir).normalize();
    const up=new THREE.Vector3().crossVectors(dir,right).normalize();
    const tangent=Math.tan(THREE.MathUtils.degToRad(camera.fov/2));
    let distance=.1;const point=new THREE.Vector3();model.updateMatrixWorld(true);
    model.traverse(n=>{if(!n.isMesh)return;const positions=n.geometry.attributes.position;
        for(let i=0;i<positions.count;i++){
            point.fromBufferAttribute(positions,i).applyMatrix4(n.matrixWorld).sub(center);
            const depth=point.dot(dir);
            distance=Math.max(distance,depth+Math.abs(point.dot(right))*1.13/(tangent*camera.aspect),
                depth+Math.abs(point.dot(up))*1.2/tangent);
        }});
    camera.position.copy(center).addScaledVector(dir,distance);
    camera.up.set(0,1,0);controls.target.copy(center);controls.update();
}
function applyExplode(){
    if(!model)return;
    model.traverse(n=>{const base=rest.get(n);if(!base)return;n.position.copy(base);
        if(n.name==='Dome')n.position.y+=explosion*.35;
        if(n.name==='Dome_Collar')n.position.y+=explosion*.15;
        if(n.name==='Socket_Top')n.position.y+=explosion*.65;
        if(n.name==='Outer_Ring_Pivot')n.position.y-=explosion*.23;
        if(n.name.startsWith('Socket_Rim_Lens'))n.position.y+=explosion*.22;
        if(n.name==='Socket_Beam')n.position.y-=explosion*.40;
        if(n.name==='Socket_Laser_Left')n.position.x-=explosion*.42;
        if(n.name==='Socket_Laser_Right')n.position.x+=explosion*.42;
    });
}
async function load(kind){
    const id=++request;$('#status').textContent='Loading model…';$('#status').className='';
    try{
        let gltf=cache.get(kind);
        if(!gltf){gltf=await loader.loadAsync(paths[kind]);cache.set(kind,gltf);}
        if(id!==request)return;
        if(model)scene.remove(model);
        model=gltf.scene;scene.add(model);rest.clear();
        // Cached scenes may retain an exploded pose. Reset from saved userData.
        model.traverse(n=>{
            n.userData.restPosition??=n.position.toArray();
            n.position.fromArray(n.userData.restPosition);rest.set(n,n.position.clone());
            if(n.isMesh)[].concat(n.material).forEach(m=>m.wireframe=wire);
        });
        rotor=model.getObjectByName('Outer_Ring_Pivot');
        spinning=false;angle=0;$('#spin').setAttribute('aria-pressed','false');
        $('#angle').value=0;$('#degrees').textContent='0°';if(rotor)rotor.rotation.y=0;
        ['#angle','#explode','#spin','#reset'].forEach(s=>$(s).disabled=kind==='original');
        applyExplode();view('three');
        let triangles=0,meshes=0;
        model.traverse(n=>{if(n.isMesh){meshes++;triangles+=(n.geometry.index?.count??n.geometry.attributes.position.count)/3;}});
        $('#stats').textContent=`${triangles.toLocaleString()} triangles · ${meshes} mesh objects`;
        $('#hud strong').textContent=kind==='original'?'Original body':kind==='upgraded'?'Assembled · upgrade kit':'Assembled · base craft';
        $('#status').textContent=kind==='original'?'Original single mesh.':reducedMotion?'Ready. Spin is off; use the rotation slider.':'Ready. Rotate the ring or separate the parts.';
    }catch(e){if(id!==request)return;$('#status').className='error';$('#status').textContent='Model could not load: '+e.message;}
}
$('#configuration').onchange=e=>load(e.target.value);
$('#spin').onclick=()=>{spinning=!spinning;$('#spin').setAttribute('aria-pressed',String(spinning));};
$('#angle').oninput=e=>{angle=THREE.MathUtils.degToRad(+e.target.value);if(rotor)rotor.rotation.y=angle;$('#degrees').textContent=e.target.value+'°';};
$('#explode').oninput=e=>{explosion=+e.target.value;$('#exploded').textContent=Math.round(explosion*100)+'%';applyExplode();};
$('#reset').onclick=()=>{spinning=false;angle=0;explosion=0;if(rotor)rotor.rotation.y=0;
    $('#angle').value=0;$('#explode').value=0;$('#degrees').textContent='0°';$('#exploded').textContent='0%';
    $('#spin').setAttribute('aria-pressed','false');applyExplode();view('three');};
$('#wire').onclick=()=>{wire=!wire;$('#wire').setAttribute('aria-pressed',String(wire));
    model?.traverse(n=>{if(n.isMesh)[].concat(n.material).forEach(m=>m.wireframe=wire);});};
document.querySelectorAll('[data-view]').forEach(b=>b.onclick=()=>view(b.dataset.view));
let previous=0;
renderer.setAnimationLoop(time=>{
    const dt=Math.min((time-previous)/1000,.1);previous=time;
    if(spinning&&rotor){angle=(angle+dt*Math.PI/4)%(Math.PI*2);rotor.rotation.y=angle;
        $('#angle').value=Math.round(THREE.MathUtils.radToDeg(angle));$('#degrees').textContent=$('#angle').value+'°';}
    controls.update();renderer.render(scene,renderCamera);
});
resize();await load('base');
// Orthographic capture mode registers one screen pixel to the planar UV map.
// At a square viewport, the outer radius is 1 / 2.04 of the texture width.
const template=new URL(location.href).searchParams.get('template');
if(['ring','dome','full'].includes(template)){
    document.body.classList.add('texture-template');
    const style=document.createElement('style');
    style.textContent='body.texture-template header,body.texture-template aside,body.texture-template #hud,body.texture-template #view-controls{display:none}body.texture-template main{display:block;height:100dvh;min-height:0;padding:0}body.texture-template #stage{width:100dvw;height:100dvh;min-height:0}';
    document.head.append(style);grid.visible=false;controls.enabled=false;
    renderer.toneMapping=THREE.NoToneMapping;renderer.setPixelRatio(1);
    scene.background=new THREE.Color('#d9dedb');
    const target=model.getObjectByName(template==='dome'?'Dome':'Outer_Ring');
    const span=new THREE.Box3().setFromObject(target).getSize(new THREE.Vector3());
    const extent=Math.max(span.x,span.z)*.51;
    renderCamera=new THREE.OrthographicCamera(-extent,extent,extent,-extent,.01,10);
    renderCamera.position.set(0,5,0);renderCamera.up.set(0,0,-1);renderCamera.lookAt(0,0,0);
    model.traverse(n=>{
        if(!n.isMesh)return;
        let p=n;while(p&&p!==target)p=p.parent;
        if(template!=='full')n.visible=p===target;
        n.material=template!=='full'?new THREE.MeshBasicMaterial({color:'#29699f'}):
            new THREE.MeshBasicMaterial({map:n.material.map,color:n.material.color});
    });
    resize();
}
