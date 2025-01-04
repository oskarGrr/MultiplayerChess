@echo off
::use the default cmake generator (on windows it's visual studio)
vcpkg x-update-baseline --add-initial-baseline
cmake -S . -B build
pause