param(
    [ValidateSet('legacy','native','performance','balanced','quality')][string]$FsrProfile='legacy',
    $KeyboardEnabled=$false,
    [switch]$CaptureOutput,
    [switch]$TraceGpu,
    [switch]$CaptureGpu,
    $AsyncShaders = $true,
    $EdramProtection = $true,
    $AchievementPopup = $true,
    $AchievementSound = $true,
    [string]$InterfaceLanguage = 'es',
    [switch]$PrepareOnly,
    [switch]$PlanOnly,
    [string]$TextLanguage = 'es',
    [string]$CommentaryLanguage = 'es',
    [string]$PostEffect = 'none',
    $VSync = $false,
    [ValidateSet('720','1080','1440','2160','1080_fsr','1440_fsr','2160_fsr')][string]$Resolution = '1080',
    [ValidateSet('window','fullscreen')][string]$DisplayMode = 'fullscreen',
    $ShowFps = $false,
    $EnhancedEffects = $false,
    $ImprovedRain = $null,
    $ImprovedParticles = $null,
    $PlayerDirt = $null,
    $SoftCrowd = $false,
    $RealisticKits = $false,
    $NeuralRendering = $false,
    [int]$FrameLimit = 60,
    [string]$GraphicsProfile = 'compatibility',
    [ValidateSet('Nativo','D3D12','Vulkan')][string]$GraphicsApi = 'Vulkan',
    [string]$BuildVersion = 'original',
    [string]$PacingMode = 'original',
    [int]$Anisotropy = -1,
    [switch]$NoWait
)
$ErrorActionPreference = 'Stop'
# Accept old shortcuts, but their saved high/unlimited limits no longer apply.
$FrameLimit = 60
$PacingMode = 'original'
. (Join-Path $PSScriptRoot 'display-config.ps1')

$cleanRoot = [IO.Path]::GetFullPath($PSScriptRoot)
while (!(Test-Path -LiteralPath (Join-Path $cleanRoot '.project-layout.json'))) {
    $parentRoot = Split-Path $cleanRoot -Parent
    if (!$parentRoot -or $parentRoot -eq $cleanRoot) { throw 'No se encontro la raiz del proyecto PES 6.' }
    $cleanRoot = $parentRoot
}
& (Join-Path $cleanRoot 'bin\Ensure-Layout.ps1') -ProjectRoot $cleanRoot
$exe = Join-Path $cleanRoot 'bin\native\build\pes6.exe'
if (!(Test-Path -LiteralPath $exe)) {
    $exe = Join-Path $cleanRoot 'native\build\pes6.exe'
}

$isVSync = ($VSync -eq $true -or [string]$VSync -in @('1','true','True','$true'))
$isShowFps = ($ShowFps -eq $true -or [string]$ShowFps -in @('1','true','True','$true'))
$isKeyboard = ($KeyboardEnabled -eq $true -or [string]$KeyboardEnabled -in @('1','true','True','$true'))
$isAsyncShaders = ($AsyncShaders -eq $true -or [string]$AsyncShaders -in @('1','true','True','$true'))
$isEdramProtection = ($EdramProtection -eq $true -or [string]$EdramProtection -in @('1','true','True','$true'))

