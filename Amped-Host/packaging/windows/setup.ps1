param([string]$Image, [string]$Destination, [switch]$NonInteractive, [string]$StatusFile)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$sourceRoot = $PSScriptRoot
$progress = $null
$createdDestination = $false
function Set-InstallStatus([string]$message) {
    if ($StatusFile) { [IO.File]::WriteAllText($StatusFile, $message, [Text.UTF8Encoding]::new($false)) }
}
try {
    if (-not $Image) {
        $pick = New-Object Windows.Forms.OpenFileDialog
        $pick.Title = 'Select your Amped XISO image'
        $pick.Filter = 'Xbox disc images (*.iso)|*.iso|All files (*.*)|*.*'
        if ($pick.ShowDialog() -ne [Windows.Forms.DialogResult]::OK) { exit 0 }
        $Image = $pick.FileName
    }
    if (-not $Destination) {
        $folder = New-Object Windows.Forms.FolderBrowserDialog
        $folder.Description = 'Select a parent folder for AMPEDUP!'
        $folder.SelectedPath = [Environment]::GetFolderPath('LocalApplicationData')
        if ($folder.ShowDialog() -ne [Windows.Forms.DialogResult]::OK) { exit 0 }
        $Destination = Join-Path $folder.SelectedPath 'AMPEDUP!'
    }
    $Image = [IO.Path]::GetFullPath($Image)
    $Destination = [IO.Path]::GetFullPath($Destination)
    if (-not (Test-Path -LiteralPath $Image -PathType Leaf)) { throw 'The selected image does not exist.' }
    if (Test-Path -LiteralPath $Destination) { throw "Installation folder already exists; choose a different parent: $Destination" }
    if ($Image -match '[^\x20-\x7e]' -or $Destination -match '[^\x20-\x7e]' -or $Destination.Length -gt 105 -or $Destination.Contains(',')) {
        throw 'For this experimental build, use an image and destination with short ASCII-only paths and no commas.'
    }
    if (-not $NonInteractive) {
        $progress = New-Object Windows.Forms.Form
        $progress.Text = 'Installing AMPEDUP!'
        $progress.Width = 510; $progress.Height = 125
        $progress.StartPosition = 'CenterScreen'
        $progress.ControlBox = $false
        $label = New-Object Windows.Forms.Label
        $label.AutoSize = $true; $label.Left = 20; $label.Top = 25
        $label.Text = 'Checking your game image. This may take a minute...'
        $progress.Controls.Add($label)
        $progress.Show(); $progress.Refresh(); [Windows.Forms.Application]::DoEvents()
    }
    Set-InstallStatus 'Checking the ISO image...'
    if ((Get-FileHash -LiteralPath $Image -Algorithm SHA256).Hash -ne '1DCAF6F4C5385B90987A0DB33CBC34ED408982995DFE3A5B68DCFAD70CA91CB0') {
        throw 'This build supports the exact Amped Europe XISO used to create it. The selected image does not match.'
    }
    $extractor = Join-Path $sourceRoot 'tools\extract-xiso.exe'
    if (-not (Test-Path -LiteralPath $extractor -PathType Leaf)) { throw 'The extractor is missing from the package.' }
    [void](New-Item -ItemType Directory -Path $Destination -ErrorAction Stop)
    $createdDestination = $true
    Set-InstallStatus 'Copying program files...'
    Copy-Item -Path (Join-Path $sourceRoot 'payload\*') -Destination $Destination -Recurse
    $game = Join-Path $Destination 'Amped-Host\run-game'
    [void][IO.Directory]::CreateDirectory($game)
    Set-InstallStatus 'Extracting game files from the ISO...'
    if ($progress) { $label.Text = 'Extracting your game. Please wait...'; $progress.Refresh(); [Windows.Forms.Application]::DoEvents() }
    & $extractor -q -x -d $game $Image
    if ($LASTEXITCODE -ne 0) { throw "Image extraction failed (exit $LASTEXITCODE)." }
    $xbe = Join-Path $game 'default.xbe'
    $menus = Join-Path $game 'Resource\Shell\Menus.res'
    if (-not (Test-Path -LiteralPath $xbe -PathType Leaf) -or (Get-FileHash $xbe -Algorithm SHA256).Hash -ne 'D825274C3A52B203E6065EF7B0109BCFFFDC175FE8B826FAC215A2ADB779E58E') { throw 'Extracted game executable did not match.' }
    if (-not (Test-Path -LiteralPath $menus -PathType Leaf) -or (Get-FileHash $menus -Algorithm SHA256).Hash -ne '336FD7CA3AEAB01B90421348F9F5C8E6996FC8A25B1D74BA07312F139C1653F4') { throw 'Extracted menu resource did not match.' }
    if ($progress) { $label.Text = 'Checking extracted game files...'; $progress.Refresh(); [Windows.Forms.Application]::DoEvents() }
    $manifest = Get-Content -LiteralPath (Join-Path $sourceRoot 'disc-manifest.json') -Raw | ConvertFrom-Json
    $verified = 0
    foreach ($item in $manifest.files) {
        if ($item.path -match '(^/|(^|/)\.\.(/|$))' -or [IO.Path]::IsPathRooted($item.path)) { throw 'Invalid game manifest path.' }
        $path = Join-Path $game ($item.path -replace '/', '\')
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -ne $item.size -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $item.sha256.ToUpperInvariant()) { throw "Extracted game file did not match: $($item.path)" }
        $verified++
        if ($verified % 25 -eq 0) { Set-InstallStatus "Verifying game files ($verified / $(@($manifest.files).Count))..." }
    }
    if (@(Get-ChildItem -LiteralPath $game -File -Recurse -Force).Count -ne @($manifest.files).Count) { throw 'Extracted game file count did not match.' }
    if ($progress) { $label.Text = 'Preparing game resources...'; $progress.Refresh(); [Windows.Forms.Application]::DoEvents() }
    Set-InstallStatus 'Preparing the game...'
    & (Join-Path $Destination 'prepare-assets.ps1') -GameRoot $game -Destination $Destination
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the game resources.' }
    if (-not $NonInteractive) {
        if ($progress) { $progress.Close(); $progress.Dispose(); $progress = $null }
        $desktop = [Environment]::GetFolderPath('DesktopDirectory')
        $shortcutPath = Join-Path $desktop 'AMPEDUP!.lnk'
        if (Test-Path -LiteralPath $shortcutPath) {
            $shortcutPath = Join-Path $desktop ('AMPEDUP! ' + [Guid]::NewGuid().ToString('N').Substring(0,8) + '.lnk')
        }
        $shortcut = (New-Object -ComObject WScript.Shell).CreateShortcut($shortcutPath)
        $shortcut.TargetPath = Join-Path $Destination 'AMPEDUP!.exe'
        $shortcut.WorkingDirectory = $Destination
        $shortcut.Save()
        [Windows.Forms.MessageBox]::Show("Installation complete. Start AMPEDUP! from your desktop.`n`n$Destination", 'AMPEDUP!', 'OK', 'Information') | Out-Null
    }
    Set-InstallStatus 'Installation complete.'
    Write-Output "Installed: $Destination"
} catch {
    $message = $_.Exception.Message
    if ($createdDestination) { $message += "`n`nThe incomplete installation is at: $Destination" }
    Set-InstallStatus ("ERROR: " + $message)
    if ($progress) { $progress.Close(); $progress.Dispose(); $progress = $null }
    if (-not $NonInteractive) { [Windows.Forms.MessageBox]::Show($message, 'Amped setup failed', 'OK', 'Error') | Out-Null }
    Write-Error $message
    exit 1
}

