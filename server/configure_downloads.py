"""Expose persistent, official downloads without changing game/website routes."""
from pathlib import Path
import shutil
import subprocess
import time


def configure():
    root=Path('/srv/openhw-downloads'); root.mkdir(mode=0o755,exist_ok=True)
    (root/'updates').mkdir(exist_ok=True)
    (root/'downloads').mkdir(exist_ok=True)
    path=Path('/etc/caddy/Caddyfile'); original=path.read_text()
    if 'root * /srv/openhw-downloads' in original:return
    marker='    handle /api/* {'
    if original.count(marker)!=1:raise RuntimeError('Review website configuration before adding downloads')
    route='''    handle /updates/* {
        root * /srv/openhw-downloads
        header Cache-Control "no-store"
        file_server
    }
    handle /downloads/* {
        root * /srv/openhw-downloads
        header Cache-Control "public, max-age=3600"
        file_server
    }
'''
    candidate=path.with_name('Caddyfile.downloads-candidate')
    candidate.write_text(original.replace(marker,route+marker),encoding='utf-8')
    subprocess.run(['caddy','validate','--config',str(candidate),'--adapter','caddyfile'],check=True)
    shutil.copy2(path,str(path)+'.before-downloads-'+str(int(time.time())))
    candidate.replace(path)
    subprocess.run(['systemctl','reload','caddy'],check=True)
    print('Persistent official download routes enabled; website and multiplayer preserved')


if __name__=='__main__':configure()
