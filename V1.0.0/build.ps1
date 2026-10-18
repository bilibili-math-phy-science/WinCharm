$env:TMP="X:\tmp"
$env:TEMP="X:\tmp"
windres clicker.rc -O coff -o clicker.o
g++ clicker.cpp clicker.o -o "X:\C\Program\clicker.exe" -municode -finput-charset=UTF-8 -luser32 -lgdi32 -lshell32 -lcomdlg32 -mwindows
$env:TMP="X:\tmp"
$env:TEMP="X:\tmp"
windres WinCharm.rc -O coff -o WinCharm.o
g++ WinCharm.cpp WinCharm.o -o "X:\C\Program\WinCharm.exe" -municode -finput-charset=UTF-8 -luser32 -lgdi32 -lshell32 -lcomdlg32 -mwindows
$env:TMP="X:\tmp"
$env:TEMP="X:\tmp"
windres WccSaveOpen.rc -O coff -o WccSaveOpen.o
g++ -shared -o ./release/WccSaveOpen.dll WccSaveOpen.cpp WccSaveOpen.o -static-libgcc -static-libstdc++
.\WinCharm.exe