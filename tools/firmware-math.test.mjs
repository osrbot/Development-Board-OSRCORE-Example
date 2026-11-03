import test from 'node:test';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
test('real C PID limits and AHRS stationary/rotation math',async()=>{
 const dir=await fs.mkdtemp(path.join(os.tmpdir(),'osrcore-math-'));
 try{
  await fs.writeFile(path.join(dir,'esp_timer.h'),'#include <stdint.h>\nint64_t esp_timer_get_time(void);\n');
  await fs.writeFile(path.join(dir,'test.c'),`
#include <assert.h>
#include <math.h>
#include "pid.h"
#include "madgwick.h"
static int64_t clock_us;
int64_t esp_timer_get_time(void){return clock_us;}
int main(void){
 pid_ctrl_t p;pid_init(&p,447,4.7,47,1000,.05);
 assert(pid_update(&p,0,0,1500,.02)==1500);
 for(int i=0;i<10000;i++){int pulse=pid_update(&p,6,0,1500,.02);assert(pulse>=1000&&pulse<=2000);}
 assert(p.integral<=1000);pid_reset(&p);assert(p.integral==0&&p.last_error==0);
 assert(pid_update(&p,1,0,1500,0)==1500);
 madgwick_t m;madgwick_init(&m,.1);
 for(int i=0;i<2000;i++){clock_us+=5000;madgwick_update_imu(&m,0,0,0,0,0,1);}
 float r,pitch,y;madgwick_get_euler(&m,&r,&pitch,&y);
 assert(isfinite(r)&&isfinite(pitch)&&isfinite(y));assert(fabsf(r)<.01&&fabsf(pitch)<.01);
 for(int i=0;i<200;i++){clock_us+=5000;madgwick_update_imu(&m,0,0,90,0,0,1);}
 madgwick_get_euler(&m,&r,&pitch,&y);assert(fabsf(y-90)<.2);
 assert(fabsf(m.q0*m.q0+m.q1*m.q1+m.q2*m.q2+m.q3*m.q3-1)<.001);
 return 0;
}
`);
  const binary=path.join(dir,'math-test');
  execFileSync('cc',['-O2','-I'+dir,'-I'+path.join(root,'09_pid_motor/main'),'-I'+path.join(root,'12_full_example/main'),path.join(dir,'test.c'),path.join(root,'09_pid_motor/main/pid.c'),path.join(root,'12_full_example/main/madgwick.c'),'-lm','-o',binary]);
  execFileSync(binary);
 }finally{await fs.rm(dir,{recursive:true,force:true});}
});
