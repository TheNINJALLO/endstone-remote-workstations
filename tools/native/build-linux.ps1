$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'docker-common.ps1')
. (Join-Path $PSScriptRoot 'build-context.ps1')
$vcfRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $vcfRoot
$vcfContext=$null
try {
 $vcfDocker=Find-VcfDocker
 Test-VcfDockerEngine $vcfDocker
 $vcfContext=New-VcfBuildContext $vcfRoot
 & $vcfDocker buildx build --platform linux/amd64 --target artifact-export --output type=local,dest=dist/linux-x64-dev $vcfContext.Path
 if ($LASTEXITCODE -ne 0) { throw 'Linux container build failed.' }
} finally { try {Remove-VcfBuildContext $vcfContext} finally {Pop-Location} }
