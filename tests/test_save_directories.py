from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class SaveDirectories(unittest.TestCase):
    def test_production_windows_directory_creation(self):
        source=(ROOT/'src/Game/File.c').read_text()
        start=source.index('bool8 fileMakeDirectory (')
        end=source.index('\n}',start)+2
        harness=r'''
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>
#define PATH_MAX 4096
#define TRUE 1
#define FALSE 0
#define stat _stat
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#define dbgAssertOrIgnore(x) ((void)0)
#define mkdir _mkdir
typedef int bool8;
struct stat fileStat;
'''
        cases=r'''
int main(void) {
    char root[4096],saved[4096],nested[4096],block[4096],failed[4096];
    assert(GetCurrentDirectoryA(sizeof(root),root));
    strcat(root,"/fresh profile");
    snprintf(saved,sizeof(saved),"%s/SavedGames",root);
    snprintf(nested,sizeof(nested),"%s/SinglePlayer",saved);
    assert(fileMakeDirectory(nested));
    assert(fileMakeDirectory(nested));
    DWORD attrs=GetFileAttributesA(nested);
    assert(attrs!=INVALID_FILE_ATTRIBUTES && (attrs&FILE_ATTRIBUTE_DIRECTORY));
    // A file masquerading as a directory must return failure, not success.
    snprintf(block,sizeof(block),"%s/blocked",root);
    FILE *f=fopen(block,"wb"); assert(f); fclose(f);
    snprintf(failed,sizeof(failed),"%s/SinglePlayer",block);
    assert(!fileMakeDirectory(block));
    assert(!fileMakeDirectory(failed));
    assert(fileMakeDirectory(""));
    assert(!fileMakeDirectory(NULL));
    char drive[4]={root[0],':','\\',0};
    assert(fileMakeDirectory(drive));
    // UNC prefixes must reach Windows unchanged, not collapse into a local path.
    assert(!fileMakeDirectory("\\\\?\\invalid-host\\not-a-drive"));
    assert(DeleteFileA(block));
    assert(RemoveDirectoryA(nested)); assert(RemoveDirectoryA(saved));
    assert(RemoveDirectoryA(root));
    return 0;
}
'''
        cache=(ROOT/'out/build/deploy-x64-clean/CMakeCache.txt').read_text()
        setup=Path(re.search(r'CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)',cache)[1])/'Common7/Tools/VsDevCmd.bat'
        with tempfile.TemporaryDirectory(prefix='hw-save-dirs-') as tmp:
            folder=Path(tmp)
            (folder/'test.c').write_text(harness+source[start:end]+cases)
            result=subprocess.run(f'cmd.exe /d /s /c ""{setup}" -arch=x64 -host_arch=x64 >nul && cl /nologo /W3 /O2 test.c /Fe:test.exe /link shell32.lib"',cwd=folder,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            subprocess.run([str(folder/'test.exe')],cwd=folder,check=True,timeout=15)

    def test_autosave_failure_is_recoverable(self):
        source=(ROOT/'src/Game/File.c').read_text()
        entry=source[source.index('filehandle fileOpen('):]
        failure=entry[entry.index('!fileMakeDestinationDirectory(fileName)'):]
        self.assertLess(failure.index('FF_ReturnNULLOnFail'),failure.index('dbgFatalf'))
        self.assertLess(failure.index('return 0;'),failure.index('dbgFatalf'))
        save=(ROOT/'src/Game/SaveGame.c').read_text()
        self.assertIn('FF_WriteMode | FF_ReturnNULLOnFail | FF_UserSettingsPath',save)
        install=(ROOT/'src/Modern/WindowsInstall.cpp').read_text()
        for tree in ('SinglePlayer','MultiPlayer','Training','RecordedGames'):
            self.assertIn('SavedGames\\\\'+tree,install)


if __name__=='__main__': unittest.main()