$env:PES6_GRAPHICS_API = $GraphicsApi
$env:PES6_FPS_LIMIT = [string]$FrameLimit
$env:PES6_PACING = $PacingMode
$env:PES6_SHOW_FPS = if ($isShowFps) { '1' } else { '0' }
$isEnhancedEffects = ($EnhancedEffects -eq $true -or [string]$EnhancedEffects -in @('1','true','True','$true'))
function Resolve-EffectOption($effectValue) {
    if ($null -eq $effectValue) { return $isEnhancedEffects }
    return ($effectValue -eq $true -or [string]$effectValue -in @('1','true','True','$true'))
}
$isImprovedRain = Resolve-EffectOption $ImprovedRain
$isImprovedParticles = Resolve-EffectOption $ImprovedParticles
$isPlayerDirt = Resolve-EffectOption $PlayerDirt
$isSoftCrowd = ($SoftCrowd -eq $true -or [string]$SoftCrowd -in @('1','true','True','$true'))
$isEnhancedEffects = $isImprovedRain -or $isImprovedParticles -or $isPlayerDirt
$env:PES6_ENHANCED_EFFECTS = if ($isEnhancedEffects) { '1' } else { '0' }
$env:PES6_IMPROVED_RAIN = if ($isImprovedRain) { '1' } else { '0' }
$env:PES6_IMPROVED_PARTICLES = if ($isImprovedParticles) { '1' } else { '0' }
$env:PES6_PLAYER_DIRT = if ($isPlayerDirt) { '1' } else { '0' }
$env:PES6_SOFT_CROWD = if ($isSoftCrowd) { '1' } else { '0' }
$isRealisticKits = ($RealisticKits -eq $true -or [string]$RealisticKits -in @('1','true','True','$true'))
$env:PES6_REALISTIC_KITS = if ($isRealisticKits) { '1' } else { '0' }
# Reserved for a future release; retain the implementation without activating it.
$NeuralRendering = $false
$isNeuralRendering = $false
$env:PES6_TEXT_LANG = $TextLanguage
$env:PES6_COMM_LANG = if ($CommentaryLanguage -eq 'original') { $TextLanguage } else { $CommentaryLanguage }

$userLangId = switch ($TextLanguage.ToLowerInvariant()) {
    { $_ -in @('en','e','eng') } { 1; break }
    { $_ -in @('ja','j','jpn') } { 2; break }
    { $_ -in @('de','g','ger','deu') } { 3; break }
    { $_ -in @('fr','f','fre','fra') } { 4; break }
    { $_ -in @('es','s','spa') } { 5; break }
    { $_ -in @('it','i','ita') } { 6; break }
    { $_ -in @('ko','k','kor') } { 7; break }
    { $_ -in @('pt','p','por') } { 9; break }
    default { 5 }
}

if ($GraphicsApi -eq 'Nativo') {
    & (Join-Path $cleanRoot 'bin\scripts\start-native-runtime.ps1') -ProjectRoot $cleanRoot -Resolution $Resolution -DisplayMode $DisplayMode -FsrProfile $FsrProfile -PostEffect $PostEffect -TextLanguage $TextLanguage -UserLanguage $userLangId -Anisotropy $Anisotropy -VSync $VSync -KeyboardEnabled $KeyboardEnabled -NeuralRendering $NeuralRendering -PlanOnly:$PlanOnly -PrepareOnly:$PrepareOnly -NoWait:$NoWait
    return
}

# Alias synchronized from the current build, including builds made outside build-native.cmd.
$xenosExe = Join-Path (Split-Path $exe) 'pes6-x.exe'
if (!$PlanOnly -and (Get-Process -Name 'pes6','pes6-x' -ErrorAction SilentlyContinue | Where-Object {
    $_.Path -and $_.Path.StartsWith($cleanRoot.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)
})) { throw 'Cierra PES 6 antes de iniciar otra sesion.' }
if (!$PlanOnly -and (Test-Path -LiteralPath $exe)) { Copy-Item -LiteralPath $exe -Destination $xenosExe -Force }
if ((Test-Path -LiteralPath $xenosExe) -or !$PlanOnly) { $exe = $xenosExe }

$assets = Join-Path $cleanRoot 'game'
$userData = Join-Path $cleanRoot 'save'
$cache = Join-Path $cleanRoot 'data/cache'
$log = Join-Path $cleanRoot 'data/logs/pes6.log'
$frames = Join-Path $cleanRoot 'data/logs/frames.csv'

New-Item -ItemType Directory -Force -Path (Split-Path $log) | Out-Null
New-Item -ItemType Directory -Force -Path $userData, $cache | Out-Null

