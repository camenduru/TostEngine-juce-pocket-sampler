$ErrorActionPreference = "Continue"
Set-Location "C:\Users\PC\Documents\content\midi\Sampler"

# Build the project
Write-Host "Building TostEngineJucePocketSampler..." -ForegroundColor Cyan
$buildResult = & "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "TostEngineJucePocketSampler.sln" /p:Configuration=Release /p:Platform=x64 /m /v:normal 2>&1

foreach ($line in $buildResult) {
    Write-Host $line
}

# Check if build succeeded
if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild successful!" -ForegroundColor Green

    # Run the application
    $exePath = "C:\Users\PC\Documents\content\midi\Sampler\Release\TostEngineJucePocketSampler.exe"
    if (Test-Path $exePath) {
        Write-Host "Running TostEngineJucePocketSampler..." -ForegroundColor Cyan
        & $exePath
    } else {
        Write-Host "ERROR: Executable not found at $exePath" -ForegroundColor Red
    }
} else {
    Write-Host "`nBuild failed!" -ForegroundColor Red
}
