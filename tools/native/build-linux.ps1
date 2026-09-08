$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$vcfRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $vcfRoot
try {
 if (-not (Get-Command docker -ErrorAction SilentlyContinue)) { throw 'BLOCKED: Docker CLI/engine is unavailable. No system installation was attempted.' }
 $os = docker info --format '{{.OSType}}'
 if ($LASTEXITCODE -ne 0 -or $os -ne 'linux') { throw 'BLOCKED: a reachable Linux Docker engine is required.' }
 docker compose version
 if ($LASTEXITCODE -ne 0) { throw 'Docker Compose is required.' }
 docker buildx version
 if ($LASTEXITCODE -ne 0) { throw 'Docker Buildx is required.' }
 docker buildx build --platform linux/amd64 --target artifact-export --output type=local,dest=dist/linux-x64-dev .
 if ($LASTEXITCODE -ne 0) { throw 'Linux container build failed.' }
} finally { Pop-Location }
