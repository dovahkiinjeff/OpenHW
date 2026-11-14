"""Server-owned rooms. No member can supply their own player index or authority."""
from copy import deepcopy
from dataclasses import dataclass, field
import secrets
import time
from protocol import *

class LobbyError(ValueError): pass

@dataclass(eq=False)
class Member:
    name: str
    fingerprint: bytes
    connection: object = None
    room: object = None
    slot: int = -1
    loaded: bool = False
    signature: bytes = b''
    tokens: float = 80
    last_request: float = field(default_factory=time.monotonic)

    def consume(self):
        now=time.monotonic()
        self.tokens=min(80,self.tokens+(now-self.last_request)*40)
        self.last_request=now
        if self.tokens<1: raise LobbyError('Too many requests')
        self.tokens-=1

class Room:
    def __init__(self, host, maps, code):
        self.code=code;self.maps=maps;self.host=host;self.members={};self.state=0
        self.config=default_config(next(iter(maps)))
        self.revision=0;self.worker=None;self.commands=[];self.task=None
        self.paused=False
        self.add_member(host)

    def host_only(self, member):
        if member is not self.host: raise LobbyError('Only the host can change match setup')
        if self.state: raise LobbyError('Match settings are locked after starting')

    def changed(self):
        self.revision+=1
        for slot in self.config['slots']:
            if slot: slot['ready']=int(slot['kind']==2)

    def capacity(self): return self.maps[self.config['map']][1]

    def add_member(self, member):
        if self.state: raise LobbyError('Match already started')
        if member.room is not None: raise LobbyError('Leave your current room first')
        if member.fingerprint!=self.host.fingerprint: raise LobbyError('Game build/content does not match the host')
        if sum(s is not None for s in self.config['slots'])>=self.capacity(): raise LobbyError('Room is full')
        index=self.config['slots'].index(None)
        self.config['slots'][index]=dict(kind=1,race=0,difficulty=1,ready=0,team=0,
            base=0xff303030,stripe=0xff2020ff,emission=0xff2020ff,name=member.name)
        self.members[index]=member;member.room=self;member.slot=index
        self.changed()

    def add_ai(self, member, payload):
        self.host_only(member)
        if len(payload)!=3: raise LobbyError('Expected race, difficulty and team')
        race,difficulty,team=payload
        if race>1 or difficulty>2 or team>8: raise LobbyError('Invalid AI setup')
        if sum(s is not None for s in self.config['slots'])>=self.capacity(): raise LobbyError('Map has no free player slots')
        index=self.config['slots'].index(None)
        self.config['slots'][index]=dict(kind=2,race=race,difficulty=difficulty,team=team,ready=1,
            base=0xff707070,stripe=0xff40c0ff,emission=0xff40c0ff,name=f'AI {index+1}')
        self.changed()

    def remove_slot(self, member, payload):
        self.host_only(member)
        if len(payload)!=1 or payload[0]>=8: raise LobbyError('Invalid slot')
        index=payload[0]
        if index in self.members: raise LobbyError('Human players leave their own slot')
        if self.config['slots'][index] is None: raise LobbyError('Slot is already empty')
        self.config['slots'][index]=None;self.changed()

    def configure(self, member, payload):
        self.host_only(member)
        if len(payload)!=4+CONFIG_SIZE: raise LobbyError('Invalid setup size')
        revision=struct.unpack_from('<I',payload)[0]
        if revision!=self.revision: raise LobbyError('Setup changed; review the latest room state')
        config=parse_config(payload[4:],self.maps)
        if sum(s is not None for s in config['slots'])>self.maps[config['map']][1]: raise LobbyError('Too many players for this map')
        for index,(old,new) in enumerate(zip(self.config['slots'],config['slots'])):
            if (old is None)!=(new is None) or (old and old['kind']!=new['kind']):
                raise LobbyError('Use Add AI / Remove AI to change slots')
            if new and new['kind']==1:
                new['name']=self.members[index].name
        self.config=config;self.changed()

    def ready(self, member, payload):
        if self.state or len(payload)!=5: raise LobbyError('Invalid ready request')
        revision,value=struct.unpack('<IB',payload)
        if revision!=self.revision or value>1: raise LobbyError('Review current settings before marking ready')
        self.config['slots'][member.slot]['ready']=value

    def begin(self, member):
        self.host_only(member)
        players=[s for s in self.config['slots'] if s]
        if len(players)<max(2,self.maps[self.config['map']][0]): raise LobbyError('Add another human or AI player')
        if not all(s['ready'] for s in players): raise LobbyError('All human players must be ready')
        if not any(a['team']==0 or b['team']==0 or a['team']!=b['team']
                   for i,a in enumerate(players) for b in players[i+1:]):
            raise LobbyError('At least two opposing teams are required')
        # The classic engine expects human slots first, then AI. Keep each
        # member attached to their own slot when compacting, including holes.
        order=sorted((i for i,s in enumerate(self.config['slots']) if s),
                     key=lambda i:(self.config['slots'][i]['kind'],i))
        slots=[self.config['slots'][i] for i in order]
        members={new:self.members[old] for new,old in enumerate(order) if old in self.members}
        self.config['slots']=slots+[None]*(8-len(slots));self.members=members
        for index,m in members.items():m.slot=index;m.loaded=False;m.signature=b''
        self.seed=secrets.randbits(32);self.state=1

    def load_payload(self, member=None):
        return LOAD_PREFIX.pack(self.seed,member.slot if member else 0,len(self.members),
            sum(s is not None for s in self.config['slots']),0)+pack_config(self.config)

    def snapshot(self, member):
        return LOBBY_PREFIX.pack(encode_text(self.code,8),self.revision,member.slot,
            member is self.host,self.state,0)+pack_config(self.config)

    def remove_member(self, member):
        self.members.pop(member.slot,None)
        self.config['slots'][member.slot]=None
        member.room=None;member.slot=-1
        if self.members and member is self.host:self.host=next(iter(self.members.values()))
        if not self.state:self.changed()

