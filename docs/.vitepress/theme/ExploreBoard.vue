<script setup>
import {ref,watch,onMounted,onUnmounted,computed} from 'vue';
import {withBase} from 'vitepress';
import {hardware} from './hardware-map.mjs';
import {headerPinPosition,headerPinNumberAt} from './header-pins.mjs';
import {illustratedMaterial,studioExposure} from './board-materials.mjs';
const props=defineProps({selection:String,running:Boolean,flow:Boolean,color:String,pin:String,ledLevel:Number});
const emit=defineEmits(['select']);
const host=ref(),failed=ref(false),ready=ref(false),hover=ref(''),path=ref(''),pinGuide=ref(null),zoomLevel=ref(1);
const choices=computed(()=>hardware);

let dispose=()=>{},invalidate=()=>{},reset=()=>{},flip=()=>{},setZoom=()=>{};
watch(()=>[props.selection,props.running,props.flow,props.color,props.pin,props.ledLevel],()=>invalidate());
let dead=false;
onMounted(async()=>{
 try{
  const [T,{GLTFLoader},{MeshoptDecoder},{RoomEnvironment}]=await Promise.all([import('three'),import('three/addons/loaders/GLTFLoader.js'),import('meshoptimizer'),import('three/addons/environments/RoomEnvironment.js')]);
  if(dead)return;
  const [{EffectComposer},{RenderPass},{GTAOPass},{OutputPass}]=await Promise.all([import('three/addons/postprocessing/EffectComposer.js'),import('three/addons/postprocessing/RenderPass.js'),import('three/addons/postprocessing/GTAOPass.js'),import('three/addons/postprocessing/OutputPass.js')]);
  if(dead)return;
  const scene=new T.Scene(),camera=new T.OrthographicCamera(-48,48,38,-38,.1,500);
  camera.position.set(0,0,150);camera.lookAt(0,0,0);
  const renderer=new T.WebGLRenderer({alpha:true,antialias:true,powerPreference:'high-performance'});
  renderer.setPixelRatio(Math.min(devicePixelRatio,1.5));renderer.toneMapping=T.ACESFilmicToneMapping;renderer.toneMappingExposure=studioExposure;renderer.shadowMap.enabled=true;renderer.shadowMap.type=T.PCFShadowMap;
  host.value.appendChild(renderer.domElement);renderer.domElement.setAttribute('aria-label','拖动旋转板卡，点击元件查看引脚');
  const pmrem=new T.PMREMGenerator(renderer),room=new RoomEnvironment(),env=pmrem.fromScene(room,.05);scene.environment=env.texture;room.dispose();pmrem.dispose();
  scene.add(new T.HemisphereLight(0xfffaf1,0x607080,.8));
  for(const [x,y,z,p] of [[-60,80,100,2],[60,-20,50,.7]]){const l=new T.DirectionalLight(0xfff7e8,p);l.position.set(x,y,z);if(x<0){l.castShadow=true;l.shadow.mapSize.set(2048,2048);Object.assign(l.shadow.camera,{left:-65,right:65,top:65,bottom:-65,near:1,far:250});l.shadow.bias=-.0003;l.shadow.normalBias=.08;l.shadow.radius=3}scene.add(l)}
  const composer=new EffectComposer(renderer);composer.addPass(new RenderPass(scene,camera));
  const ao=new GTAOPass(scene,camera,512,512);ao.updateGtaoMaterial({radius:2.2,thickness:1.4,distanceExponent:1.5,samples:8,screenSpaceRadius:false});ao.blendIntensity=.85;composer.addPass(ao);const outputPass=new OutputPass();composer.addPass(outputPass);
  scene.environmentRotation.set(.25,0,.5);
  const board=new T.Group();scene.add(board);
  const center=new T.Vector3(0,.15734,1.74673),pickers=[];
  const location=h=>new T.Vector3((h.position[0]/100-.5)*41,(.5-h.position[1]/100)*60,(h.kind==='xt30'?4.8:h.position[2])).sub(center);
  const ledPosition=new T.Vector3(18.161,-22.733,.84).sub(center);
  const ledLight=new T.PointLight(0xffffff,0,3.5,2);ledLight.position.copy(ledPosition).add(new T.Vector3(0,0,.2));board.add(ledLight);

  // Seat a shallow resin lens within the existing housing footprint.
  const shellShape=new T.Shape();shellShape.moveTo(-1.2,-1.3);shellShape.lineTo(1.2,-1.3);shellShape.quadraticCurveTo(1.3,-1.3,1.3,-1.2);shellShape.lineTo(1.3,1.2);shellShape.quadraticCurveTo(1.3,1.3,1.2,1.3);shellShape.lineTo(-1.2,1.3);shellShape.quadraticCurveTo(-1.3,1.3,-1.3,1.2);shellShape.lineTo(-1.3,-1.2);shellShape.quadraticCurveTo(-1.3,-1.3,-1.2,-1.3);const opening=new T.Path();opening.moveTo(-.85,-.85);opening.lineTo(-.85,.85);opening.lineTo(.85,.85);opening.lineTo(.85,-.85);opening.closePath();shellShape.holes.push(opening);
  const ledHousing=new T.Mesh(new T.ExtrudeGeometry(shellShape,{depth:.85,bevelEnabled:true,bevelSize:.045,bevelThickness:.045,bevelSegments:3,steps:1}),new T.MeshPhysicalMaterial({color:'#fff5df',roughness:.45,metalness:0}));ledHousing.position.copy(new T.Vector3(18.161,-22.733,.02).sub(center));board.add(ledHousing);
  const lensMaterial=new T.MeshPhysicalMaterial({color:'#d8d5ad',roughness:.32,metalness:0,clearcoat:.3,emissive:0x000000});
  const lens=new T.Mesh(new T.SphereGeometry(.83,24,12),lensMaterial);
  lens.scale.z=.12;lens.position.copy(new T.Vector3(18.161,-22.733,1.0).sub(center));board.add(lens);
  const selectionUniforms={selectionId:{value:0},selectedPinNumber:{value:0},headerMode:{value:0},ledPartId:{value:hardware.findIndex(p=>p.id==='RGB')+1},ledColor:{value:new T.Color()},ledStrength:{value:0}};
  const applySelection=material=>{const original=material.onBeforeCompile.bind(material),cacheKey=material.customProgramCacheKey();material.onBeforeCompile=shader=>{original(shader);Object.assign(shader.uniforms,selectionUniforms);shader.vertexShader='attribute float partId; attribute float pinNumber;attribute vec3 ledSurface;varying vec3 vLedSurface; varying float vPartId; varying float vPinNumber;\n'+shader.vertexShader;shader.vertexShader=shader.vertexShader.replace('#include <begin_vertex>','#include <begin_vertex>\nvPartId=partId;vPinNumber=pinNumber;vLedSurface=ledSurface;');shader.fragmentShader='uniform float ledPartId;uniform vec3 ledColor;uniform float ledStrength;varying vec3 vLedSurface;uniform float selectionId;uniform float selectedPinNumber;uniform float headerMode; varying float vPartId;varying float vPinNumber;\n'+shader.fragmentShader;shader.fragmentShader=shader.fragmentShader.replace('#include <color_fragment>','#include <color_fragment>\nvec2 lensD=abs(vLedSurface.xy)-vec2(0.57);float ledWindow=(1.0-smoothstep(0.09,0.14,length(max(lensD,vec2(0.0)))))*step(0.01,vLedSurface.z)*(1.0-step(0.1,abs(vPartId-ledPartId)));\ndiffuseColor.rgb=mix(diffuseColor.rgb,vec3(0.22,0.25,0.21),ledWindow*0.82);');shader.fragmentShader=shader.fragmentShader.replace('#include <emissivemap_fragment>','#include <emissivemap_fragment>\ntotalEmissiveRadiance+=ledColor*ledStrength*ledWindow;');shader.fragmentShader=shader.fragmentShader.replace('#include <opaque_fragment>','float selectedSurface=step(0.5,selectionId)*(1.0-step(0.1,abs(vPartId-selectionId)));\nif(headerMode>0.5)selectedSurface=step(0.5,selectedPinNumber)*(1.0-step(0.1,abs(vPinNumber-selectedPinNumber)));\noutgoingLight=mix(outgoingLight, outgoingLight*vec3(0.35,0.8,1.15)+vec3(0.015,0.24,0.38),selectedSurface*0.68*(1.0-ledWindow));\n#include <opaque_fragment>');};material.customProgramCacheKey=()=>cacheKey+'-component-selection-led-v5';return material;};
  let raf=0,w=1,h=1;
  const point=v=>{const q=board.localToWorld(v.clone()).project(camera);return [(q.x*.5+.5)*w,(-q.y*.5+.5)*h]};
  const front=()=>new T.Vector3(0,0,1).applyQuaternion(board.quaternion).z>0;
  const render=()=>{const renderStart=performance.now();raf=0;if(dead||document.hidden)return;board.updateMatrixWorld(true);const f=front();
   const active=hardware.find(p=>p.id===(props.selection==='EX2'?props.selection:(hover.value||props.selection))&&(p.side==='front')===f);
   selectionUniforms.selectionId.value=active?hardware.indexOf(active)+1:0;
   ledHousing.material.color.set(active?.id==='RGB'?'#65c4ee':'#fff5df');
   const headerPin=hardware.find(p=>p.id==='EX2').pins.find(p=>p.n===props.pin);
   selectionUniforms.headerMode.value=active?.id==='EX2'&&headerPin?1:0;
   selectionUniforms.selectedPinNumber.value=headerPin?(headerPin.row-1)*3+headerPin.col+1:0;
   pinGuide.value=null;
   if(props.selection==='EX2'&&headerPin&&f){const pinPoint=point(new T.Vector3(...headerPinPosition(headerPin)).sub(center));pinGuide.value={pin:pinPoint,label:`第 ${headerPin.row} 行 · ${['左','中','右'][headerPin.col]}列 · ${headerPin.net}`,labelX:Math.max(8,Math.min(w-210,pinPoint[0]+24)),labelY:Math.max(30,Math.min(h-34,pinPoint[1]+40))};}
   const selected=hardware.find(x=>x.id===props.selection),cpu=hardware.find(x=>x.id==='CPU');path.value='';
   if(props.flow&&selected&&['EX1','IMU','RGB'].includes(selected.id)&&(selected.id!=='EX1'||selected.pins.find(p=>p.n===props.pin)?.gpio!=null)&&f){const a=point(location(cpu)),b=point(selected.id==='RGB'?ledPosition:location(selected));if(selected.id==='RGB'){const dx=a[0]-b[0],dy=a[1]-b[1],distance=Math.hypot(dx,dy)||1;b[0]+=dx/distance*12;b[1]+=dy/distance*12}path.value=`M ${a[0]} ${a[1]} C ${a[0]} ${b[1]}, ${b[0]} ${a[1]}, ${b[0]} ${b[1]}`}
   const ledColor=selectionUniforms.ledColor.value;ledColor.set(props.color||'#000000');const maximum=Math.max(ledColor.r,ledColor.g,ledColor.b);const level=props.running&&props.selection==='RGB'&&maximum>0?Math.max(0,Math.min(1,(props.ledLevel||0)/255)):0;if(maximum>0)ledColor.multiplyScalar(1/maximum);selectionUniforms.ledStrength.value=level*5;ledLight.color.copy(ledColor);ledLight.intensity=level*1.8;lensMaterial.emissive.copy(ledColor);lensMaterial.color.copy(level>0?ledColor:new T.Color('#bdb681'));lensMaterial.emissiveIntensity=level*1.2;composer.render();
   if(host.value){host.value.dataset.ledStrength=String(level);host.value.dataset.renderMode='webgl';host.value.dataset.side=f?'front':'back';host.value.dataset.drawCalls=renderer.info.render.calls;host.value.dataset.renderMs=(performance.now()-renderStart).toFixed(2);}
  };
  invalidate=()=>{if(!raf&&!dead)raf=requestAnimationFrame(render)};
  const resize=()=>{if(!host.value)return;w=host.value.clientWidth;h=host.value.clientHeight;if(!w||!h)return;renderer.setSize(w,h);composer.setSize(w,h);const aspect=w/h,span=Math.max(31,39/aspect,h*31/550);camera.left=-span*aspect;camera.right=span*aspect;camera.top=span;camera.bottom=-span;camera.updateProjectionMatrix();invalidate()};
  setZoom=value=>{zoomLevel.value=Math.max(.8,Math.min(3.5,value));camera.zoom=zoomLevel.value;camera.updateProjectionMatrix();invalidate()};
  const wheel=e=>{e.preventDefault();setZoom(zoomLevel.value*Math.exp(-e.deltaY*.0015))};
  renderer.domElement.addEventListener('wheel',wheel,{passive:false});
  const ray=new T.Raycaster(),mouse=new T.Vector2();
  const hit=e=>{const r=renderer.domElement.getBoundingClientRect();mouse.set((e.clientX-r.left)/r.width*2-1,-(e.clientY-r.top)/r.height*2+1);ray.setFromCamera(mouse,camera);board.updateMatrixWorld(true);const targets=pickers.filter(p=>(p.userData.side==='front')===front());for(const p of targets)p.matrixWorld.copy(p.userData.source.matrixWorld);const actual=ray.intersectObjects(targets,false)[0]?.object.userData.id;if(actual)return actual;if(front()){const p=point(ledPosition);if(Math.hypot(e.clientX-r.left-p[0],e.clientY-r.top-p[1])<=10)return 'RGB'}return ''};

  let down=null,pinchDistance=0;const touches=new Map();
  const start=e=>{if(e.button!==0&&e.button!==2)return;if(e.pointerType==='touch'){touches.set(e.pointerId,[e.clientX,e.clientY]);if(touches.size===2){const [a,b]=[...touches.values()];pinchDistance=Math.hypot(a[0]-b[0],a[1]-b[1]);down=null;renderer.domElement.setPointerCapture(e.pointerId);return}}down={x:e.clientX,y:e.clientY,px:e.clientX,py:e.clientY,moved:false,pan:e.shiftKey||e.button===2};renderer.domElement.setPointerCapture(e.pointerId)};
  const move=e=>{if(touches.has(e.pointerId)){touches.set(e.pointerId,[e.clientX,e.clientY]);if(touches.size===2){const [a,b]=[...touches.values()],distance=Math.hypot(a[0]-b[0],a[1]-b[1]);if(pinchDistance>0)setZoom(zoomLevel.value*distance/pinchDistance);pinchDistance=distance;return}}if(down){const dx=e.clientX-down.px,dy=e.clientY-down.py;down.moved ||=Math.hypot(e.clientX-down.x,e.clientY-down.y)>5;if(down.moved){if(down.pan){board.position.x+=dx*(camera.right-camera.left)/camera.zoom/w;board.position.y-=dy*(camera.top-camera.bottom)/camera.zoom/h;}else{board.quaternion.premultiply(new T.Quaternion().setFromAxisAngle(new T.Vector3(0,1,0),dx*.008));board.quaternion.premultiply(new T.Quaternion().setFromAxisAngle(new T.Vector3(1,0,0),dy*.008));}hover.value='';}down.px=e.clientX;down.py=e.clientY;}else hover.value=hit(e);renderer.domElement.style.cursor=down?'grabbing':hover.value?'pointer':'grab';invalidate()};
  const end=e=>{touches.delete(e.pointerId);if(touches.size<2)pinchDistance=0;if(down&&!down.moved&&!down.pan){const id=hit(e);if(id)emit('select',id)}down=null;invalidate()};
  const leave=()=>{if(!down){hover.value='';invalidate()}};
  renderer.domElement.addEventListener('contextmenu',e=>e.preventDefault());renderer.domElement.addEventListener('pointerdown',start);renderer.domElement.addEventListener('pointermove',move);renderer.domElement.addEventListener('pointerup',end);renderer.domElement.addEventListener('pointercancel',e=>{touches.delete(e.pointerId);pinchDistance=0;down=null});renderer.domElement.addEventListener('pointerleave',leave);
  reset=()=>{setZoom(1);board.position.set(0,0,0);const view=new T.PerspectiveCamera();view.up.set(0,0,1);view.position.set(100,-45,140);view.lookAt(0,0,0);board.quaternion.copy(view.quaternion).invert();invalidate()};
  flip=()=>{board.quaternion.premultiply(new T.Quaternion().setFromAxisAngle(new T.Vector3(0,1,0),Math.PI));invalidate()};
  const ro=new ResizeObserver(resize);ro.observe(host.value);const visibility=()=>invalidate();document.addEventListener('visibilitychange',visibility);
  dispose=()=>{cancelAnimationFrame(raf);ro.disconnect();document.removeEventListener('visibilitychange',visibility);pickers.forEach(p=>{p.geometry.dispose();p.material.dispose()});scene.traverse(o=>{o.geometry?.dispose();for(const m of o.material?(Array.isArray(o.material)?o.material:[o.material]):[]){m.userData.shieldTexture?.dispose();m.dispose()}});env.dispose();ao.dispose();outputPass.dispose();composer.dispose();renderer.dispose();renderer.domElement.remove()};
  const [gltf,map,maskBuffer]=await Promise.all([new GLTFLoader().setMeshoptDecoder(MeshoptDecoder).loadAsync(withBase('/hardware/osrcore-styled.glb')),fetch(withBase('/hardware/selection-map.json')).then(r=>{if(!r.ok)throw Error('Selection map unavailable');return r.json()}),fetch(withBase('/hardware/selection-map.bin')).then(r=>{if(!r.ok)throw Error('Selection mask unavailable');return r.arrayBuffer()})]);if(dead)return;
  let meshIndex=0;
  gltf.scene.traverse(o=>{if(!o.isMesh)return;const info=map.meshes[meshIndex++],geometry=o.geometry,pos=geometry.attributes.position;if(info.count!==pos.count||info.material!==o.material.name)throw Error('Selection map/model mismatch');const ids=new Uint8Array(maskBuffer,info.offset,info.count);geometry.setAttribute('partId',new T.Uint8BufferAttribute(ids,1));const indices=geometry.index,sets=new Map();for(let i=0;i<(indices?indices.count:pos.count);i+=3){const a=indices?indices.getX(i):i,id=ids[a];if(!id)continue;if(!sets.has(id))sets.set(id,[]);for(let j=0;j<3;j++)sets.get(id).push(indices?indices.getX(i+j):i+j)}for(const [id,vertices] of sets){const geo=new T.BufferGeometry();geo.setAttribute('position',pos);geo.setIndex(vertices);const box=new T.Box3(),v=new T.Vector3();for(const i of vertices)box.expandByPoint(v.fromBufferAttribute(pos,i));geo.boundingBox=box;geo.boundingSphere=box.getBoundingSphere(new T.Sphere());const picker=new T.Mesh(geo,new T.MeshBasicMaterial({side:T.DoubleSide}));picker.matrixAutoUpdate=false;picker.userData={id:map.ids[id-1],side:hardware[id-1].side,source:o};pickers.push(picker)}});
  gltf.scene.scale.setScalar(1000);gltf.scene.position.sub(center);
  gltf.scene.traverse(o=>{if(!o.isMesh)return;o.castShadow=true;o.receiveShadow=true;o.material=Array.isArray(o.material)?o.material.map(m=>applySelection(illustratedMaterial(m,T))):applySelection(illustratedMaterial(o.material,T))});
  board.add(gltf.scene);scene.updateMatrixWorld(true);
  gltf.scene.traverse(o=>{if(!o.isMesh)return;const pos=o.geometry.attributes.position,ids=o.geometry.attributes.partId,values=new Float32Array(pos.count),v=new T.Vector3();for(let i=0;i<pos.count;i++){if(map.ids[ids.getX(i)-1]!=='EX2'||o.material.name!=='mtl3')continue;v.fromBufferAttribute(pos,i);o.localToWorld(v);v.add(center);values[i]=headerPinNumberAt(v.x,v.y,v.z)}o.geometry.setAttribute('pinNumber',new T.Float32BufferAttribute(values,1))});

  gltf.scene.traverse(o=>{if(!o.isMesh)return;const mats=Array.isArray(o.material)?o.material:[o.material];if(!mats.some(m=>m.name==='mtl3'))return;const pos=o.geometry.getAttribute('position'),colors=new Float32Array(pos.count*3),v=new T.Vector3(),cream=new T.Color('#f0e0bd'),gold=new T.Color('#dfb34f');for(let i=0;i<pos.count;i++){v.fromBufferAttribute(pos,i);o.localToWorld(v);const c=v.x>12&&v.y<3&&v.y>-18&&v.z>2?gold:cream;c.toArray(colors,i*3)}o.geometry.setAttribute('color',new T.BufferAttribute(colors,3));for(const m of mats)if(m.name==='mtl3'){m.color.set('#ffffff');m.vertexColors=true;m.needsUpdate=true}});
  gltf.scene.traverse(o=>{if(!o.isMesh)return;const p=o.geometry.attributes.position,ids=o.geometry.attributes.partId,values=new Float32Array(p.count*3),v=new T.Vector3();for(let i=0;i<p.count;i++){if(map.ids[ids.getX(i)-1]!=='RGB')continue;v.fromBufferAttribute(p,i);o.localToWorld(v);v.add(center);values[i*3]=v.x-18.161;values[i*3+1]=v.y+22.733;values[i*3+2]=v.z}o.geometry.setAttribute('ledSurface',new T.Float32BufferAttribute(values,3))});
  reset();resize();ready.value=true;
 }catch(e){console.error("Board preview failed",e);failed.value=true;dispose()}
});
onUnmounted(()=>{dead=true;dispose()});
</script>
<template>
 <section class="explore-board">

  <div class="explore-stage">
   <div ref="host" class="explore-canvas" v-show="!failed"></div>
   <div v-if="!ready" class="explore-fallback" role="status"><span>{{failed?'三维暂不可用，请通过下方选择器查看元件':'正在加载三维板卡…'}}</span></div>
   <svg v-if="ready&&pinGuide" class="pin-guide" aria-label="所选排针位置">
    <circle :cx="pinGuide.pin[0]" :cy="pinGuide.pin[1]" r="6" class="selected-pin"/>
    <path :d="`M${pinGuide.pin[0]} ${pinGuide.pin[1]}L${pinGuide.labelX+8} ${pinGuide.labelY}`" class="pin-leader"/>
   </svg>
   <div v-if="ready&&pinGuide" class="pin-location-label" :style="{left:pinGuide.labelX+'px',top:pinGuide.labelY+'px'}">{{pinGuide.label}}</div>
   <svg v-if="ready&&path" class="board-signal" :class="{running}" aria-label="逻辑信号路径"><path :d="path"/><path :d="path" class="signal-dashes"/></svg>

  </div>
  <div class="explore-controls"><span title="Shift + 拖动或右键拖动可平移查看放大后的细节">{{failed?'此浏览器暂不支持三维，仍可选择元件':'拖动旋转 · 滚轮缩放'}}</span><div v-if="ready" class="zoom-controls"><button aria-label="缩小板卡" :disabled="zoomLevel<=.8" @click="setZoom(zoomLevel/1.25)">−</button><output aria-label="板卡缩放比例">{{Math.round(zoomLevel*100)}}%</output><button aria-label="放大板卡" :disabled="zoomLevel>=3.5" @click="setZoom(zoomLevel*1.25)">+</button></div><button v-if="ready" @click="flip()">翻转 ↔</button><button v-if="ready" @click="reset()">复位视角</button><label class="sr-only" for="hardware-select">选择板卡元件</label><select id="hardware-select" :value="selection" @change="emit('select',$event.target.value)"><option v-for="h in choices" :value="h.id" :key="h.id">{{h.name}}</option></select></div>
 </section>
