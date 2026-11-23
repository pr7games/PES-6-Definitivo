param([switch]$UpdatePackage, [string]$OutputDirectory, [string]$Icon)
$ErrorActionPreference='Stop'
if(-not $OutputDirectory){$OutputDirectory=Join-Path $PSScriptRoot ('dist\launcher-'+$(if($UpdatePackage){'0.3'}else{'0.2.1'}))}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$sourceDir=Join-Path $PSScriptRoot 'source\launcher-src'
$compilerArgs=@('/nologo','/codepage:65001','/target:winexe','/optimize+','/platform:anycpu',('/out:'+(Join-Path $OutputDirectory 'PES 6 Launcher.exe')),('/win32manifest:'+(Join-Path $sourceDir 'app.manifest')),'/reference:System.dll','/reference:System.Drawing.dll','/reference:System.Windows.Forms.dll','/reference:System.Core.dll','/reference:System.Web.Extensions.dll','/reference:System.IO.Compression.dll','/reference:System.IO.Compression.FileSystem.dll')
if($UpdatePackage){$compilerArgs+='/define:UPDATE_PACKAGE_BUILD'}
if($Icon){$compilerArgs+=('/win32icon:'+$Icon)}
$compilerArgs+=@(Get-ChildItem -LiteralPath $sourceDir -Filter '*.cs' -File | ForEach-Object {$_.FullName})
& $compiler @compilerArgs
if($LASTEXITCODE -ne 0){throw 'Fallo la compilacion del launcher'}
Copy-Item -LiteralPath (Join-Path $OutputDirectory 'PES 6 Launcher.exe') -Destination (Join-Path $OutputDirectory 'PES 6.exe') -Force
Write-Output $OutputDirectory
