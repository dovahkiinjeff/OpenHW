"""Build a private diagnostic source copy, without changing release sources."""
import argparse,shutil
from pathlib import Path
def prepare(source,destination):
 source,destination=source.resolve(),destination.resolve()
 if destination.exists() or destination==source or source.is_relative_to(destination):raise ValueError('Choose a new isolated destination')
 shutil.copytree(source,destination,ignore=shutil.ignore_patterns('.git','out','.deps','__pycache__'))
 def edit(name,old,new,count=1):
  p=destination/name;s=p.read_text(encoding='utf-8')
  if s.count(old)!=count:raise ValueError('Patch no longer matches: '+name)
  p.write_text(s.replace(old,new),encoding='utf-8')
 edit('src/Network/MatchRules.c','int hwMatchInstall(const OHLoad *load)','static int matchInstall(const OHLoad *load,int lab)')
 edit('src/Network/MatchRules.c','!load->humans || load->humans>load->players','(!load->humans && !lab) || load->humans>load->players')
 edit('src/Network/MatchRules.c','load->ownSlot>=load->humans','(load->humans ? load->ownSlot>=load->humans : load->ownSlot!=0)')
 edit('src/Network/MatchRules.c','void hwMatchClear(void)','int hwMatchInstall(const OHLoad *load){return matchInstall(load,0);}\nint hwMatchInstallAiLab(const OHLoad *load){return load && !load->humans && load->players==8 && matchInstall(load,1);}\nvoid hwMatchClear(void)')
 edit('src/Network/DedicatedServer.c','static int workerRun(void)','static int aiLab;\nint hwMatchInstallAiLab(const OHLoad *load);\n#include "AiLabSnapshot.inc"\nstatic int workerRun(void)')
 edit('src/Network/DedicatedServer.c','if(request.type==OH_STOP && !request.length)return 0;','if(request.type==OH_STOP && !request.length)return 0;\n        if(aiLab && request.type==206 && !request.length){aiLabSnapshot(expected);if(!sendMessage(206,&expected,sizeof(expected)))return 6;continue;}')
 edit('src/Network/DedicatedServer.c','else if(!strcmp(argv[i],"--list-maps"))listMaps=1;','else if(!strcmp(argv[i],"--list-maps"))listMaps=1;\n        else if(!strcmp(argv[i],"--ai-lab")){aiLab=1;aiplayerLogEnable=TRUE;}')
 edit('src/Network/DedicatedServer.c','if(!hwMatchInstall(&load))return 7;','if(!(aiLab?hwMatchInstallAiLab(&load):hwMatchInstall(&load)))return 7;')
 # Normal dedicated startup already initializes hyperspace without renderer effects.
 edit('src/Game/AIPlayer.h','#ifdef HW_BUILD_FOR_DEBUGGING\n#define aiplayerLog(x)    aiplayerDebugLog x','#if defined(OPENHW_AI_LAB)\n#define aiplayerLog(x) do { if(aiplayerLogEnable) aiplayerDebugLog x; } while(0)\n#elif defined(HW_BUILD_FOR_DEBUGGING)\n#define aiplayerLog(x)    aiplayerDebugLog x')
 edit('src/Game/AIPlayer.c','if (aiplayerLogEnable)\n    {\n        logfileClear','if (aiplayerLogEnable && !getenv("OPENHW_AI_LAB"))\n    {\n        logfileClear')
 edit('src/Game/AIPlayer.c','void aiplayerDebugLog(uword playerIndex, char *format, ...)\n{','''void aiplayerDebugLog(uword playerIndex, char *format, ...)
{
    if(aiplayerLogEnable && getenv("OPENHW_AI_LAB")) {
        char text[2048];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
        fprintf(stderr,"AI_EVENT\\t%u\\t%.4f\\t%s\\n",playerIndex,universe.totaltimeelapsed,text);return;
    }
''')
 shutil.copyfile(Path(__file__).with_name('engine_snapshot.inc'),destination/'src/Network/AiLabSnapshot.inc')
 with (destination/'CMakeLists.txt').open('a',encoding='utf-8') as f:f.write('\ntarget_compile_definitions(homeworld PRIVATE OPENHW_AI_LAB=1)\n')
 (destination/'AI-LAB-ONLY.txt').write_text('Private test build. Not a release or public multiplayer worker.\n')
 return destination
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--destination',type=Path,required=True)
 a=p.parse_args();print(prepare(a.source,a.destination))
