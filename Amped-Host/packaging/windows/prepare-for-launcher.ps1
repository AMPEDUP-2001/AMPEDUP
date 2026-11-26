param([Parameter(Mandatory=$true)][string]$Game)
$ErrorActionPreference = 'Stop'

$root = $PSScriptRoot
$runtime = Join-Path $root 'runtime'
$id = [Guid]::NewGuid().ToString('N').Substring(0, 8)
$stage = Join-Path $root ('.setup-' + $id)
$backup = Join-Path $root ('.previous-' + $id)
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('AMPEDUP-setup-' + $id)
$mutex = [Threading.Mutex]::new($false, 'Local\AmpedRecompUnified')
$held = $false
$owned = $false

function Remove-SetupDirectory([string]$path) {
    if ([IO.Directory]::Exists($path)) {
        try { [IO.Directory]::Delete($path, $true) }
        catch { Write-Warning "Could not clean up AMPEDUP! setup files at $path." }
    }
}

function Test-RecoverableRuntime([string]$path) {
    foreach ($name in @('', '.ampedup-ready', 'Amped-Host', 'Amped-Host/bin', 'Amped-Host/bin/amped_recomp.exe')) {
        $node = if ($name) { Join-Path $path $name } else { $path }
        $item = Get-Item -LiteralPath $node -Force -ErrorAction SilentlyContinue
        $directory = $name -in @('', 'Amped-Host', 'Amped-Host/bin')
        if (-not $item -or $item.PSIsContainer -ne $directory -or
            ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { return $false }
    }
    return $true
}

try {
    try { $held = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $held = $true }
    if (-not $held) { throw 'Close the running AMPEDUP! game or setup before preparing another copy.' }
    # A terminated process cannot run the catch block between the two directory moves.
    if (-not (Test-Path -LiteralPath $runtime)) {
        $previous = @(Get-ChildItem -LiteralPath $root -Directory -Force -Filter '.previous-*' |
            Where-Object { $_.Name -match '^\.previous-[0-9a-f]{8}$' -and (Test-RecoverableRuntime $_.FullName) })
        if ($previous.Count -gt 1) {
            Write-Warning 'Multiple previous AMPEDUP! runtimes were found. Keeping all of them and continuing with setup.'
        }
        if ($previous.Count -eq 1) {
            [IO.Directory]::Move($previous[0].FullName, $runtime)
            [Console]::WriteLine('progress 0.01 Restored the previous AMPEDUP! runtime after an interrupted update.')
        }
    }
    if (Test-Path -LiteralPath $runtime) {
        $installed = Get-Item -LiteralPath $runtime -Force
        if (($installed.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
            -not (Test-Path -LiteralPath (Join-Path $runtime '.ampedup-ready') -PathType Leaf)) {
            throw 'The runtime folder is already in use. Pick an empty AMPEDUP! installation folder.'
        }
    }
    foreach ($path in @($stage, $backup, $scratch)) {
        if (Test-Path -LiteralPath $path) { throw 'An AMPEDUP! setup staging folder already exists. Retry setup.' }
    }
    $owned = $true
    Copy-Item -LiteralPath (Join-Path $root 'setup') -Destination $scratch -Recurse
    & (Join-Path $scratch 'setup.ps1') -Image $Game -Destination $stage -NonInteractive -LauncherSetup
    if ($LASTEXITCODE -ne 0) { throw "Game preparation stopped with exit code $LASTEXITCODE." }
    foreach ($output in @('amped_recomp.exe', 'snow-summit-visible.ahd')) {
        $path = Join-Path $stage ('Amped-Host/bin/' + $output)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -eq 0) {
            throw "Setup did not produce the required game file $output."
        }
    }
    [IO.File]::WriteAllText((Join-Path $stage '.ampedup-ready'), "1`n")
    [IO.File]::WriteAllText((Join-Path $stage '.ampedup-runtime-r11'), "11`n")

    if ([IO.Directory]::Exists($runtime)) { [IO.Directory]::Move($runtime, $backup) }
    try {
        [IO.Directory]::Move($stage, $runtime)
    } catch {
        if ([IO.Directory]::Exists($backup)) { [IO.Directory]::Move($backup, $runtime) }
        throw
    }
    Remove-SetupDirectory $backup
    [Console]::WriteLine('progress 1.0 AMPEDUP! is ready to play.')
    exit 0
} catch {
    $message = $_.Exception.Message
    if ($owned) {
        $buildLog = Join-Path $stage 'setup-build.log'
        if (Test-Path -LiteralPath $buildLog -PathType Leaf) {
            $logs = Join-Path $root 'profile\logs'
            [void][IO.Directory]::CreateDirectory($logs)
            $retainedLog = Join-Path $logs 'setup-failed.log'
            Copy-Item -LiteralPath $buildLog -Destination $retainedLog -Force
            $message = $message.Replace($buildLog, $retainedLog)
        }
    }
    [Console]::Error.WriteLine($message)
    exit 1
} finally {
    if ($owned) {
        Remove-SetupDirectory $scratch
        Remove-SetupDirectory $stage
    }
    if ($held) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
