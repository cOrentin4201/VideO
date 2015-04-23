@echo off

if not exist %1\data (
  xcopy /E /I /Y /Q data %1\data
)

if not exist %1\videO.exe.config (
  xcopy /Y /Q misc\windows\videO.exe.config %1
)

if not exist %1\readme.html (
  xcopy /Y /Q misc\readme.html %1
)
