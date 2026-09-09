# Materialize the closed native build context as regular files. BuildKit can
# reject OneDrive cloud reparse points even when PowerShell can read the file.
Set-StrictMode -Version Latest
function New-VcfBuildContext {
 param([Parameter(Mandatory=$true)][string]$Root)
 $vcfSource=(Resolve-Path -LiteralPath $Root).Path
 if((Get-Item -LiteralPath $vcfSource -Force).LinkType) {throw 'Build-context root must not be a link or junction.'}
 $vcfScratch=Join-Path ([System.IO.Path]::GetTempPath()) ('onistone-vcf-build-'+[guid]::NewGuid().ToString('N'))
 $vcfContext=[pscustomobject]@{Path=$vcfScratch;Source=$vcfSource;Files=0;Bytes=[long]0}
 New-Item -ItemType Directory -Path $vcfScratch -ErrorAction Stop | Out-Null
 try {
  $vcfPending=New-Object 'System.Collections.Generic.Queue[string]'
  $vcfChecked=New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
  foreach($vcfRelative in @('.dockerignore','Dockerfile','CMakeLists.txt','CMakePresets.json','cmake','include','framework','third_party','tests/native','examples/native','tools/native','research/original-ui-catalog.json','research/original-ui-catalog.sha256')) {$vcfPending.Enqueue($vcfRelative)}
  while($vcfPending.Count) {
   $vcfRelative=$vcfPending.Dequeue()
   $vcfItem=Get-Item -LiteralPath (Join-Path $vcfSource $vcfRelative) -Force -ErrorAction Stop
   $vcfAncestor=Split-Path -Parent $vcfItem.FullName
   while($vcfAncestor -ne $vcfSource) {
    if(-not $vcfChecked.Add($vcfAncestor)) {break}
    if((Get-Item -LiteralPath $vcfAncestor -Force -ErrorAction Stop).LinkType) {throw "Build-context ancestor links are refused: $vcfRelative"}
    $vcfAncestor=Split-Path -Parent $vcfAncestor
   }
   # Cloud files have ReparsePoint attributes but no LinkType. Refuse actual
   # links/junctions so a source entry cannot escape the selected public tree.
   if($vcfItem.LinkType) {throw "Build-context links are refused: $vcfRelative"}
   $vcfDestination=Join-Path $vcfScratch $vcfRelative
   if($vcfItem.PSIsContainer) {
    New-Item -ItemType Directory -Path $vcfDestination -Force -ErrorAction Stop | Out-Null
    foreach($vcfChild in Get-ChildItem -LiteralPath $vcfItem.FullName -Force -ErrorAction Stop) {$vcfPending.Enqueue((Join-Path $vcfRelative $vcfChild.Name))}
   } else {
    if($vcfItem.Length -gt 16MB -or $vcfContext.Files -ge 4096 -or $vcfContext.Bytes+$vcfItem.Length -gt 128MB) {throw 'Native build context exceeds the declared source bounds.'}
    $vcfParent=Split-Path -Parent $vcfDestination
    New-Item -ItemType Directory -Path $vcfParent -Force -ErrorAction Stop | Out-Null
    # Reading content hydrates a cloud file; creating a new file does not carry
    # its cloud reparse metadata into the Docker context.
    $vcfBytes=[System.IO.File]::ReadAllBytes($vcfItem.FullName)
    if($vcfBytes.Length -ne $vcfItem.Length) {throw "Source length changed during staging: $vcfRelative"}
    [System.IO.File]::WriteAllBytes($vcfDestination,$vcfBytes)
    $vcfContext.Files++;$vcfContext.Bytes+=$vcfBytes.Length
   }
  }
  return $vcfContext
 } catch {Remove-VcfBuildContext $vcfContext;throw}
}
function Remove-VcfBuildContext {
 param($Context)
 if($null -eq $Context) {return}
 $vcfTemporary=[System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\','/')
 $vcfTarget=[System.IO.Path]::GetFullPath($Context.Path)
 if((Split-Path -Parent $vcfTarget) -ne $vcfTemporary -or (Split-Path -Leaf $vcfTarget) -notmatch '^onistone-vcf-build-[0-9a-f]{32}$') {throw 'Refusing cleanup outside the exact owned temporary build directory.'}
 if(Test-Path -LiteralPath $vcfTarget) {
  $vcfItem=Get-Item -LiteralPath $vcfTarget -Force
  if($vcfItem.LinkType -or -not $vcfItem.PSIsContainer) {throw 'Temporary build directory changed type; cleanup refused.'}
  Remove-Item -LiteralPath $vcfTarget -Recurse -Force -ErrorAction Stop
 }
}
function New-VcfComposeContextOverride {
 param([Parameter(Mandatory=$true)]$Context,[Parameter(Mandatory=$true)][ValidateSet('abi-lab','smoke')][string]$Service)
 $vcfServices=@{};$vcfServices[$Service]=@{build=@{context=$Context.Path}}
 $vcfOverride=Join-Path $Context.Path 'compose-context.json'
 [System.IO.File]::WriteAllText($vcfOverride,(@{services=$vcfServices}|ConvertTo-Json -Depth 5),(New-Object System.Text.UTF8Encoding($false)))
 return $vcfOverride
}
