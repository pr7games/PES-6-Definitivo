$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
$projectRoot = Split-Path $sourceRoot -Parent
$patched = Join-Path $sourceRoot 'native/replay-runtime/rexruntime.dll'
if (!(Test-Path -LiteralPath $patched)) { throw 'Primero ejecuta node scripts/build-replay-runtime.cjs' }
if (Get-Process pes6 -ErrorAction SilentlyContinue | Where-Object {
    $_.Path -and $_.Path.StartsWith($projectRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)
}) { throw 'Cierra PES 6 antes de actualizar su runtime.' }
$destinations = @(
    (Join-Path $sourceRoot 'native/build/rexruntime.dll'),
    (Join-Path $projectRoot 'bin/native/build/rexruntime.dll'),
    (Join-Path $projectRoot 'bin/native/build/Backends/Vulkan/rexruntime.dll'),
    (Join-Path $projectRoot 'bin/native/build/Backends/D3D12/rexruntime.dll')
)
$baseline = Join-Path $sourceRoot 'native/replay-runtime/baseline/rexruntime.dll'
foreach ($destination in $destinations) {
    if (!(Test-Path -LiteralPath (Split-Path $destination -Parent))) { continue }
    $backup = "$destination.before-replay-fix"
    if (!(Test-Path -LiteralPath $backup)) {
        if (Test-Path -LiteralPath $baseline) {
            Copy-Item -LiteralPath $baseline -Destination $backup
        } elseif (Test-Path -LiteralPath $destination) {
            Copy-Item -LiteralPath $destination -Destination $backup
        }
    }
    Copy-Item -LiteralPath $patched -Destination $destination -Force
    if ((Get-FileHash -LiteralPath $patched).Hash -ne (Get-FileHash -LiteralPath $destination).Hash) {
        throw "No coincide el runtime desplegado: $destination"
    }
}
Write-Host 'Runtime de repeticiones instalado en PES 6 (Vulkan y D3D12).'
