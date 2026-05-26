@echo off

p4 edit libs/...

REM debug

REM releae

p4 revert -a libs/...
