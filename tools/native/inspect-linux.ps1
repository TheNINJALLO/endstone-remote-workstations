param([string]$Inputs=$env:VCF_LINUX_INPUTS)
. (Join-Path $PSScriptRoot 'docker-common.ps1')
$vcfRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$vcfPreviousInputs=$env:VCF_LINUX_INPUTS
Push-Location $vcfRoot
try {
    $vcfDocker=Find-VcfDocker
    Test-VcfDockerEngine $vcfDocker
    $env:VCF_LINUX_INPUTS=Resolve-VcfLinuxInputs $Inputs
    New-Item -ItemType Directory -Path out/linux-abi -Force | Out-Null
    & $vcfDocker compose --profile research build abi-lab
    if ($LASTEXITCODE -ne 0) {throw 'Linux ABI lab build failed.'}
    & $vcfDocker compose --profile research run --rm --no-deps --entrypoint /bin/sh abi-lab /workspace/tools/native/inspect-runtime.sh
    if ($LASTEXITCODE -ne 0) {throw 'Linux runtime input inspection failed; inspect out/linux-abi.'}
} finally {$env:VCF_LINUX_INPUTS=$vcfPreviousInputs;Pop-Location}
