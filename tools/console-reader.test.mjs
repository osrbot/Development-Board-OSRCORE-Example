import test from 'node:test';
import {mkdtemp,writeFile,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
test('firmware console assembles USB fragments and discards entire oversized lines',async()=>{
 const dir=await mkdtemp(path.join(tmpdir(),'osrcore-console-'));
 try {
  await writeFile(path.join(dir,'test.c'),String.raw`
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "osrcore_console.h"
int main(void) {
 int fd[2]; assert(pipe(fd)==0);
 assert(fcntl(fd[0],F_SETFL,O_NONBLOCK)==0);
 FILE *input=fdopen(fd[0],"r"); assert(input);
 char line[384]; osrcore_console_reader_t reader={0};
 const char *request="osrcore params {\"id\":\"hardware-regression\",\"lesson\":\"02_buzzer\",\"values\":{\"frequency\":660,\"duration\":150},\"save\":false}";
 for(size_t chunk=1;chunk<=64;chunk++) {
  for(size_t pos=0;pos<strlen(request);pos+=chunk) {
   size_t n=strlen(request)-pos; if(n>chunk)n=chunk;
   assert(write(fd[1],request+pos,n)==(ssize_t)n);
   assert(!osrcore_console_read_line(&reader,input,line,sizeof(line)));
   assert(!osrcore_console_read_line(&reader,input,line,sizeof(line)));
  }
  assert(write(fd[1],"\r\n",2)==2);
  assert(osrcore_console_read_line(&reader,input,line,sizeof(line)));
  assert(strcmp(line,request)==0);
  assert(!osrcore_console_read_line(&reader,input,line,sizeof(line)));
 }
 char overflow[450];memset(overflow,'x',sizeof(overflow));
 assert(write(fd[1],overflow,sizeof(overflow))==sizeof(overflow));
 assert(!osrcore_console_read_line(&reader,input,line,sizeof(line)));
 const char *tail="osrcore reboot\nosrcore info\nstatus\n";
 assert(write(fd[1],tail,strlen(tail))==(ssize_t)strlen(tail));
 assert(osrcore_console_read_line(&reader,input,line,sizeof(line)));
 assert(strcmp(line,"osrcore info")==0); // Never execute an oversized line's suffix.
 assert(osrcore_console_read_line(&reader,input,line,sizeof(line)));
 assert(strcmp(line,"status")==0);
 assert(write(fd[1],"bad\0status\nOK\n",14)==14);
 assert(osrcore_console_read_line(&reader,input,line,sizeof(line)));
 assert(strcmp(line,"OK")==0);
 assert(!osrcore_console_read_line(&reader,input,line,sizeof(line)));
 fclose(input);close(fd[1]);return 0;
}
`);
 const bin=path.join(dir,'test');
 execFileSync('cc',['-std=c11','-Wall','-Wextra','-Werror','-I'+path.join(root,'components/osrcore_fw_update/include'),path.join(dir,'test.c'),path.join(root,'components/osrcore_fw_update/osrcore_console.c'),'-o',bin]);
 execFileSync(bin);
 } finally {await rm(dir,{recursive:true,force:true});}
});
