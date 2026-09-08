$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'docker-common.ps1')
$vcfRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $vcfRoot
try {
 $vcfDocker=Find-VcfDocker
 Test-VcfDockerEngine $vcfDocker
 & $vcfDocker buildx build --platform linux/amd64 --target artifact-export --output type=local,dest=dist/linux-x64-dev .
 if ($LASTEXITCODE -ne 0) { throw 'Linux container build failed.' }
} finally { Pop-Location }
