param(
  [string]$InstallDir = (Join-Path $env:RUNNER_TEMP 'InnoSetup-7.1.0')
)
$ErrorActionPreference = 'Stop'
$version = '7.1.0'
$url = 'https://github.com/jrsoftware/issrc/releases/download/is-7_1_0/innosetup-7.1.0-x64.exe'
$expected = '0362a383ed217d4c4239b5933866dd96d3eb2102737da92f80f6057a4b40df2f'
$download = Join-Path $env:RUNNER_TEMP 'innosetup-7.1.0-x64.exe'
Invoke-WebRequest -Uri $url -OutFile $download
$actual = (Get-FileHash -Algorithm SHA256 $download).Hash.ToLowerInvariant()
if ($actual -ne $expected) { throw "Inno Setup SHA-256 mismatch: $actual" }
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
$arguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-','/CURRENTUSER',"/DIR=$InstallDir")
$process = Start-Process -FilePath $download -ArgumentList $arguments -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "Inno Setup installer failed with exit code $($process.ExitCode)" }
$iscc = Join-Path $InstallDir 'ISCC.exe'
if (-not (Test-Path $iscc)) { throw "ISCC.exe not found at $iscc" }
$msysPath = $iscc -replace '\\','/'
"INNO_ISCC=$msysPath" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
Write-Host "Inno Setup $version verified: sha256:$expected"
& $iscc '/?' | Select-Object -First 8
