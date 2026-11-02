@echo off
rem Armed Police Batrider (the original arcade game, MAME's batriderj) with the batrider360 GADGETS.
rem MAME = the folder with your mame.exe; ROMS = the folder with your own batrider.zip (MAME's merged set).
set MAME=C:\mame
set ROMS=%MAME%\roms
start "" /d "%MAME%" "%MAME%\mame.exe" batriderj -rompath "%ROMS%" -artpath "%~dp0artwork" -plugin layout
