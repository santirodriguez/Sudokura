param(
  [string]$InstallDir = (Join-Path $env:RUNNER_TEMP 'NSIS-3.12')
)
$ErrorActionPreference = 'Stop'
$version = '3.12'
$url = 'https://downloads.sourceforge.net/project/nsis/NSIS%203/3.12/nsis-3.12.zip'
$expected = '56581f90db321581c5381193d796fffcf2d24b2f8fed2160a6c6a3baa67f2c4f'
$archive = Join-Path $env:RUNNER_TEMP "nsis-$version.zip"

curl.exe --fail --location --retry 3 --retry-all-errors --output $archive $url
$actual = (Get-FileHash -Algorithm SHA256 $archive).Hash.ToLowerInvariant()
if ($actual -ne $expected) { throw "NSIS SHA-256 mismatch: $actual" }

if (Test-Path $InstallDir) { Remove-Item -Recurse -Force $InstallDir }
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Expand-Archive -Path $archive -DestinationPath $InstallDir -Force
$nsisRoot = Join-Path $InstallDir "nsis-$version"
$makensis = Join-Path $nsisRoot 'makensis.exe'
if (-not (Test-Path $makensis)) { throw "makensis.exe not found at $makensis" }
$msysPath = $makensis -replace '\\','/'
"NSIS_MAKENSIS=$msysPath" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
"NSIS_VERSION=$version" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
"NSIS_ARCHIVE_SHA256=$expected" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
Write-Host "NSIS $version verified: sha256:$expected"
& $makensis '/VERSION'
