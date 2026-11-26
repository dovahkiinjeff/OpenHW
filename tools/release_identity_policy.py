"""Local fleet identities are player files, never release defaults or presets."""
import re

def private_identity_key(key):
    key=key.lstrip('\ufeff').lower()
    return (key in {'colorspicked','playerrace'} or
            bool(re.fullmatch(r'prevcolor\d+\.(?:base|stripe)',key)) or
            bool(re.fullmatch(r'player(?:base|stripe|enginetrail|navlight|resourcebeam|hyperspace)(?:color|override)',key)))

def remove_private_identity(text):
    return '\n'.join(line for line in text.splitlines()
        if not line.split() or not private_identity_key(line.split()[0]))+'\n'

def verify_public_identity(text,source):
    for line in text.splitlines():
        words=line.split()
        if words and private_identity_key(words[0]):
            raise ValueError('Private player identity in release configuration: '+str(source)+' ('+words[0]+')')
