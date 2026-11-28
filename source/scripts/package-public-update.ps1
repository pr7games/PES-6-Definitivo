param([Parameter(Mandatory=$true)][string]$ProjectRoot,[string]$Version='0.3',[string]$PreviousInventory)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$releaseRoot=Join-Path $repoRoot ('dist\public-'+$Version)
$payload=Join-Path $releaseRoot 'payload'
if(Test-Path -LiteralPath $payload){throw 'El payload ya existe; usar un destino limpio.'}
New-Item -ItemType Directory -Path $payload -Force | Out-Null
. (Join-Path $ProjectRoot 'source\scripts\select-game-runtime.ps1')
$selected=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($relative in Get-Pes6RuntimeFiles $ProjectRoot){[void]$selected.Add($relative.Replace('\','/'))}
foreach($relative in @('bin/PES 6.exe','bin/PES 6 Launcher.exe')){[void]$selected.Add($relative)}
foreach($name in @('pes6.exe','rexruntime.dll','rexruntimerd.dll','rexgpu-xenos.dll','rexgpu-xenosrd.dll','TracyClientrd.dll')){[void]$selected.Add('bin/native/build/'+$name)}
foreach($api in @('Vulkan','D3D12')){
 $base='bin/native/build/Backends/'+$api
 $backend=Get-Content -LiteralPath (Join-Path $ProjectRoot ($base+'/manifest.json')) -Raw | ConvertFrom-Json
 [void]$selected.Add($base+'/manifest.json')
 foreach($entry in $backend.Files){if([IO.Path]::GetFileName($entry.Name) -ne $entry.Name){throw 'Ruta de backend no permitida'};[void]$selected.Add($base+'/'+$entry.Name)}
}
foreach($name in @('pes6.exe','rexruntime.dll','rexgpu-xenos.dll','amd_fidelityfx_dx12.dll','pes6-native-extension.dll')){[void]$selected.Add('bin/native-renderer/'+$name)}
$fileInventory=@()
$previous=$null
if($PreviousInventory){$previous=Get-Content -LiteralPath $PreviousInventory -Raw | ConvertFrom-Json}
$previousLayout=$previous.files | Where-Object path -eq '.project-layout.json' | Select-Object -First 1
foreach($relative in ($selected | Sort-Object)){
 if($relative -match '(^|/)(save|backups?|tests?|analysis|cache|logs|exports?|trace|defaults)(/|$)|\.before|\.bak$|\.pdb$|trace\.enabled$|(^|/)editor-src/|^bin/editor/|PES 6 Editor\.exe'){throw "Recurso de desarrollo seleccionado: $relative"}
 $source=Join-Path $ProjectRoot $relative
 # Preserve the installed layout marker if it already belongs to this base.
 if($relative -eq '.project-layout.json' -and $previousLayout -and
    (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -eq $previousLayout.sha256){continue}
 if($relative -in @('PES 6.exe','PES 6 Launcher.exe','bin/PES 6.exe','bin/PES 6 Launcher.exe')){$source=Join-Path $repoRoot ('dist/launcher-'+$Version+'/PES 6 Launcher.exe')}
 $item=Get-Item -LiteralPath $source -Force
 if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Enlace no permitido: $relative"}
 $target=Join-Path $payload $relative
 New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
 Copy-Item -LiteralPath $source -Destination $target
 $fileInventory+=@{path=$relative;size=$item.Length;sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()}
}
Copy-Item -LiteralPath (Join-Path $releaseRoot 'game-patches') -Destination $payload -Recurse
$utf8=New-Object Text.UTF8Encoding($false)
$remove=@()
if($previous){
 $remove=@($previous.files | Where-Object {-not $selected.Contains($_.path)} | ForEach-Object {$_.path})
}
$inventory=@{version=$Version;files=$fileInventory;remove=$remove}
[IO.File]::WriteAllText((Join-Path $payload 'update-inventory.json'),($inventory|ConvertTo-Json -Depth 6),$utf8)
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$assets=@();$archive=$null;$bytes=0;$part=0;$zipPath=$null
try{
 foreach($file in Get-ChildItem -LiteralPath $payload -Force -Recurse -File | Sort-Object FullName){
  if($archive -and $bytes+$file.Length -gt 900MB){$archive.Dispose();$archive=$null;$assets+=Get-Item -LiteralPath $zipPath}
  if(-not $archive){$part++;$zipPath=Join-Path $releaseRoot ('PES6-Definitivo-'+$Version+'-part'+$part.ToString('D2')+'.zip');$archive=[IO.Compression.ZipFile]::Open($zipPath,[IO.Compression.ZipArchiveMode]::Create);$bytes=0}
  $relative=$file.FullName.Substring($payload.Length+1).Replace('\','/')
  [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,$relative,[IO.Compression.CompressionLevel]::Optimal)|Out-Null
  $bytes+=$file.Length
 }
}finally{if($archive){$archive.Dispose();$assets+=Get-Item -LiteralPath $zipPath}}
$packages=@($assets | ForEach-Object {@{name=$_.Name;url='https://github.com/pr7games/PES-6-Definitivo/releases/download/v'+$Version+'/'+$_.Name;size=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}})
$feed=@{schemaVersion=2;version=$Version;minimumLauncher='0.2.1';packages=$packages}
[IO.File]::WriteAllText((Join-Path $releaseRoot 'latest.json'),($feed|ConvertTo-Json -Depth 6),$utf8)
Write-Output ($packages | Select-Object name,size | Format-Table | Out-String)
Write-Output ('Archivos runtime: '+$fileInventory.Count)
