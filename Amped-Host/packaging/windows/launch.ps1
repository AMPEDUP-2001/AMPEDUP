param([switch]$OverlayTest, [string]$KeyLiveFile, [switch]$Managed, [switch]$Headless)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$root = $PSScriptRoot
$menuRecipe = Get-Content -LiteralPath (Join-Path $root 'hud-patches.json') -Raw | ConvertFrom-Json
$files = if ($Managed) { Join-Path $root 'runtime' } else { $root }
$switches = @{
    snow_gather = 'AMPED_GATHER_SNOW'
    texture_set_cache = 'AMPED_TEXTURE_SET_CACHE'
    texture_hash_memo = 'AMPED_TEXTURE_HASH_MEMO'
    bulk_pushbuffer = 'AMPED_PB_BULK'
}
$options = $null
$optionsPath = Join-Path $root 'package-options.json'
if (Test-Path -LiteralPath $optionsPath -PathType Leaf) {
    $options = Get-Content -LiteralPath $optionsPath -Raw | ConvertFrom-Json
    if ($options.schema -ne 1 -or $options.snow_gather -isnot [bool]) { throw 'Invalid package options.' }
    foreach ($key in @($switches.Keys) + @('bounded_frame_cap', 'vulkan_batching', 'wma_yield_fix')) {
        if ($options.PSObject.Properties[$key] -and $options.$key -isnot [bool]) { throw "Invalid package option: $key." }
    }
}
$mutex = [Threading.Mutex]::new($false, 'Local\AmpedRecompUnified')
$held = $false
try {
    try { $held = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $held = $true }
    if (-not $held) { throw 'AMPEDUP! is already running.' }
    $game = Join-Path $files 'Amped-Host\bin\amped_recomp.exe'
    if ($Managed -and (-not (Test-Path -LiteralPath (Join-Path $files '.ampedup-ready') -PathType Leaf) -or
        -not (Test-Path -LiteralPath $game -PathType Leaf))) {
        if ($Headless) {
            [Console]::Error.WriteLine('Prepare your game copy in RecompLauncher before playing AMPEDUP!.')
            exit 12
        }
        throw 'Prepare your game copy in RecompLauncher before playing AMPEDUP!.'
    }
    $reader = Join-Path $root 'tools\hud_settings_reader.exe'
    $profile = Join-Path $root 'profile'
    $saves = Join-Path $profile 'saves'
    $settings = Join-Path $profile 'settings.toml'
    foreach ($dir in @($profile, $saves, (Join-Path $profile 'logs'))) { [void][IO.Directory]::CreateDirectory($dir) }
    if (-not (Test-Path -LiteralPath $settings)) {
        $config = "[display]`namped_native_wide = true`n`n[graphics]`nrender_scale = 4`naniso = `"16x`"`naa = `"off`"`namped_crt = true`n"
        $f = [IO.File]::Open($settings, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try { $b = [Text.UTF8Encoding]::new($false).GetBytes($config); $f.Write($b,0,$b.Length) } finally { $f.Dispose() }
    }
    if (-not ('AmpedWindow' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class AmpedWindow {
  public delegate bool Callback(IntPtr window, IntPtr state);
  [DllImport("user32.dll")] public static extern bool EnumWindows(Callback cb, IntPtr state);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window, StringBuilder text, int n);
  [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr window, int command);
}
'@
    }
    for ($restarts = 0; $restarts -lt 20; ++$restarts) {
        $mode = (& $reader $settings | Out-String).Trim()
        if ($LASTEXITCODE -ne 0 -or $mode -notin @('standard','wide')) { throw 'Settings could not be read. See profile/settings.toml.' }
        $override = Join-Path $files ("overrides\$mode\Menus.res")
        $expected = $menuRecipe.modes.$mode.sha256
        if ((Get-FileHash -LiteralPath $override -Algorithm SHA256).Hash -ne $expected) { throw 'A required menu resource is missing or changed.' }
        $disk = Join-Path $profile ('disk-' + $mode)
        [void][IO.Directory]::CreateDirectory($disk)
        $session = Join-Path (Join-Path $profile 'logs') ('session-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
        [void][IO.Directory]::CreateDirectory($session)
        Get-ChildItem Env: | Where-Object { $_.Name -like 'AMPED_*' -or $_.Name -like 'RECOMP_*' } | ForEach-Object { [Environment]::SetEnvironmentVariable($_.Name, $null, 'Process') }
        $env:AMPED_RESTART_MARKER = Join-Path $session 'restart-request.txt'
        $env:AMPED_DEMO_ASSET_OVERRIDE = $override
        $env:AMPED_SETTINGS_TEST_ROOT = $disk
        $env:RECOMP_SAVE_DIR = $saves
        $env:RECOMP_SETTINGS = $settings
        $env:RECOMP_VBLANK = '1'
        $env:RECOMP_PB_EXEC = '1'
        $env:RECOMP_PB_BACKEND = 'vulkan'
        $env:RECOMP_USB = '1'
        $env:RECOMP_KEYBOARD = '1'
        $env:AMPED_KEYBOARD_AXES = '1'
        $env:RECOMP_LOG_DIAG = '0'
        $env:RECOMP_DEBUG = 'cpuid_mmx=1'
        if ($options.vulkan_batching) { $env:RECOMP_DEBUG += ',vk_batch=300' }
        if ($options.wma_yield_fix) { $env:RECOMP_DEBUG += ',amped_wma_yield=1' }
        if ($KeyLiveFile) {
            $live = [IO.Path]::GetFullPath($KeyLiveFile)
            if ($live.Contains(',')) { throw 'Test input path cannot contain a comma.' }
            $env:RECOMP_DEBUG += ',key_live=' + $live
        }
        $env:AMPED_ACK_POLL_BURST = '1'
        foreach ($key in $switches.Keys) {
            if ($options.$key) { [Environment]::SetEnvironmentVariable($switches[$key], '1', 'Process') }
        }
        if ($options.bounded_frame_cap) { $env:AMPED_CAP_PHASE = '2' }
        foreach ($name in @('CONTIG_TOPDOWN','FENCE_EXPERIMENT','FLIP_EXPERIMENT','MOVIE_RHW_EXPERIMENT','OHCI_OWNER_REPORT','MORE_THREAD_STACKS','DEPTH_ONLY_EXPERIMENT','SHADOW_EXPERIMENT','OCC_FF_TRANSFORM','CACHE_DRAW_ENV','CACHE_GUEST_PROBES')) { [Environment]::SetEnvironmentVariable('AMPED_' + $name, '1', 'Process') }
        if ($OverlayTest) { $env:RECOMP_OVERLAY_TEST = Join-Path $session 'overlay.txt'; [IO.File]::WriteAllText($env:RECOMP_OVERLAY_TEST, '') }
        $p = Start-Process -FilePath $game -WorkingDirectory $files -WindowStyle Hidden -RedirectStandardOutput (Join-Path $session 'stdout.log') -RedirectStandardError (Join-Path $session 'stderr.log') -PassThru
        $null = $p.Handle
        $until = [DateTime]::UtcNow.AddSeconds(30)
        $script:shown = $false
        while (-not $p.HasExited -and -not $script:shown -and [DateTime]::UtcNow -lt $until) {
            [AmpedWindow]::EnumWindows({
                param($window, $state)
                $owner = 0
                [void][AmpedWindow]::GetWindowThreadProcessId($window, [ref]$owner)
                if ($owner -eq $p.Id) {
                    $title = New-Object Text.StringBuilder 128
                    [void][AmpedWindow]::GetWindowText($window, $title, $title.Capacity)
                    if ($title.ToString() -in @('AMPEDUP!', 'Amped - diagnostic build')) {
                        [void][AmpedWindow]::ShowWindowAsync($window, 5)
                        $script:shown = $true
                        return $false
                    }
                }
                return $true
            }, [IntPtr]::Zero) | Out-Null
            if (-not $script:shown) { Start-Sleep -Milliseconds 200 }
        }
        $p.WaitForExit()
        $code = $p.ExitCode
        $marker = Join-Path $session 'restart-request.txt'
        if ($code -eq 0 -and (Test-Path -LiteralPath $marker -PathType Leaf)) {
            if ([IO.File]::ReadAllText($marker) -cne "restart`n") { throw 'Invalid restart request.' }
            continue
        }
        if ($code -ne 0) { throw "The game exited with code $code. Logs: $session" }
        exit 0
    }
    throw 'Too many consecutive restarts. Please start the game again.'
} catch {
    if (-not $OverlayTest -and -not $Headless) { [Windows.Forms.MessageBox]::Show($_.Exception.Message, 'AMPEDUP!', 'OK', 'Error') | Out-Null }
    Write-Error $_.Exception.Message
    exit 1
} finally {
    if ($held) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
