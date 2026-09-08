param([string]$Inputs=$env:VCF_LINUX_INPUTS)
. (Join-Path $PSScriptRoot 'docker-common.ps1')
$vcfRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$vcfPreviousInputs=$env:VCF_LINUX_INPUTS
Push-Location $vcfRoot
try {
    $vcfDocker=Find-VcfDocker
    Test-VcfDockerEngine $vcfDocker
    $env:VCF_LINUX_INPUTS=Resolve-VcfLinuxInputs $Inputs
    if ($env:VCF_LICENSE_CONFIRMED -ne '1') {throw 'BLOCKED: the applicable server license must be confirmed separately; this script does not accept it.'}
    if (-not (Test-Path -LiteralPath (Join-Path $env:VCF_LINUX_INPUTS 'launch.sh'))) {throw 'BLOCKED: an admitted private Linux launcher is required.'}
    New-Item -ItemType Directory -Path out/linux-smoke -Force | Out-Null
    & $vcfDocker compose --profile runtime build smoke
    if ($LASTEXITCODE -ne 0) {throw 'Linux smoke image build failed.'}
    & $vcfDocker compose --profile runtime run --rm --service-ports smoke
    if ($LASTEXITCODE -ne 0) {throw 'Linux runtime smoke failed or was blocked; no gameplay qualification is implied.'}
} finally {$env:VCF_LINUX_INPUTS=$vcfPreviousInputs;Pop-Location}
