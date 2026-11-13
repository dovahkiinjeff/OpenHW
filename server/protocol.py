"""OpenHW dedicated transport, matching src/Network/OnlineProtocol.h."""
import struct

VERSION = 1
MAX_MESSAGE = 65536
HEADER = struct.Struct('<4sHHI')
HELLO = struct.Struct('<48s32s')
SLOT = struct.Struct('<BBBBB3xIII48s')
CONFIG_PREFIX = struct.Struct('<64s14H4B5I')
CONFIG_SIZE = CONFIG_PREFIX.size + 8*SLOT.size
LOBBY_PREFIX = struct.Struct('<8sIBBBB')
LOAD_PREFIX = struct.Struct('<IBBBB')
HELLO_MSG, CREATE, JOIN, LEAVE, CONFIGURE, ADD_AI, REMOVE_SLOT, READY, START, LOADED, COMMAND = range(1,12)
PAUSE = 12
WELCOME, LOBBY, ERROR, LOAD, SYNC, STOP = range(100,106)
PAUSED = 106
WORKER_INIT, WORKER_READY, WORKER_STEP, WORKER_SYNC, WORKER_ERROR = range(200,205)
WORKER_INSPECT = 205
RULE_LIMITS = ((0,1),(0,1),(0,1),(1,4),(0,1),(0,2),(0,100),(0,1),(0,1),(0,1),(0,1),(0,1),(0,1),(0,0))

def envelope(kind, payload=b''):
    if len(payload)>MAX_MESSAGE: raise ValueError('Message too large')
    return HEADER.pack(b'OHMP',VERSION,kind,len(payload))+payload

def unpack(message):
    if not isinstance(message,bytes) or len(message)<HEADER.size: raise ValueError('Invalid envelope')
    magic,version,kind,length=HEADER.unpack_from(message)
    if magic!=b'OHMP' or version!=VERSION or length>MAX_MESSAGE or len(message)!=HEADER.size+length:
        raise ValueError('Unsupported protocol or invalid size')
    return kind,message[HEADER.size:]

def text(raw, maximum):
    if b'\0' not in raw: raise ValueError('Unterminated text')
    value=raw.split(b'\0',1)[0].decode('utf-8',errors='strict')
    if not value or len(value.encode('utf-8'))>maximum or any(ord(c)<32 for c in value):
        raise ValueError('Invalid text')
    return value

def encode_text(value, size):
    data=value.encode('utf-8')
    if len(data)>=size: raise ValueError('Text too long')
    return data+b'\0'*(size-len(data))

def default_config(map_name):
    return {'map':map_name,'rules':[0,1,0,1,1,0,100,0,1,0,0,0,0,0],
            'fleet':0,'resources':1,'bounty':3,'flags':1578,
            'injection_interval':300,'injection_amount':500,'lump_time':600,'lump_amount':1000,
            'slots':[None]*8}

def pack_config(config):
    result=CONFIG_PREFIX.pack(encode_text(config['map'],64),*config['rules'],
        config['fleet'],config['resources'],config['bounty'],0,config['flags'],
        config['injection_interval'],config['injection_amount'],config['lump_time'],config['lump_amount'])
    for slot in config['slots']:
        result+=SLOT.pack(slot['kind'],slot['race'],slot['difficulty'],slot.get('ready',0),slot.get('team',0),
            slot['base'],slot['stripe'],slot['emission'],encode_text(slot['name'],48)) if slot else bytes(SLOT.size)
    return result

def parse_config(payload, maps):
    if len(payload)!=CONFIG_SIZE: raise ValueError('Invalid configuration size')
    values=CONFIG_PREFIX.unpack_from(payload)
    name=text(values[0],63)
    if name not in maps: raise ValueError('Map not installed on server')
    rules=list(values[1:15])
    for value,(low,high) in zip(rules,RULE_LIMITS):
        if not low<=value<=high: raise ValueError('Invalid gameplay rule')
    fleet,resources,bounty,reserved,flags,interval,amount,lump_time,lump_amount=values[15:]
    if reserved or fleet>2 or resources>2 or bounty>3 or flags & ~0x7eee:
        raise ValueError('Invalid match settings')
    if interval>86400 or amount>1000000 or lump_time>86400 or lump_amount>1000000:
        raise ValueError('Resource injection is out of range')
    slots=[]
    for index in range(8):
        if any(payload[CONFIG_PREFIX.size+index*SLOT.size+5:CONFIG_PREFIX.size+index*SLOT.size+8]):
            raise ValueError('Reserved slot bytes must be zero')
        kind,race,difficulty,ready,team,base,stripe,emission,raw=SLOT.unpack_from(payload,CONFIG_PREFIX.size+index*SLOT.size)
        if kind==0: slots.append(None);continue
        if kind not in (1,2) or race>1 or difficulty>2 or ready>1 or team>8: raise ValueError('Invalid player slot')
        slots.append(dict(kind=kind,race=race,difficulty=difficulty,ready=ready,team=team,
                          base=base,stripe=stripe,emission=emission,name=text(raw,47)))
    return dict(map=name,rules=rules,fleet=fleet,resources=resources,bounty=bounty,flags=flags,
        injection_interval=interval,injection_amount=amount,lump_time=lump_time,lump_amount=lump_amount,slots=slots)
