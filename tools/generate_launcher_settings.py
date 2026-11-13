"""Generate the launcher's settings catalogue from the game's persisted options."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]


def generate():
    source=(ROOT/'src/SDL/utility.c').read_text(encoding='utf-8')
    table=source.split('scriptEntry utyOptionsList[] =',1)[1].split('END_SCRIPT_ENTRY',1)[0]
    defaults={}
    for line in (ROOT/'assets/Release/defaults.cfg').read_text().splitlines():
        parts=line.split(None,1)
        if len(parts)==2 and not line.startswith('['): defaults[parts[0]]=parts[1]
    hidden={'HomeworldDataPath','screenDepth','detectDisplayOnFirstBoot','deviceCRC','deviceIndex','dlaa','KeyBindingsVersion','ColorsPicked','TutorialNeeded','MultiPlayerLastMapID','FirewallDetect','DisableSPResourceScaling'}
    labels={'antiAliasing':'Anti-aliasing / upscaling','frameGeneration':'Frame generation','raytracing':'Path tracing','rasterGlowBrightness':'Non-PT glow brightness (%)','refreshRate':'Exclusive fullscreen refresh rate (Hz; 0 = automatic)','mainFrameRateLimit':'Frame rate limit','frameRateLimit':'Frame rate limit (0 = unlimited)','opMouseSens':'Camera sensitivity (%)','SPResourcePopulationPercent':'Mission resource population (%)','StartingResourceFleet':'Campaign starting resource fleet','keyboardLayout':'Keyboard layout','voiceLanguage':'Voice language','language':'Classic interface / subtitle language','fullScreen':'Fullscreen','exclusiveFullscreen':'Exclusive fullscreen','vSync':'VSync','MusicVolume':'Music volume (%)','SpeechVolume':'Speech volume (%)','SFXVolume':'Effects volume (%)','godRayQuality':'God ray quality','uiScalePercent':'Interface scale (%)','textureResolution':'Texture resolution','noLOD':'Disable classic LOD reduction','screenWidth':'Screen width','screenHeight':'Screen height'}
    bools=set('fullScreen exclusiveFullscreen raytracing lensFlares noLOD enableSmoothing texLinearFiltering enableStipple enableTrails etgDamageEffectsEnabled etgHitEffectsEnabled etgFireEffectsEnabled etgBulletEffectsEnabled InfoOverlay fuzzyBlobs InstantTransition Voice0 Voice1 Voice2 VoiceCommands VoiceStatus VoiceChatter AutoChannel PauseOrders UnlimitedFuel SuperSalvagers CaptureBuildAll DisableUnitCaps RankSystem UnlimitedRUs RestoreFogOfWar showFPS PlayIntros ShipRecoil vSync'.split())
    choices={
        'antiAliasing': ['Off','NVIDIA DLAA (experimental)','FXAA','DLSS Quality (experimental)','DLSS Balanced (experimental)','DLSS Performance (experimental)','DLSS Ultra Performance (experimental)','Automatic / Quality','Automatic / Native AA','Universal temporal AA','AMD FSR Native AA (recommended)','AMD FSR Quality (recommended)','AMD FSR Balanced','AMD FSR Performance','AMD FSR Ultra Performance','Intel XeSS AA (experimental)','XeSS Quality (experimental)','XeSS Balanced (experimental)','XeSS Performance (experimental)','XeSS Ultra Performance (experimental)'],
        'frameGeneration':['Off','Automatic','NVIDIA (not implemented)','AMD FSR 2x (recommended)','Intel (not implemented)'],
        'textureResolution':['Low / 512','Medium / 1024','High / 2048','Ultra / original source'],
        'godRayQuality':['Low','Medium','High','Ultra'],
        'language':['English','French','German','Spanish','Italian'],
        'keyboardLayout':['Windows / automatic','US QWERTY / physical keys','French AZERTY','German QWERTZ','Spanish QWERTY','Italian QWERTY','US Dvorak'],
        'StartingResourceFleet':['Mission default','At least 3 collectors and 2 controllers'],
        'MissionStartLaunch':['Vanilla','Hold combat craft','Hold all carried ships'],
        'PlayerRace':['Kushan','Taiidan'],
        'effectsLevel':['High','Low','Custom'],
    }
    groups={'init options':'Graphics','graphics options':'Graphics / classic effects','sound options':'Audio','input options':'Keyboard & mouse','player preferences':'Player & language','single player':'Gameplay','multiplayer options':'Skirmish','New HWSDL Options':'Gameplay'}
    defaults.update(vSync='1',godRayQuality='1',frameRateLimit='0',keyboardLayout='0',voiceLanguage='0',StartingResourceFleet='0')
    rows=[]; group='Graphics'
    for match in re.finditer(r'\{"([^"\n]*)",\s*(script\w+),\s*&([^}]+)\}',table):
        key,kind,variable=match.groups()
        if key.startswith('\\n['): group=groups.get(key[3:].split(']')[0],group); continue
        if key in hidden or key.startswith('PrevColor'): continue
        label=labels.get(key,re.sub(r'(?<=[a-z])(?=[A-Z])',' ',key).replace('_',' ').replace('opBattle','Battle').title())
        row=dict(key=key,label=label,group=group,kind='text' if kind=='scriptSetStringCB' else 'bool' if key in bools or kind=='scriptSetBool' else 'int',default=defaults.get(key,'0'))
        if key=='keyboardLayout':row['group']='Keyboard & mouse'
        if row['kind']=='bool': row['numeric_bool']=kind!='scriptSetBool'
        if key.startswith('Player') and key.endswith('Color'): row['kind']='color'
        if key in choices: row['choices']={str(i):name for i,name in enumerate(choices[key])}
        if key.isupper() and group=='Keyboard & mouse':
            names={0:'Unbound',**{i+4:chr(65+i) for i in range(26)},**{i+30:str(i+1) for i in range(9)},39:'0',40:'Enter',41:'Escape',42:'Backspace',43:'Tab',44:'Space',45:'Minus',46:'Equals',47:'Left bracket',48:'Right bracket',49:'Backslash',51:'Semicolon',52:'Apostrophe',53:'Grave',54:'Comma',55:'Period',56:'Slash',57:'Caps Lock',**{i+58:'F'+str(i+1) for i in range(12)},70:'Print Screen',71:'Scroll Lock',72:'Pause',73:'Insert',74:'Home',75:'Page Up',76:'Delete',77:'End',78:'Page Down',79:'Right',80:'Left',81:'Down',82:'Up',88:'Keypad Enter'}
            row['choices']={str(i):name for i,name in names.items()}
        if key in ('screenWidth','screenHeight'): row.update(min=320,max=32768,default='1024' if key=='screenWidth' else '768')
        elif key=='refreshRate': row.update(max=2000,help='Borderless and windowed use the Windows desktop refresh rate.')
        elif key=='SPResourcePopulationPercent': row.update(max=100,help='0 = vanilla resource scaling; 100 = all authored harvestable resources.')
        elif key=='keyboardLayout': row.update(help='Shortcut letters follow this layout. Text entry continues to use Windows.')
        elif key=='language': row.update(help='Original localized strings/subtitles; new OpenHW interface labels remain English.')
        elif key=='voiceLanguage': row.update(help='Only complete, checksum-matched installed voice packs are offered.')
        elif key=='StartingResourceFleet': row.update(help='Adds missing resource ships on a fresh campaign mission; never removes carried ships.')
        elif key in ('brightnessVal','MusicVolume','SpeechVolume','SFXVolume'): row['max']=100
        elif key=='uiScalePercent': row.update(min=50,max=200)
        rows.append(row)
    (ROOT/'tools/launcher_settings.py').write_text('# Generated by generate_launcher_settings.py; no map/editor settings.\nSETTINGS = '+repr(rows)+'\n',encoding='utf-8')
    print('Generated',len(rows),'launcher settings')


if __name__=='__main__': generate()
