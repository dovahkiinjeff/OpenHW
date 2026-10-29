"""Exercise the real incoming-damage gate used by Super Salvagers."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SuperSalvagerCollision(unittest.TestCase):
    def test_damage_gate(self):
        source = (ROOT / 'src/Game/UnivUpdate.c').read_text()
        start = source.index('bool32 ApplyDamageToTarget(')
        end = source.index('    target->health -= damagetaken;', start)
        function = source[start:end] + '    target->health -= damagetaken; return FALSE;\n}\n'
        harness = r'''
#include <assert.h>
#include <math.h>
typedef int bool32; typedef float real32; typedef int sdword; typedef int GunSoundType;
#define FALSE 0
#define SOF_Dead 1
#define SOF_Slaveable 2
#define SF_SLAVE 1
#define OBJ_ShipType 1
#define OBJ_AsteroidType 2
#define SalCapCorvette 7
#define DEATH_Killed_By_Collision 777
#define DEATH_Killed_By_Kamikaze 778
typedef struct Player {int id;} Player;
typedef struct SlaveInfo SlaveInfo;
typedef struct Ship {int flags;float health;int objtype;SlaveInfo *slaveinfo;Player *playerowner;int shiptype;} Ship;
typedef Ship SpaceObjRotImpTarg;
struct SlaveInfo {int flags;Ship *Master;};
struct {Player *curPlayerPtr;} universe;
int nisIsRunning,singlePlayerGame,opSuperSalvagers;
'''
        checks = r'''
static void hit(Ship *s,int how){ApplyDamageToTarget(s,30,0,how,0);}
static void near(float a,float b){assert(fabsf(a-b)<0.0001f);}
int main(void){
 Player player={0},enemy={1}; Ship own={0,100,OBJ_ShipType,0,&player,SalCapCorvette};
 Ship hostile={0,100,OBJ_ShipType,0,&enemy,SalCapCorvette};
 Ship other={0,100,OBJ_ShipType,0,&player,8};
 Ship rock={0,100,OBJ_AsteroidType,0,0,0}; universe.curPlayerPtr=&player;
 singlePlayerGame=1;opSuperSalvagers=1;
 hit(&own,DEATH_Killed_By_Collision);near(own.health,100);
 hit(&own,DEATH_Killed_By_Kamikaze);near(own.health,100);
 hit(&own,12);near(own.health,90); /* weapons retain the existing 3x health */
 hit(&hostile,DEATH_Killed_By_Collision);near(hostile.health,70);
 hit(&other,DEATH_Killed_By_Collision);near(other.health,70);
 hit(&rock,DEATH_Killed_By_Collision);near(rock.health,70);
 opSuperSalvagers=0;hit(&own,DEATH_Killed_By_Collision);near(own.health,60);
 opSuperSalvagers=1;singlePlayerGame=0;hit(&own,DEATH_Killed_By_Collision);near(own.health,30);
 nisIsRunning=1;hit(&own,DEATH_Killed_By_Collision);near(own.health,30);
 return 0;
}
'''
        setup = r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat'
        with tempfile.TemporaryDirectory(prefix='openhw-salvage-damage-') as tmp:
            folder = Path(tmp)
            (folder / 'salvage.c').write_text(harness + function + checks)
            (folder / 'compile.cmd').write_text(f'@echo off\ncall "{setup}" x64 >nul\ncl /nologo /TC salvage.c /Fe:salvage.exe\nif errorlevel 1 exit /b 1\nsalvage.exe\n')
            result = subprocess.run(['cmd.exe', '/c', 'compile.cmd'], cwd=folder, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
