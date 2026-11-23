param(
 [string]$OriginalGame='F:\Xenia X360\Games\PES 6',
 [string]$Destination=(Join-Path $PSScriptRoot 'dist\PES 6 DEFINITIVO Installer 0.3.1'),
 [string]$PreparedGamePatches
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(Test-Path -LiteralPath $Destination){throw 'El destino ya existe; usa una carpeta nueva.'}
& (Join-Path $PSScriptRoot 'build-installer.cmd')
if($LASTEXITCODE -ne 0){throw 'Fallo la compilacion del instalador.'}
$payload=Join-Path $Destination 'payload'
New-Item -ItemType Directory -Path $payload -Force|Out-Null
function Copy-ReleaseFile([string]$relative){
 $source=Join-Path $root $relative
 if(-not (Test-Path -LiteralPath $source -PathType Leaf)){throw "Falta componente: $relative"}
 $item=Get-Item -LiteralPath $source -Force
 if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "No distribuir enlaces: $relative"}
 if($relative.Replace('\','/') -match '(^|/)(save|source|tests?|analysis|backups?|defaults|cache|logs|trace)(/|$)|^bin/editor/|PES 6 Editor\.exe|trace\.enabled$'){throw "Componente excluido: $relative"}
 $target=Join-Path $payload $relative
 New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force|Out-Null
 Copy-Item -LiteralPath $source -Destination $target
}
. (Join-Path $PSScriptRoot 'scripts\select-game-runtime.ps1')
foreach($relative in Get-Pes6RuntimeFiles $root){if($relative -notmatch '^docs[\\/]'){Copy-ReleaseFile $relative}}
foreach($relative in @('bin\PES 6.exe','bin\PES 6 Launcher.exe')){Copy-ReleaseFile $relative}
foreach($file in @('pes6.exe','rexruntime.dll','rexruntimerd.dll','rexgpu-xenos.dll','rexgpu-xenosrd.dll','TracyClientrd.dll')){Copy-ReleaseFile ('bin\native\build\'+$file)}
foreach($api in @('D3D12','Vulkan')){
 $relative='bin\native\build\Backends\'+$api
 $manifest=Get-Content -LiteralPath (Join-Path $root ($relative+'\manifest.json')) -Raw|ConvertFrom-Json
 Copy-ReleaseFile ($relative+'\manifest.json')
 foreach($entry in $manifest.Files){if([IO.Path]::GetFileName($entry.Name) -ne $entry.Name){throw 'Ruta de backend invalida'};Copy-ReleaseFile ($relative+'\'+$entry.Name)}
}
foreach($file in @('pes6.exe','rexruntime.dll','rexgpu-xenos.dll','amd_fidelityfx_dx12.dll','pes6-native-extension.dll')){Copy-ReleaseFile ('bin\native-renderer\'+$file)}
# Required attribution remains with the binaries; no developer documentation.
$notice=@()
foreach($relative in @('docs\PES6-NOTICE.txt','docs\THIRD_PARTY_RENDERER.md')){$notice+=[IO.File]::ReadAllText((Join-Path $root $relative))}
$utf8=New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $payload 'bin\PES6-NOTICE.txt'),($notice -join [Environment]::NewLine),$utf8)
$exe=Join-Path $PSScriptRoot 'Instalador PES 6.exe'
$patchArgs='--build-patches "'+$OriginalGame+'" "'+(Join-Path $root 'game')+'" "'+(Join-Path $payload 'game-patches')+'"'
if($PreparedGamePatches){
 Copy-Item -LiteralPath $PreparedGamePatches -Destination (Join-Path $payload 'game-patches') -Recurse
}else{
 $process=Start-Process -FilePath $exe -ArgumentList $patchArgs -WindowStyle Hidden -PassThru -Wait
 if($process.ExitCode -ne 0){throw 'Fallo la generacion de parches. Revisa installer-build-error.txt.'}
}
$patches=Get-Content -LiteralPath (Join-Path $payload 'game-patches\manifest.json') -Raw|ConvertFrom-Json
$xex=$patches|Where-Object Name -eq 'default.xex'
if(-not $xex){throw 'Falta default.xex en el manifiesto'}
$metadata=@{Version='0.3.1';SourceDefaultXexSHA256=$xex.OriginalSHA256;GameAssetsIncluded=$false;GamePatchesIncluded=$true}
[IO.File]::WriteAllText((Join-Path $payload 'port-manifest.json'),($metadata|ConvertTo-Json),$utf8)
Copy-Item -LiteralPath $exe -Destination (Join-Path $Destination 'Instalador PES 6 DEFINITIVO.exe')
Write-Output ('Paquete preparado: '+$Destination)