class Lobby:
    def __init__(self, maps, fingerprint):
        if not maps: raise ValueError('No installed multiplayer maps')
        self.maps=maps;self.fingerprint=fingerprint;self.rooms={}

    def hello(self, payload):
        if len(payload)!=HELLO.size: raise LobbyError('Invalid hello')
        raw,fingerprint=HELLO.unpack(payload)
        name=text(raw,17) # Legacy engine player names have 18 bytes including NUL.
        if fingerprint!=self.fingerprint: raise LobbyError('This server requires the matching OpenHW multiplayer build')
        return Member(name,fingerprint)

    def directory(self):
        """Bounded, sorted room directory; no tokens, fingerprints or gameplay data."""
        rooms=sorted(self.rooms.values(),key=lambda room:room.code)
        payload=struct.pack('<I',len(rooms))
        for room in rooms:
            slots=[slot for slot in room.config['slots'] if slot]
            payload+=ROOM_SUMMARY.pack(encode_text(room.code,8),encode_text(room.host.name,48),
                encode_text(room.config['map'],64),sum(s['kind']==1 for s in slots),
                sum(s['kind']==2 for s in slots),room.state,room.capacity())
        return payload

    def create(self, member):
        if member.room is not None: raise LobbyError('Leave current room first')
        if len(self.rooms)>=16: raise LobbyError('Server room limit reached')
        code=''.join(secrets.choice('ABCDEFGHJKLMNPQRSTUVWXYZ23456789') for _ in range(6))
        while code in self.rooms:code=''.join(secrets.choice('ABCDEFGHJKLMNPQRSTUVWXYZ23456789') for _ in range(6))
        room=Room(member,self.maps,code);self.rooms[code]=room;return room

    def join(self, member, payload):
        if len(payload)!=8: raise LobbyError('Invalid room code')
        code=text(payload,6).upper()
        room=self.rooms.get(code)
        if not room: raise LobbyError('Room not found')
        room.add_member(member);return room
