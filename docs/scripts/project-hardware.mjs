import {OrthographicCamera,Vector3} from 'three';
import {hardware,headerPins} from '../.vitepress/theme/hardware-map.mjs';
import {writeFileSync} from 'node:fs';
const center=new Vector3(0,.15734291076660156,1.7467324733734135),out={};
for(const view of ['angle','front','back']){
 const c=view==='angle'?new OrthographicCamera(-45.8181818,45.8181818,28,-28,.1,500):new OrthographicCamera(-40.5,40.5,36,-36,.1,500);c.up.set(...(view==='angle'?[0,0,1]:[0,1,0]));c.position.set(...(view==='front'?[0,0,130]:view==='back'?[0,0,-130]:[100,-45,140]));c.lookAt(0,0,0);c.updateMatrixWorld();
 out[view]=Object.fromEntries(hardware.filter(h=>h.side===(view==='back'?'back':'front')).map(h=>{const [x,y,z]=h.position;const v=new Vector3((x/100-.5)*41,(.5-y/100)*60,z).sub(center).project(c);return [h.id,[+(v.x*.5+.5).toFixed(5),+(-v.y*.5+.5).toFixed(5)]]}));
 if(view!=='back')for(const p of headerPins){const x=[83.7,89.9,96.1][p.col],y=47.5+(p.row-1)*4.2;const v=new Vector3((x/100-.5)*41,(.5-y/100)*60,6).sub(center).project(c);out[view]['EX2-'+p.n]=[+(v.x*.5+.5).toFixed(5),+(-v.y*.5+.5).toFixed(5)];}
}
writeFileSync(new URL('../.vitepress/theme/hardware-projection.json',import.meta.url),JSON.stringify(out,null,2)+'\n');
