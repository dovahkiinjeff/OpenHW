"""Install private diagnostics intake on the existing, intended Ubuntu VPS.

Administrator-only deployment helper. Preserves website/download/game routes.
No domain, DNS, billing or private game-content changes are made here.
"""
from datetime import datetime, timezone
import os
from pathlib import Path
import shutil
import subprocess

APP = Path('/srv/openhw-game/app')
ROOT = Path('/srv/openhw-reports')
CADDY = Path('/etc/caddy/Caddyfile')
SERVICE = Path('/etc/systemd/system/openhw-diagnostics.service')
BEGIN = '    # BEGIN OPENHW PRIVATE DIAGNOSTICS'
END = '    # END OPENHW PRIVATE DIAGNOSTICS'
ROUTE = '''    # BEGIN OPENHW PRIVATE DIAGNOSTICS
    handle /diagnostics/v1/reports {
        request_body {
            max_size 64MiB
        }
        reverse_proxy 127.0.0.1:8768 {
            header_up X-Forwarded-For {remote_host}
            transport http {
                response_header_timeout 130s
            }
        }
    }
    # END OPENHW PRIVATE DIAGNOSTICS
'''


def add_route(text):
    if text.count(BEGIN) != text.count(END) or text.count(BEGIN) > 1:
        raise ValueError('Ambiguous existing diagnostics route')
    if BEGIN in text:
        start, end = text.index(BEGIN), text.index(END) + len(END)
        text = text[:start] + text[end:].lstrip('\r\n')
    # This helper targets the existing shared site block whose multiplayer
    # route precedes API, downloads and fallback handlers. Refuse guessing.
    if text.count('    handle /multiplayer/v1 {') != 1:
        raise ValueError('Expected one existing multiplayer route; review Caddyfile')
    start = text.index('    handle /multiplayer/v1 {')
    if '/diagnostics/v1/reports' in text:
        raise ValueError('Unmanaged diagnostics route already exists; review it')
    return text[:start] + ROUTE + text[start:]


def configure():
    import grp
    import pwd

    if os.geteuid() != 0:
        raise PermissionError('Run this intended-host installer as administrator')
    account, group = pwd.getpwnam('openhw-game'), grp.getgrnam('openhw-game')
    source = Path(__file__).resolve().parent
    if source != APP or not (source / 'diagnostics.py').is_file():
        raise ValueError('Place the server sources in /srv/openhw-game/app first')
    ROOT.mkdir(mode=0o700, parents=True, exist_ok=True)
    if ROOT.is_symlink() or not ROOT.is_dir():
        raise ValueError('Private reports root must be a real directory')
    os.chown(ROOT, account.pw_uid, group.gr_gid)
    os.chmod(ROOT, 0o700)
    original = CADDY.read_text(encoding='utf-8')
    updated = add_route(original)
    candidate = CADDY.with_name('Caddyfile.openhw-diagnostics.pending')
    candidate.write_text(updated, encoding='utf-8')
    try:
        subprocess.run(['caddy', 'validate', '--config', str(candidate), '--adapter', 'caddyfile'], check=True)
        if updated != original:
            timestamp = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
            shutil.copy2(CADDY, CADDY.with_name('Caddyfile.before-diagnostics-' + timestamp))
            candidate.replace(CADDY)
        service_text = (source / 'openhw-diagnostics.service').read_text(encoding='utf-8')
        if SERVICE.exists() and SERVICE.read_text(encoding='utf-8') != service_text:
            timestamp = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
            shutil.copy2(SERVICE, SERVICE.with_name(SERVICE.name + '.before-' + timestamp))
        SERVICE.write_text(service_text, encoding='utf-8')
        os.chmod(SERVICE, 0o644)
        subprocess.run(['systemctl', 'daemon-reload'], check=True)
        subprocess.run(['systemctl', 'enable', '--now', 'openhw-diagnostics'], check=True)
        subprocess.run(['systemctl', 'reload', 'caddy'], check=True)
    finally:
        candidate.unlink(missing_ok=True)
    print('Private diagnostics enabled on loopback 8768; existing website, download and multiplayer routes preserved')


if __name__ == '__main__':
    configure()
