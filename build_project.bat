@echo off
cd /d "C:\Users\PC\Documents\content\midi\Sampler"
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "TostEngineJucePocketSampler.sln" /p:Configuration=Release /p:Platform=x64 /m /v:normal
pause
