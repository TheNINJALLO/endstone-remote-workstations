$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
function Find-VcfDocker {
    $vcfCommand=Get-Command docker -ErrorAction SilentlyContinue
    if ($vcfCommand) {return $vcfCommand.Source}
    foreach ($vcfCandidate in @(
        (Join-Path $env:LOCALAPPDATA 'Programs\DockerDesktop\resources\bin\docker.exe'),
        (Join-Path $env:ProgramFiles 'Docker\Docker\resources\bin\docker.exe')
    )) {if (Test-Path -LiteralPath $vcfCandidate -PathType Leaf) {return $vcfCandidate}}
    throw 'BLOCKED: Docker CLI is unavailable. Install Docker Desktop with its WSL 2 backend.'
}
function Test-VcfDockerEngine([string]$Docker) {
    $vcfProbe=[Diagnostics.Process]::new()
    $vcfProbe.StartInfo=[Diagnostics.ProcessStartInfo]::new($Docker,'info --format "{{json .}}"')
    $vcfProbe.StartInfo.UseShellExecute=$false
    $vcfProbe.StartInfo.CreateNoWindow=$true
    $vcfProbe.StartInfo.RedirectStandardOutput=$true
    $vcfProbe.StartInfo.RedirectStandardError=$true
    try {
        [void]$vcfProbe.Start()
        $vcfOutput=$vcfProbe.StandardOutput.ReadToEndAsync()
        $vcfErrors=$vcfProbe.StandardError.ReadToEndAsync()
        if (-not $vcfProbe.WaitForExit(15000)) {
            $vcfProbe.Kill()
            throw 'BLOCKED: Docker engine probe timed out. Finish Docker Desktop startup and any required Windows restart.'
        }
        if ($vcfProbe.ExitCode -ne 0) {throw 'BLOCKED: the Docker engine is unavailable. Start Docker Desktop, complete its first-launch agreement and restart Windows if WSL installation requested it.'}
        $vcfInfo=($vcfOutput.GetAwaiter().GetResult() | ConvertFrom-Json)
        [void]$vcfErrors.GetAwaiter().GetResult()
    } finally {$vcfProbe.Dispose()}
    if ($vcfInfo.OSType -ne 'linux') {throw 'BLOCKED: switch Docker Desktop to Linux containers.'}
    if ($vcfInfo.Architecture -notin @('x86_64','amd64')) {throw 'BLOCKED: this private runtime qualification requires an x86-64 Linux Docker engine.'}
    & $Docker compose version
    if ($LASTEXITCODE -ne 0) {throw 'BLOCKED: Docker Compose is unavailable.'}
    & $Docker buildx version
    if ($LASTEXITCODE -ne 0) {throw 'BLOCKED: Docker Buildx is unavailable.'}
    Write-Host ('Docker Linux engine '+$vcfInfo.ServerVersion+'; architecture '+$vcfInfo.Architecture)
}
function Resolve-VcfLinuxInputs([string]$Inputs) {
    if (-not $Inputs -or -not (Test-Path -LiteralPath $Inputs -PathType Container)) {throw 'BLOCKED: pass -Inputs with the authorized private Linux input directory.'}
    $vcfInputs=(Resolve-Path -LiteralPath $Inputs).Path
    foreach ($vcfFile in @('input-manifest.json','SHA256SUMS','server/bedrock_server')) {
        if (-not (Test-Path -LiteralPath (Join-Path $vcfInputs $vcfFile) -PathType Leaf)) {throw ('BLOCKED: missing private input '+$vcfFile)}
    }
    return $vcfInputs
}