if (!(Test-Path -LiteralPath $exe)) { throw "Falta compilar el ejecutable: $exe" }
if (!(Test-Path -LiteralPath (Join-Path $assets 'default.xex'))) {
    throw "Faltan los recursos extraidos del juego en $assets"
}

# Ensure existing instances are closed before switching backends
if (!$PlanOnly -and (Get-Process -Name 'pes6','pes6-x' -ErrorAction SilentlyContinue | Where-Object {
    $_.Path -and $_.Path.StartsWith($cleanRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)
})) {
    throw 'Cierra PES 6 antes de iniciar otra sesion.'
}

# Deploy selected graphics backend (Vulkan or D3D12)
$backendDirectory = Join-Path (Split-Path $exe) ('Backends\' + $GraphicsApi)
if (!(Test-Path -LiteralPath $backendDirectory)) {
    $backendDirectory = Join-Path $cleanRoot ('bin\native\build\Backends\' + $GraphicsApi)
}
if (!(Test-Path -LiteralPath $backendDirectory)) {
    throw "No se encontro la carpeta del backend $GraphicsApi en $backendDirectory"
}

$manifestPath = Join-Path $backendDirectory 'manifest.json'
if (!$PlanOnly) {
if (Test-Path -LiteralPath $manifestPath) {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    foreach ($entry in $manifest.Files) {
        $srcFile = Join-Path $backendDirectory $entry.Name
        if (Test-Path -LiteralPath $srcFile) {
            Copy-Item -LiteralPath $srcFile -Destination (Join-Path (Split-Path $exe) $entry.Name) -Force
        }
    }
    Write-Host "Backend cargado: $GraphicsApi ($($manifest.Origin))"
} else {
    Copy-Item -LiteralPath "$backendDirectory\*" -Destination (Split-Path $exe) -Force
    Write-Host "Backend cargado: $GraphicsApi"
}
}

if ($PrepareOnly) { return }

# Resolution & FSR 1.0.2 calculation (aligned with FIFA 08 profiles and presets)
$outputResolution = $Resolution.Replace('_fsr','')
$useSpatialFsr = $Resolution.EndsWith('_fsr') -or $PostEffect -eq 'fsr'
if ($FsrProfile -ne 'legacy') { $useSpatialFsr = $FsrProfile -ne 'native' }

$displayConfig = Get-Pes6DisplayConfig -Resolution $outputResolution -DisplayMode $DisplayMode
$env:PES6_SELECTED_RESOLUTION = "$($displayConfig.BufferWidth) x $($displayConfig.BufferHeight)"
$env:FIFA08_SELECTED_RESOLUTION = $env:PES6_SELECTED_RESOLUTION

# Desired GPU input to FSR, separate from the game's internal render targets.
$table = @{
    '720'  = @{ native = @(1280,720,1);  performance = @(960,540,1);   balanced = @(1056,594,1); quality = @(1152,648,1) }
    '1080' = @{ native = @(1920,1080,2); performance = @(1280,720,1);  balanced = @(1536,864,1); quality = @(1728,972,1) }
    '1440' = @{ native = @(2560,1440,2); performance = @(1728,972,1);  balanced = @(1920,1080,2); quality = @(2112,1188,2) }
    '2160' = @{ native = @(3840,2160,3); performance = @(1920,1080,2); balanced = @(2560,1440,2); quality = @(3360,1890,3) }
}

$activeProfile = if ($FsrProfile -eq 'legacy') { if ($useSpatialFsr) { 'performance' } else { 'native' } } else { $FsrProfile }
$preset = $table[$outputResolution][$activeProfile]
if ($null -eq $preset) {
    $preset = if ($useSpatialFsr) { @(1280, 720, 1) } else { @($displayConfig.BufferWidth, $displayConfig.BufferHeight, $displayConfig.RenderScale) }
}

# Use the same direct FSR path as the working profiles. The three former
# affected presets now use distinct full-size guest/GPU inputs at scale1.
# Clear inherited staging values as well; no intermediate presentation pass.
$guestWidth = [int]$preset[0] / [int]$preset[2]
$guestHeight = [int]$preset[1] / [int]$preset[2]
$env:PES6_RENDER_WIDTH = [string]$guestWidth
$env:PES6_RENDER_HEIGHT = [string]$guestHeight
$env:PES6_FSR_STAGE_WIDTH = '0'
$env:PES6_FSR_STAGE_HEIGHT = '0'
$stageFsrInput = $false
$sceneWidth = $guestWidth * [int]$preset[2]
$sceneHeight = $guestHeight * [int]$preset[2]
$env:PES6_FRAME_DIAGNOSTICS = '0'

# Environment variables for FSR 1.0.2 EASU + RCAS and pc_overlay.h
$env:FIFA08_FSR_OUTPUT_WIDTH = if ($useSpatialFsr) { [string]$displayConfig.BufferWidth } else { '0' }
$env:FIFA08_FSR_OUTPUT_HEIGHT = if ($useSpatialFsr) { [string]$displayConfig.BufferHeight } else { '0' }
$env:FIFA08_FSR_INPUT_WIDTH = [string]$preset[0]
$env:FIFA08_FSR_INPUT_HEIGHT = [string]$preset[1]
$env:FIFA08_NATIVE_RCAS = if ($useSpatialFsr) { '0' } else { '1' }

$fullscreenArgument = if ($DisplayMode -eq 'fullscreen') { '--fullscreen' } else { '--no-fullscreen' }
# NR captures a borderless window reliably; exclusive startup can bypass it.
$env:PES6_NR_BORDERLESS = if ($isNeuralRendering -and $DisplayMode -eq 'fullscreen') { '1' } else { '0' }
if ($env:PES6_NR_BORDERLESS -eq '1') { $fullscreenArgument = '--no-fullscreen' }

$windowWidth = $displayConfig.WindowWidth
$windowHeight = $displayConfig.WindowHeight

$arguments = @(
    '--game_data_root', ('"' + $assets + '"'),
    '--user_data_root', ('"' + $userData + '"'),
    '--cache_root', ('"' + $cache + '"'),
    '--log_file', ('"' + $log + '"'),
    '--log_level', $(if ($TraceGpu) { 'info' } else { 'warning' }),
    ('--user_language=' + $userLangId),
    ('--swap_post_effect=' + $(if ($useSpatialFsr -or $PostEffect -eq 'fsr') { 'none' } else { $PostEffect })),
    ('--anisotropic_override=' + $Anisotropy),
    ('--mnk_mode=' + $isKeyboard.ToString().ToLowerInvariant()),
    ('--present_effect=' + $(if ($useSpatialFsr) { 'fsr' } else { 'bilinear' })),
    ('--present_fsr_sharpness_reduction=' + $(if ($useSpatialFsr) { '0' } else { '0.5' })),
    ('--async_shader_compilation=' + $isAsyncShaders.ToString().ToLowerInvariant()),
    ('--edram_ownership_no_wrap=' + $isEdramProtection.ToString().ToLowerInvariant()),
    '--host_coalesce_guest_paints=true',
    '--readback_resolve_skip_unchanged=true',
    '--clear_memory_page_state=true',
    ('--readback_resolve=' + $(if ($GraphicsApi -eq 'D3D12') { 'none' } else { 'fast' })),
    ('--window_width=' + $windowWidth),
    ('--window_height=' + $windowHeight),
    '--video_mode_width=1280',
    '--video_mode_height=720',
    ('--resolution_scale=' + $preset[2]),
    ('--draw_resolution_scale_x=' + $preset[2]),
    ('--draw_resolution_scale_y=' + $preset[2]),
    $fullscreenArgument,
    '--video_mode_refresh_rate=60',
    ('--vsync=' + $isVSync.ToString().ToLowerInvariant())
)

if ($isKeyboard) {
    $controlsFile = Join-Path $cleanRoot 'data\config\launcher-controls.json'
    if (!(Test-Path -LiteralPath $controlsFile)) {
        $controlsFile = Join-Path $cleanRoot 'launcher-controls.json'
    }
    if (Test-Path -LiteralPath $controlsFile) {
        $bindings = Get-Content -LiteralPath $controlsFile -Raw | ConvertFrom-Json
        foreach ($name in @('a','b','x','y','left_trigger','right_trigger','left_shoulder','right_shoulder','start','back','lstick_up','lstick_down','lstick_left','lstick_right','lstick_press','rstick_up','rstick_down','rstick_left','rstick_right','rstick_press','dpad_up','dpad_down','dpad_left','dpad_right')) {
            $key = [string]$bindings.$name
            if ($key -and $key -match '^[A-Za-z0-9+_,]+$') { $arguments += "--keybind_$name=$key" }
        }
    }
}

$renderTargetPath = if ($GraphicsApi -eq 'Vulkan') {
    if ($GraphicsProfile -eq 'compatibility') { 'fsi' } else { 'fbo' }
} else { if ($GraphicsProfile -eq 'compatibility') { 'rov' } else { 'rtv' } }

if ($GraphicsApi -eq 'Vulkan') {
    $arguments += ('--render_target_path_vulkan=' + $renderTargetPath)
    $arguments += @(
        '--host_present_from_non_ui_thread=false',
        '--vulkan_allow_present_mode_mailbox=true',
        '--vulkan_allow_present_mode_immediate=false',
        '--vulkan_allow_present_mode_fifo_relaxed=true',
        '--readback_memexport=false',
        '--vulkan_readback_memexport=false',
        '--depth_float24_convert_in_pixel_shader=false',
        '--execute_unclipped_draw_vs_on_cpu=true',
        '--execute_unclipped_draw_vs_on_cpu_for_psi_render_backend=true',
        '--execute_unclipped_draw_vs_on_cpu_with_scissor=false',
        '--occlusion_query_enable=true',
        '--vulkan_submit_on_primary_buffer_end=false'
    )
} else {
    $arguments += ('--host_present_from_non_ui_thread=' + (-not $isVSync).ToString().ToLowerInvariant())
    $arguments += @(
        ('--render_target_path_d3d12=' + $(if ($GraphicsProfile -eq 'compatibility') { 'rov' } else { 'rtv' })),
        '--d3d12_submit_on_primary_buffer_end=false',
        '--d3d12_startup_buffer_recovery=true',
        ('--d3d12_startup_recovery_target_fps=' + $(if ($FrameLimit -gt 0) { $FrameLimit } else { 120 })),
        '--d3d12_paced_present_tearing=true',
        '--d3d12_allow_variable_refresh_rate_and_tearing=true',
        # Match the proven PES6 Vulkan coherency policy. A legacy backend
        # flag at its default value does not override readback_memexport=true.
        '--readback_memexport=false',
        '--d3d12_readback_memexport=false',
        # PES6 uses the resolved textures on the GPU. The inherited FIFA
        # CPU resolve mirror adds downscales, mappings and page invalidations.
        '--d3d12_readback_resolve=false',
        # Bound unclipped clears/draws to the EDRAM they actually touch.
        '--execute_unclipped_draw_vs_on_cpu=true',
        '--execute_unclipped_draw_vs_on_cpu_for_psi_render_backend=true',
        '--execute_unclipped_draw_vs_on_cpu_with_scissor=false'
    )
}

if ($TraceGpu) { $arguments += '--gpu_log_sync_costs=true' }

if ($PlanOnly) {
    [pscustomobject]@{
        Executable = $exe
        Arguments = $arguments
        BufferWidth = $preset[0]
        BufferHeight = $preset[1]
        RenderScale = $preset[2]
        FsrInputStage = $stageFsrInput
        SceneResolution = "${sceneWidth}x${sceneHeight}"
        GuestWidth = [int]$env:PES6_RENDER_WIDTH
        GuestHeight = [int]$env:PES6_RENDER_HEIGHT
        FrameLimit = $FrameLimit
        PacingMode = $PacingMode
        TextLanguage = $TextLanguage
        CommentaryLanguage = $env:PES6_COMM_LANG
        GraphicsApi = $GraphicsApi
        RenderTargetPath = $renderTargetPath
        VSync = $isVSync
        EnhancedEffects = $isEnhancedEffects
        ImprovedRain = $isImprovedRain
        ImprovedParticles = $isImprovedParticles
        PlayerDirt = $isPlayerDirt
        SoftCrowd = $isSoftCrowd
        RealisticKits = $isRealisticKits
        NeuralRendering = $isNeuralRendering
        NeuralNetworkScale = 0.05
        NeuralModel = 'fast'
        Resolution = $Resolution
        InternalResolution = "$($preset[0])x$($preset[1])"
        SpatialFsr = $useSpatialFsr
        FsrProfile = $activeProfile
        FsrOutput = "$($env:FIFA08_FSR_OUTPUT_WIDTH)x$($env:FIFA08_FSR_OUTPUT_HEIGHT)"
        FsrInput = "$($env:FIFA08_FSR_INPUT_WIDTH)x$($env:FIFA08_FSR_INPUT_HEIGHT)"
        PostEffect = $(if ($useSpatialFsr -or $PostEffect -eq 'fsr') { 'none' } else { $PostEffect })
        Anisotropy = $Anisotropy
    } | ConvertTo-Json -Depth 4
    return
}

Write-Host "Iniciando PES 6..."
Write-Host "API grafica: $GraphicsApi (render targets: $renderTargetPath)"
Write-Host "Idiomas: Textos=$TextLanguage, Comentarios=$($env:PES6_COMM_LANG)"
Write-Host "Buffer: $($preset[0])x$($preset[1]) -> $windowWidth x $windowHeight (Cadencia original por escena)"
Write-Host "FSR: $useSpatialFsr (Perfil: $activeProfile, escena: ${sceneWidth}x${sceneHeight}, entrada FSR: $($preset[0])x$($preset[1]), etapa GPU: $stageFsrInput)"
Write-Host "Registro: $log"

if ($CaptureGpu) {
    $renderdoc = 'D:\Herramientas\RenderDoc\RenderDoc_1.46_64\renderdoccmd.exe'
    $captureDir = Join-Path $cleanRoot 'data\captures\gpu'
    New-Item -ItemType Directory -Path $captureDir -Force | Out-Null
    $capturePrefix = Join-Path $captureDir ('konami-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    $captureArguments = @('capture', '--working-dir', ('"' + (Split-Path $exe) + '"'), '--capture-file', ('"' + $capturePrefix + '"'), '--wait-for-exit', ('"' + $exe + '"')) + $arguments
    Write-Host 'RenderDoc: pulsa F12 sobre la toma blanca para guardar un frame GPU.'
    $process = Start-Process -FilePath $renderdoc -ArgumentList $captureArguments -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -PassThru
} else {
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory (Split-Path $exe) -PassThru
}

$neuralProcess = $null
if ($isNeuralRendering) {
    try {
        . (Join-Path $cleanRoot 'bin\scripts\neural-rendering.ps1')
        $neuralProcess = Start-Pes6NeuralRendering -ProjectRoot $cleanRoot -GameProcessId $process.Id
    } catch { Write-Warning "No se pudo iniciar Neural Rendering. PES 6 continua sin NR: $_" }
}

if ($NoWait) { return $process }

try { $process.WaitForExit() }
finally {
    # The NR supervisor watches the actual game handle, even with -NoWait or
    # if this launcher script is closed. Give it time to release its own Job.
    if ($null -ne $neuralProcess -and !$neuralProcess.HasExited) {
        [void]$neuralProcess.WaitForExit(18000)
    }
}
Write-Host "PES 6 finalizo con codigo $($process.ExitCode)."

# IMPORTANT: Do NOT trigger an automatic fallback if it fails!
if ($process.ExitCode -ne 0) {
    throw "PES 6 termino con codigo de error $($process.ExitCode). Consulta el archivo de registro en $log."
}
