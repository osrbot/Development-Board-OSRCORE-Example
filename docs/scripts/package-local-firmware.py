"""Package successful local ESP-IDF builds for the local tutorial site.
Run using the ESP-IDF Python environment after building examples.
"""
from pathlib import Path
import json, shutil, subprocess, sys
from datetime import datetime, timezone
repo = Path(__file__).resolve().parents[2]
dest = repo/'docs/public/firmware/local'
dest.mkdir(parents=True, exist_ok=True)
examples=[]
for example in sorted(repo.glob('[0-9][0-9]_*')):
    build=example/'build'
    app=build/(example.name+'.bin')
    if not app.is_file(): continue
    args=json.loads((build/'flasher_args.json').read_text())
    full=dest/('osrcore-'+example.name+'-full.bin')
    cmd=[sys.executable,'-m','esptool','--chip','esp32s3','merge_bin','-o',str(full)]
    for offset, path in args['flash_files'].items(): cmd.extend([offset,str(build/path)])
    subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
    shutil.copyfile(app,dest/('osrcore-'+example.name+'-app.bin'))
    examples.append({'id':example.name,'label':example.name.replace('_',' '),'bin':'/firmware/local/'+full.name})
    if example.name=='13_factory_demo':
        shutil.copyfile(app,dest.parent/'osrbot_ESP32S3_IDF_App.bin')
        shutil.copyfile(full,dest.parent/'osrbot_ESP32S3_IDF_FullFlash.bin')
(dest.parent/'manifest.json').write_text(json.dumps({'version':'local-development','date':datetime.now(timezone.utc).isoformat(),'examples':examples},indent=2)+'\n')
print(f'Packaged {len(examples)} locally compiled examples; hardware execution has not been verified.')
