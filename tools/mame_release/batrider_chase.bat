@echo off
rem Armed Police Batrider CHASE! in MAME, with the batrider360 GADGETS.
rem MAME = the folder with your mame.exe; ROMS = the folder with your own batrider.zip (MAME's merged set).
rem The patched batriderj.zip in roms\ beside this file holds the CHASE! program; the rest comes from batrider.zip.
rem MAME warns that the program ROMs' checksums differ: that is expected.
set MAME=C:\mame
set ROMS=%MAME%\roms
start "" /d "%MAME%" "%MAME%\mame.exe" batriderj -rompath "%~dp0roms;%ROMS%" -artpath "%~dp0artwork" -plugin layout
