# Build on Windows

Compiling on Windows is done using the MinGW-w64 compiler/toolchain.
This is a windows version of the GCC compiler that (by default) uses the MSVCRT.DLL libc crt (runtime) that we assume to be included in all windows versions (so no external/exotic DLLs need to be distributed).

## Compiling via WinLibs (portable)
- Download "GCC 12.2.0 + LLVM/Clang/LLD/LLDB 14.0.6 + MinGW-w64 10.0.0 (MSVCRT) - release 2" Win64 zip archive & extract it: https://winlibs.com
- Add the MinGW bin folder to your environment PATH variable: `C:\winlibs\mingw64\bin`
- Open cmd.exe and run `build-gcc production` to build and `run.bat` to run (or `brun.bat` to build and run a dev version)

## Compiling via MSYS2 (semi-portable)
- Download & install MSYS2: https://www.msys2.org
- Open "MSYS2 MSYS" from the start menu
- Update the pacman package manager `pacman -Syu`
- Close msys, reopen it, and update again `pacman -Syu`
- Install the MinGW-w64 compiler toolchain `pacman -S --needed base-devel mingw-w64-x86_64-toolchain`
- Add the MinGW bin folder to your environment PATH variable: `C:\msys64\mingw64\bin`
- Open cmd.exe and run `build-gcc production` to build and `run.bat` to run (or `brun.bat` to build and run a dev version)

NB: It seems we do not need to run the build tools from inside the "MSYS2 MinGW x64" console provided by msys2, it also works from a regular cmd.exe

## Building with CMake (optional)
- Download & install/extract cmake (portable zip available): https://cmake.org
- Add the cmake bin folder to your environment PATH variable: `C:\cmake\bin`
- Open cmd.exe and run `build-cmake production` to build

## Compiling the MSI via WiX (portable)
- Download & install/extract WiX toolset (portable zip available): https://wixtoolset.org
- Add the wix folder to your environment PATH variable: `C:\wix`
- Open cmd.exe and navigate to the pkg/msi/opticon-agent folder and run `build` to build the customAction DLL and the MSI
- opticon-{gitVersionTag}.msi is now in opticon/bin

## "Manually" installing Opticon Agent as a Windows service
- Open cmd.exe as Administrator ("Run as administrator")
- Navigate to opticon-agent.exe
- Run `opticon-agent --winservice install`

Other options to the `--winservice` flag are `installAndStart` and `uninstall`