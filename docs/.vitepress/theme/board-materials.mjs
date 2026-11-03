// One material palette for the full board and isolated component views.
export function styleBoardMaterial(material) {
 const id=parseInt(material.name.replace('mtl',''),10);
 material.roughness=.48;material.metalness=.12;material.envMapIntensity=1.05;
 if([2,6,16,17,29,34].includes(id)) {material.color.set('#d8a33c');material.metalness=.72;material.roughness=.29;}
 else if([3,8,11,31,33].includes(id)) {material.color.set('#eee1c9');material.metalness=0;material.roughness=.36;}
 else if([23,26].includes(id)) {material.color.set('#1b2924');material.metalness=.12;material.roughness=.5;}
 else if([4,20,21,24,28,32,36,39].includes(id)) {material.metalness=.88;material.roughness=.28;}
 return material;
}
export const studioExposure=.85;

export function illustratedMaterial(source,T) {
 const material=new T.MeshPhysicalMaterial();
 T.MeshStandardMaterial.prototype.copy.call(material,source);
 styleBoardMaterial(material);
 const id=parseInt(material.name.replace('mtl',''),10);
 material.envMapIntensity=.85;
 if([3,8,11,31,33].includes(id)){material.color.set('#ddc59e');material.roughness=.3;material.clearcoat=.24;material.clearcoatRoughness=.32;}
 else if([2,6,16,17,29,34].includes(id)){material.color.set('#e7b85c');material.metalness=.82;material.roughness=.23;}
 else if([4,20,21,24,28,32,36,39].includes(id)){material.color.set('#bbc1c6');material.metalness=.9;material.roughness=.28;material.anisotropy=0;material.envMapIntensity=1.0;}
 else {if([1,5,7,9,12,15,18,22,53].includes(id)){material.color.set('#15181d');material.metalness=0;material.specularIntensity=.55;}if(id===52){material.color.set('#bb9145');material.metalness=.6;}if(id===53)material.color.set('#181b20');material.clearcoat=.18;material.clearcoatRoughness=.4;material.roughness=.45;}
 if(id===28){
  // Laser-etched identity lives on the actual CAD shield, so it rotates and
  // receives light with the module rather than floating over the board.
  const canvas=document.createElement('canvas');canvas.width=1024;canvas.height=1024;
  const ctx=canvas.getContext('2d');ctx.fillStyle='#26343d';ctx.strokeStyle='#26343d';
  ctx.textBaseline='top';ctx.font='600 112px monospace';ctx.fillText('ESP32-S3',90,255);
  ctx.font='500 63px monospace';ctx.fillText('WROOM-1U',95,395);
  ctx.lineWidth=3;ctx.beginPath();ctx.moveTo(95,505);ctx.lineTo(860,505);ctx.stroke();
  ctx.font='36px monospace';ctx.fillText('Wi-Fi / Bluetooth',95,545);
  ctx.font='600 37px monospace';ctx.fillText('OSRCORE',95,740);
  ctx.lineWidth=6;
  for(let r=20;r<=80;r+=20){ctx.beginPath();ctx.arc(785,775,r,Math.PI,Math.PI*1.5);ctx.stroke();}
  const ink=new T.CanvasTexture(canvas);ink.colorSpace=T.SRGBColorSpace;ink.anisotropy=4;
  material.userData.shieldTexture=ink;material.color.set('#d3d9df');material.roughness=.32;material.metalness=.82;material.envMapIntensity=1.25;
  material.onBeforeCompile=shader=>{
   shader.uniforms.shieldInk={value:ink};
   shader.vertexShader='varying vec3 vBoardSurface;\n'+shader.vertexShader;
   shader.vertexShader=shader.vertexShader.replace('#include <begin_vertex>','#include <begin_vertex>\nvBoardSurface=position;');
   shader.fragmentShader='varying vec3 vBoardSurface;uniform sampler2D shieldInk;\n'+shader.fragmentShader;
   shader.fragmentShader=shader.fragmentShader.replace('#include <color_fragment>',`#include <color_fragment>
    vec2 shieldUv=(vBoardSurface.xy-vec2(-0.2264168,-0.1112095))/vec2(0.5191809,0.5805231);
    vec4 engraving=texture2D(shieldInk,shieldUv);
    float shieldTop=step(0.047,vBoardSurface.z);
    float satinBand=0.88+0.12*smoothstep(-0.3,0.7,shieldUv.x+shieldUv.y*0.45);
    diffuseColor.rgb*=mix(1.0,satinBand,shieldTop);
    diffuseColor.rgb=mix(diffuseColor.rgb,engraving.rgb,engraving.a*shieldTop*0.85);
   `);
   shader.fragmentShader=shader.fragmentShader.replace('#include <roughnessmap_fragment>',`#include <roughnessmap_fragment>
    float brushedPhase=vBoardSurface.y*3800.0;
    float brushedVisibility=1.0-smoothstep(0.8,3.0,fwidth(brushedPhase));
    roughnessFactor*=1.0+0.12*sin(brushedPhase)*brushedVisibility;
    roughnessFactor=mix(roughnessFactor,0.65,engraving.a*shieldTop);
   `);
  };
  material.customProgramCacheKey=()=> 'osrcore-etched-shield-v2';
 }

 return material;
}
