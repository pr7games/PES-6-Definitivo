function Get-Pes6RuntimeFiles([string]$Root) {
    $files=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    function Add-File([string]$relative) {
        if(!(Test-Path -LiteralPath (Join-Path $Root $relative) -PathType Leaf)){throw "Falta recurso de juego: $relative"}
        [void]$files.Add($relative)
    }
    function Add-Matching([string]$folder,[string]$pattern) {
        $directory=Join-Path $Root $folder
        if(!(Test-Path -LiteralPath $directory)){return}
        foreach($f in Get-ChildItem -LiteralPath $directory -File -Recurse){
            $local=$f.FullName.Substring($directory.Length+1).Replace('\','/')
            if($local -match $pattern){Add-File ($f.FullName.Substring($Root.Length+1))}
        }
    }
    foreach($f in @('PES 6.exe','PES 6 Launcher.exe','bin\probar-pes6.ps1','bin\display-config.ps1','docs\PES6-NOTICE.txt','docs\THIRD_PARTY_RENDERER.md',
        'bin\scripts\start-native-runtime.ps1')){Add-File $f}
    Add-Matching 'bin\launcher-assets' '.'
    Add-File '.project-layout.json'
    Add-File 'bin\Ensure-Layout.ps1'
    # Neural Rendering is not shipped in this release. Sources remain in development.
    Add-Matching 'data\audio' '^net/net-\d+\.wav$'
    Add-Matching 'data\balls' '(?:\.bundle$|^selection\.txt$)'
    foreach($m in [regex]::Matches([IO.File]::ReadAllText((Join-Path $Root 'source\native\src\cloth_variant_map.h')),'"([^"]+\.bin)"')){
        Add-File ('data\cloth\models\'+$m.Groups[1].Value)
    }
    Add-Matching 'data\graphics\soft-crowd' '(?:\.we$|^sources\.txt$)'
    Add-Matching 'data\loading-logo\packages' '\.(?:bin|txt)$'
    Add-Matching 'data\loading-logo' '^\d{2}\.we$'
    Add-Matching 'data\menu-carousel' '^(?:0[1-9]|10)\.we$'
    foreach($name in @('asset-runtime.txt','openplay-frequency.txt','openplay-shot-trial.txt','player-appearance.txt','stadiums.txt','team-info.txt','team-management.txt')){
        $relative='data\editor\'+$name
        if(Test-Path -LiteralPath (Join-Path $Root $relative)){Add-File $relative}
    }
    Add-Matching 'data\editor\assets' '\.bin$'
    Add-Matching 'data\editor\boots' '(?:\.(?:we|rgba)$|^packages/[^/]+\.bin$|^(?:names|hidden-in-game)\.txt$)'
    Add-Matching 'data\editor\boot-previews' '\.(?:we|rgba)$'
    Add-Matching 'data\editor\coaches' '\.we$'
    Add-Matching 'data\editor\motion-recipes' '\.(?:p6motion|motion|bundle|bin|txt)$'
    Add-Matching 'data\editor\kits\packages' '\.bin$'
    Add-Matching 'data\editor\kit-previews' '\.(?:bin|txt)$'
    Add-Matching 'data\editor\competitions' '^(?:(?:logos|names|textures)\.txt|packages/[^/]+\.bin|textures/[^/]+\.we|presentation/.*\.(?:bin|we|txt))$'
    Add-Matching 'data\patches\fonts' '\.bin$'
    Add-Matching 'data\patches\kits\fonts' '\.bin$'
    Add-Matching 'data\patches\kits\packages' '\.bin$'
    Add-Matching 'data\patches\kits\previews\hd' '\.bin$'
    Add-Matching 'data\patches\premier-clubs' '^(?:packages/[^/]+\.bin|\d+-\d+\.we|emblems_64x64\.pack)$'
    foreach($venue in @('konami','wembley')){
        Add-Matching ('data\stadiums\'+$venue) '^(?:(?:variant|literal)-[0-5](?:-winter)?|lighting-\d+|preview)\.bundle$'
    }
    Add-Matching 'data\stadiums\global-adboards' '^sources\.txt$'
    Add-Matching 'data\stadiums\native-lighting' '^lighting-\d+\.bundle$'
    Add-Matching 'data\stadiums\pes2009-pitches' '^stadium-[0-7]/variant-(?:[0-5]|dusk-wet)(?:-winter)?\.bundle$'
    Add-Matching 'data\stadiums\sunset' '^(?:lighting-\d+|stadium-[0-7]-sky(?:-winter)?)\.bundle$'
    foreach($relative in @('data\stadiums\lighting-gain-percent.txt','data\stadiums\konami\preferred-hour.txt','data\camera-wide.txt')){
        if(Test-Path -LiteralPath (Join-Path $Root $relative)){Add-File $relative}
    }
    $files | Sort-Object
}
