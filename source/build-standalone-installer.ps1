param(
    [string]$Payload = (Join-Path $PSScriptRoot 'dist\PES 6 DEFINITIVO Installer 0.3.1\payload'),
    [string]$Destination = (Join-Path $PSScriptRoot 'dist\PES6-DEFINITIVO-Setup-0.3.1.exe'),
    [string]$CertificateThumbprint
)
$ErrorActionPreference = 'Stop'
$Payload = (Resolve-Path -LiteralPath $Payload).Path
foreach ($relative in @('bin\native\build\pes6.exe','PES 6 Launcher.exe','port-manifest.json','game-patches\manifest.json')) {
    if (!(Test-Path -LiteralPath (Join-Path $Payload $relative))) { throw "Falta componente: $relative" }
}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = Join-Path $PSScriptRoot ('dist\pes6-payload-'+[Guid]::NewGuid().ToString('N')+'.zip')
$previous = $env:PES6_INSTALLER_PAYLOAD
try {
    [IO.Compression.ZipFile]::CreateFromDirectory($Payload,$archive,[IO.Compression.CompressionLevel]::Optimal,$false)
    if ((Get-Item -LiteralPath $archive).Length -gt 1800MB) { throw 'El payload supera el limite del EXE integrado. Usa el paquete de instalador con carpeta payload.' }
    $env:PES6_INSTALLER_PAYLOAD = $archive
    & (Join-Path $PSScriptRoot 'build-installer.cmd')
    if ($LASTEXITCODE -ne 0) { throw 'Fallo la compilacion integrada.' }
    New-Item -ItemType Directory -Force -Path (Split-Path $Destination) | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Instalador PES 6.exe') -Destination $Destination -Force
    if ($CertificateThumbprint) {
        $cert = Get-Item -LiteralPath ('Cert:\CurrentUser\My\'+$CertificateThumbprint)
        $signature = Set-AuthenticodeSignature -FilePath $Destination -Certificate $cert -HashAlgorithm SHA256 -TimestampServer 'http://timestamp.digicert.com'
        if ($signature.Status -ne 'Valid') { throw "Firma invalida: $($signature.StatusMessage)" }
    }
    ((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($Destination)) |
        Set-Content -LiteralPath ($Destination+'.sha256') -Encoding ASCII
    Write-Host "Instalador integrado listo: $Destination"
} finally {
    $env:PES6_INSTALLER_PAYLOAD = $previous
    if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
}
