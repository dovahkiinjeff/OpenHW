!macro NSIS_HOOK_POSTINSTALL
  ; meshopt's windows-gnu build links libstdc++ dynamically. Install the
  ; MinGW runtime DLLs beside the app executable so Windows can load it.
  File "/oname=libstdc++-6.dll" "/usr/x86_64-w64-mingw32/sys-root/mingw/bin/libstdc++-6.dll"
  File "/oname=libgcc_s_seh-1.dll" "/usr/x86_64-w64-mingw32/sys-root/mingw/bin/libgcc_s_seh-1.dll"
  File "/oname=libwinpthread-1.dll" "/usr/x86_64-w64-mingw32/sys-root/mingw/bin/libwinpthread-1.dll"
!macroend

!macro NSIS_HOOK_POSTUNINSTALL
  Delete "$INSTDIR\libstdc++-6.dll"
  Delete "$INSTDIR\libgcc_s_seh-1.dll"
  Delete "$INSTDIR\libwinpthread-1.dll"
!macroend
