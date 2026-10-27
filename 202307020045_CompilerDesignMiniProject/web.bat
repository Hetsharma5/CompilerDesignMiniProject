@echo off
if not exist minicc.exe call build.bat
python server.py
