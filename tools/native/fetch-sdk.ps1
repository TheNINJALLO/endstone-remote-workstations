param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
$dest=[IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Path $dest -Force | Out-Null
$sdk=Join-Path $dest 'endstone'
if(-not(Test-Path -LiteralPath (Join-Path $sdk '.git'))){
 git -c core.autocrlf=false clone --no-checkout https://github.com/EndstoneMC/endstone.git $sdk
 if($LASTEXITCODE -ne 0){throw 'SDK clone failed'}
}
git -C $sdk -c core.autocrlf=false checkout --detach 8f84d6f5b556916597ed5b6b71329b2ed3ca8fc8
if($LASTEXITCODE -ne 0){throw 'SDK checkout failed'}
$expected=Join-Path $dest 'expected-lite/include/nonstd'
New-Item -ItemType Directory -Path $expected -Force | Out-Null
$header=Join-Path $expected 'expected.hpp'
Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/martinmoene/expected-lite/v0.9.0/include/nonstd/expected.hpp' -OutFile $header
if((Get-FileHash -LiteralPath $header -Algorithm SHA256).Hash -ne 'bbab48a56231800c21373be71e47f1b9d8d4e8e10e2c9d3a51c4f5f850104086'){throw 'expected-lite hash mismatch'}