</template>
<style scoped>
.explore-board{display:flex;flex-direction:column;width:100%;height:100%;min-height:0}.functional-filters{display:flex;flex-wrap:wrap;gap:5px}.functional-filters button{font-size:12px;padding:7px 12px;border-radius:20px;color:#657581}.functional-filters button[aria-pressed=true]{background:#e7f3fd;color:#0076cf}.explore-stage{position:relative;flex:1;min-height:240px;isolation:isolate}.explore-canvas{position:absolute;inset:0}.explore-canvas :deep(canvas){width:100%;height:100%;touch-action:none;cursor:grab;filter:drop-shadow(0 15px 16px #142a3822)}.explore-fallback{position:absolute;inset:0;display:grid;place-items:center;container-type:size}.fallback-art{position:relative;width:min(100cqw,calc(100cqh * 1.635066));aspect-ratio:1604/981}.fallback-art img{width:100%;height:100%}.explore-fallback img{max-width:100%;max-height:100%;object-fit:contain}.explore-fallback button{position:absolute;transform:translate(-50%,-50%);font-size:11px;padding:8px;background:#ffffffde;border:1px solid #c5d8e3;border-radius:5px}.explore-controls{display:flex;align-items:center;gap:14px;font-size:11px;color:#70818b;flex-wrap:wrap;padding:10px 0}.explore-controls button{color:#0076cf}.explore-controls select{margin-left:auto;max-width:180px;background:transparent;border-bottom:1px solid #cbd7df;font-size:12px;padding:4px}.hover-caption{position:absolute;bottom:16px;left:50%;transform:translateX(-50%);padding:7px 13px;background:#fff;box-shadow:0 3px 16px #18364718;border:1px solid #e2e9ee;border-radius:6px;font-size:12px;pointer-events:none}.board-signal{position:absolute;inset:0;width:100%;height:100%;pointer-events:none;overflow:visible}.board-signal path{fill:none;stroke:#fff;stroke-width:5;opacity:.85}.board-signal .signal-dashes{stroke:#008fd5;stroke-width:2;stroke-dasharray:7 7}.board-signal.running .signal-dashes{animation:flow .8s linear infinite}@keyframes flow{to{stroke-dashoffset:-28}}.pin-guide{position:absolute;inset:0;width:100%;height:100%;pointer-events:none;overflow:visible}.pin-guide .selected-pin{fill:#00a8ed66;stroke:#007cc4;stroke-width:2;filter:drop-shadow(0 0 2px white)}.pin-guide .pin-leader{fill:none;stroke:#0082c9;stroke-width:1.5}.pin-location-label{position:absolute;pointer-events:none;white-space:nowrap;padding:5px 9px;background:#fff;border:1px solid #8fc8e6;border-radius:5px;color:#006fae;font-size:11px;box-shadow:0 2px 6px #12345615}.zoom-controls{display:flex;align-items:center;gap:7px;border:1px solid #dce5eb;border-radius:5px}.zoom-controls button{min-height:24px;width:24px;font-size:17px}.zoom-controls output{font-size:10px;min-width:30px;text-align:center}.zoom-controls button:disabled{opacity:.35}.sr-only{position:absolute;width:1px;height:1px;overflow:hidden;clip-path:inset(50%)}@media(max-width:850px){.explore-stage{height:350px;flex:none}}@media(prefers-reduced-motion:reduce){.board-signal.running .signal-dashes{animation:none}}
</style>
