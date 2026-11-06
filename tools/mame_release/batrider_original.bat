@echo off
rem Armed Police Batrider (the original arcade game, MAME's batriderj) with the batrider360 GADGETS.
rem MAME = the folder with your mame.exe. Leave it empty to look in D:\mame, C:\mame, this folder and the PATH.
set MAME=
if not defined MAME if exist "D:\mame\mame.exe" set MAME=D:\mame
if not defined MAME if exist "C:\mame\mame.exe" set MAME=C:\mame
if not defined MAME if exist "%~dp0mame.exe" set MAME=%~dp0.
if not defined MAME for %%i in (mame.exe) do if not "%%~$PATH:i"=="" set MAME=%%~dp$PATH:i
if not defined MAME (
	echo mame.exe not found. Open this .bat in a text editor and set MAME= to your MAME folder.
	pause
	exit /b 1
)
rem ROMS = the folder(s) with your batrider.zip; empty = the rompath in MAME's own mame.ini
set ROMS=
if not defined ROMS if exist "%MAME%\mame.ini" for /f "tokens=1,*" %%a in ('findstr /b /i "rompath" "%MAME%\mame.ini"') do set ROMS=%%~b
if not defined ROMS set ROMS=roms
start "" /d "%MAME%" "%MAME%\mame.exe" batriderj -rompath "%ROMS%" -artpath "%~dp0artwork" -plugin layout
