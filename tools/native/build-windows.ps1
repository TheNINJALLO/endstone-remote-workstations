param([ValidateSet('windows-release','windows-debug')][string]$Preset='windows-release')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$vcfRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
 $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
 if (-not (Test-Path -LiteralPath $vswhere)) { throw 'MSVC Build Tools are required.' }
 $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
 $devShell=Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
 Import-Module $devShell
 Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
 if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'The MSVC development environment could not be initialized.' }
 $env:PATH=(Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin')+';'+(Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja')+';'+$env:PATH
}
Push-Location $vcfRoot
try {
 cmake --preset $Preset
 if($LASTEXITCODE -ne 0){throw 'CMake configure failed'}
 cmake --build --preset $Preset --parallel 4
 if($LASTEXITCODE -ne 0){throw 'Native build failed'}
 ctest --preset $Preset
 if($LASTEXITCODE -ne 0){throw 'Native tests failed'}
 cmake --install "out/$Preset" --prefix "dist/$Preset"
 if($LASTEXITCODE -ne 0){throw 'Artifact export failed'}
} finally {Pop-Location}
