"""Install the private service after the owned game data has been staged.

Run as administrator on the Ubuntu host. Nothing here downloads retail data
or places it in Caddy's public root. Existing website routes are preserved.
"""
from pathlib import Path
import json
import re
import shutil
import subprocess
import time

root=Path('/srv/openhw-game')
fingerprint=(root/'app/server-fingerprint.txt').read_text().strip()
if not re.fullmatch('[0-9a-f]{64}',fingerprint):raise SystemExit('Invalid build fingerprint')
maps={}
for line in (root/'state/maps.log').read_text().splitlines():
    fields=line.split('\t')
    if len(fields)==4 and fields[0]=='MAP':maps[fields[1]]=[int(fields[2]),int(fields[3])]
if not maps:raise SystemExit('No validated maps')
config=dict(maps=maps,fingerprint=fingerprint,port=8767,max_matches=1,
    worker_command=['/usr/lib/wine/wine64',str(root/'game/OpenHW-Server.exe'),'--dedicated-worker'],
    game_directory=str(root/'game'),log_dir=str(root/'logs'),
    worker_environment=dict(WINEPREFIX=str(root/'state/wine'),WINEDEBUG='-all',
                            WINEDLLOVERRIDES='mscoree,mshtml='))
settings=root/'state/server-config.json'
settings.write_text(json.dumps(config,indent=2)+'\n',encoding='utf-8',newline='\n')
shutil.chown(settings,user='openhw-game',group='openhw-game');settings.chmod(0o600)
subprocess.run(['chown','-R','openhw-game:openhw-game',str(root/'game')],check=True)
unit=Path('/etc/systemd/system/openhw-multiplayer.service')
unit.write_text((root/'app/openhw-multiplayer.service').read_text(),encoding='utf-8',newline='\n')
caddy=Path('/etc/caddy/Caddyfile');original=caddy.read_text()
if 'handle /multiplayer/v1' not in original:
    marker='    handle /api/* {'
    if original.count(marker)!=1:raise SystemExit('Review website configuration before inserting multiplayer route')
    updated=original.replace(marker,'    handle /multiplayer/v1 {\n        reverse_proxy 127.0.0.1:8767\n    }\n'+marker)
    candidate=Path('/etc/caddy/Caddyfile.openhw-candidate')
    candidate.write_text(updated,encoding='utf-8',newline='\n')
    subprocess.run(['caddy','validate','--config',str(candidate),'--adapter','caddyfile'],check=True)
    shutil.copy2(caddy,str(caddy)+f'.before-multiplayer-{int(time.time())}')
    candidate.replace(caddy)
subprocess.run(['systemctl','daemon-reload'],check=True)
subprocess.run(['systemctl','enable','--now','openhw-multiplayer'],check=True)
subprocess.run(['systemctl','reload','caddy'],check=True)
print(f'Private dedicated gateway enabled with {len(maps)} maps; website configuration preserved')
