param([string]$GitHubCli,[switch]$CheckRemoteManifest)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Web.Extensions
Add-Type -AssemblyName System.IO.Compression.FileSystem
$assembly=[Reflection.Assembly]::LoadFile((Join-Path $PSScriptRoot 'dist\launcher-0.1\PES 6 Launcher.exe'))
$updater=$assembly.GetType('FIFA08Launcher.PortUpdater',$true)
function Invoke-Updater([string]$Name,[object[]]$Arguments) {
    $invokeArguments=New-Object object[] $Arguments.Length
    for($i=0;$i -lt $Arguments.Length;$i++) {if($null -ne $Arguments[$i]){$invokeArguments[$i]=$Arguments[$i].PSObject.BaseObject}}
    $updater.GetMethod($Name,[Reflection.BindingFlags]'Static,Public').Invoke($null,$invokeArguments)
}
function Assert-Test([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
$version=$updater.GetField('CurrentVersion').GetRawConstantValue()
Assert-Test ($version -eq '0.1') 'La version local no es 0.1'
$newAssembly=[Reflection.Assembly]::LoadFile((Join-Path $PSScriptRoot 'dist\launcher-0.2\PES 6 Launcher.exe'))
Assert-Test ($newAssembly.GetType('FIFA08Launcher.PortUpdater').GetField('CurrentVersion').GetRawConstantValue() -eq '0.2') 'El paquete no es 0.2'
Assert-Test ((Invoke-Updater ParseVersion @('0.2')) -gt (Invoke-Updater ParseVersion @('0.1'))) 'No detecta 0.2 > 0.1'
Assert-Test ((Invoke-Updater ParseVersion @('v0.2')) -eq (Invoke-Updater ParseVersion @('0.2.0'))) 'Versiones equivalentes no coinciden'
$fixture=Join-Path $PSScriptRoot ('dist\verification-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'bin'),(Join-Path $fixture 'save'),(Join-Path $fixture 'mods'),(Join-Path $fixture 'data\config') -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $fixture 'bin\probar-pes6.ps1'),'fixture only')
[IO.File]::WriteAllText((Join-Path $fixture 'save\preservar.txt'),'save unchanged')
[IO.File]::WriteAllText((Join-Path $fixture 'mods\preservar.txt'),'mods unchanged')
if($GitHubCli){[IO.File]::WriteAllText((Join-Path $fixture 'data\config\updater-local.json'),(@{ghExe=$GitHubCli}|ConvertTo-Json))}
$serializer=New-Object Web.Script.Serialization.JavaScriptSerializer
$manifest=$serializer.Deserialize([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'updates\test\manifest.json')),$assembly.GetType('FIFA08Launcher.UpdateManifest'))
Invoke-Updater Validate @($manifest)
if($CheckRemoteManifest){$remote=Invoke-Updater Check @($fixture);Assert-Test ($remote.version -eq '0.2' -and $remote.enabled) 'El feed remoto no ofrece 0.2'}
$package=Join-Path $fixture 'downloaded.zip'
Invoke-Updater DownloadPackage @($fixture,$manifest,$package)
foreach($relative in $updater.GetField('LauncherFiles').GetValue($null)) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'dist\launcher-0.1\PES 6 Launcher.exe') -Destination (Join-Path $fixture $relative)
}
$originalHash=(Get-FileHash -LiteralPath (Join-Path $fixture 'PES 6 Launcher.exe')).Hash
$injected=[Action[int]]{param($count) if($count -eq 2){throw 'Injected copy failure'}}
$rollbackFailed=$false
try{Invoke-Updater Apply @($package,$fixture,(Join-Path $fixture 'rollback-injected'),$injected)}catch{$rollbackFailed=$true}
Assert-Test $rollbackFailed 'No se detecto fallo inyectado'
foreach($relative in $updater.GetField('LauncherFiles').GetValue($null)) {
    Assert-Test ((Get-FileHash -LiteralPath (Join-Path $fixture $relative)).Hash -eq $originalHash) 'Rollback incompleto'
}
$badHashRejected=$false
try{Invoke-Updater Verify @($package,('0'*64))}catch{$badHashRejected=$true}
Assert-Test $badHashRejected 'Se acepto un SHA-256 incorrecto'
$unsafe=Join-Path $fixture 'unsafe.zip'
$archive=[IO.Compression.ZipFile]::Open($unsafe,[IO.Compression.ZipArchiveMode]::Create)
try{foreach($entryName in @('../escape.exe','PES 6.exe','bin/PES 6 Launcher.exe','bin/PES 6.exe')){[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $fixture 'PES 6.exe'),$entryName) | Out-Null}}finally{$archive.Dispose()}
$unsafeRejected=$false
try{Invoke-Updater Apply @($unsafe,$fixture,(Join-Path $fixture 'rollback-unsafe'),$null)}catch{$unsafeRejected=$true}
Assert-Test $unsafeRejected 'Se acepto una ruta no permitida'
Assert-Test ((Get-FileHash -LiteralPath (Join-Path $fixture 'PES 6 Launcher.exe')).Hash -eq $originalHash) 'El paquete rechazado modifico el launcher'
Invoke-Updater Apply @($package,$fixture,(Join-Path $fixture 'rollback-success'),$null)
$newHash=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'dist\launcher-0.2\PES 6 Launcher.exe')).Hash
foreach($relative in $updater.GetField('LauncherFiles').GetValue($null)) {
    Assert-Test ((Get-FileHash -LiteralPath (Join-Path $fixture $relative)).Hash -eq $newHash) 'No se instalo el launcher 0.2'
}
Assert-Test ([IO.File]::ReadAllText((Join-Path $fixture 'save\preservar.txt')) -eq 'save unchanged') 'Partida modificada'
Assert-Test ([IO.File]::ReadAllText((Join-Path $fixture 'mods\preservar.txt')) -eq 'mods unchanged') 'Mod modificado'
Write-Output 'PASS: versiones, descarga privada, SHA-256, rollback, rutas y actualizacion aislada 0.1 -> 0.2; mods/save conservados.'
Write-Output $fixture
