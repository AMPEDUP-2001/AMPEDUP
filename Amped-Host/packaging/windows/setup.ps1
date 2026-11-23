param([string]$Image, [string]$Destination, [switch]$NonInteractive, [string]$StatusFile,
    [switch]$LauncherSetup)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$sourceRoot = $PSScriptRoot
$progress = $null
$createdDestination = $false
$discStage = $null
function Set-InstallStatus([string]$message, [double]$fraction = 0) {
    if ($StatusFile) { [IO.File]::WriteAllText($StatusFile, $message, [Text.UTF8Encoding]::new($false)) }
    if ($LauncherSetup -and $fraction -gt 0) {
        [Console]::WriteLine("progress $($fraction.ToString('0.00', [Globalization.CultureInfo]::InvariantCulture)) $message")
    }
}
try {
    if ($LauncherSetup -and (-not $NonInteractive -or -not $Image -or -not $Destination)) {
        throw 'Launcher setup requires a game path and a staging folder.'
    }
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
    $folderInput = $LauncherSetup -and (Test-Path -LiteralPath $Image -PathType Container)
    if (-not $folderInput -and -not (Test-Path -LiteralPath $Image -PathType Leaf)) { throw 'The selected image does not exist.' }
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
    $xbeHash = 'D825274C3A52B203E6065EF7B0109BCFFFDC175FE8B826FAC215A2ADB779E58E'
    if ($folderInput) {
        Set-InstallStatus 'Checking the extracted game folder...' 0.05
        $originalXbe = Join-Path $Image 'default.xbe'
        if (-not (Test-Path -LiteralPath $originalXbe -PathType Leaf) -or
            (Get-FileHash -LiteralPath $originalXbe).Hash -ne $xbeHash) {
            throw 'Choose an extracted folder from the supported European multilingual retail disc.'
        }
    }
    $manifest = Get-Content -Encoding UTF8 -LiteralPath (Join-Path $sourceRoot 'disc-manifest.json') -Raw | ConvertFrom-Json
    $parent = [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Destination))
    $stagePath = Join-Path $parent.FullName ('.ampedup-disc-' + [guid]::NewGuid().ToString('N'))
    $discStage = (New-Item -ItemType Directory -Path $stagePath -ErrorAction Stop).FullName
    $stream = $null
    try {
        if (-not $folderInput) {
            Set-InstallStatus 'Reading the Xbox disc file table...' 0.05
            $stream = [IO.File]::OpenRead($Image)
            $reader = New-Object Diagnostics.ProcessStartInfo
            $reader.FileName = Join-Path $sourceRoot 'tools/disc-file-table.exe'
            $reader.Arguments = '"' + $Image + '"'
            $reader.UseShellExecute = $false
            $reader.CreateNoWindow = $true
            $reader.RedirectStandardOutput = $true
            $reader.StandardOutputEncoding = [Text.Encoding]::GetEncoding(1252)
            $process = [Diagnostics.Process]::Start($reader)
            try {
                $table = $process.StandardOutput.ReadToEnd()
                $process.WaitForExit()
                if ($process.ExitCode -ne 0) { throw "Could not read the Xbox disc image. $($table.Trim())" }
            } finally { $process.Dispose() }
            $entries = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
            foreach ($line in $table.TrimEnd().Split([char]10)) {
                $fields = $line.TrimEnd([char]13) -split "`t", 3
                if ($fields.Count -ne 3 -or $entries.ContainsKey($fields[2])) { throw 'Invalid or duplicate disc file entry.' }
                $entries.Add($fields[2], @([long]$fields[0], [long]$fields[1]))
            }
            if ($entries.Count -ne $manifest.files.Count) {
                throw "The image contains $($entries.Count) files. The European retail disc contains $($manifest.files.Count)."
            }
            $buffer = New-Object byte[] (1MB)
        }
        Set-InstallStatus 'Verifying and copying the retail game files...' 0.10
        $verified = 0
        foreach ($item in $manifest.files) {
            if ($item.path -match '(^/|(^|/)\.\.(/|$))' -or [IO.Path]::IsPathRooted($item.path)) { throw 'Invalid game manifest path.' }
            $file = Join-Path $discStage $item.path
            [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($file))
            if ($folderInput) {
                $original = Join-Path $Image $item.path
                if (-not (Test-Path -LiteralPath $original -PathType Leaf)) { throw "The game folder is missing $($item.path)." }
                Copy-Item -LiteralPath $original -Destination $file
            } else {
                $extent = $entries[$item.path]
                if ($null -eq $extent -or $extent[1] -ne $item.size) { throw "Missing or wrong-size retail file: $($item.path)." }
                $stream.Position = $extent[0]
                $out = [IO.File]::Create($file)
                try {
                    $remaining = [long]$item.size
                    while ($remaining -gt 0) {
                        $n = $stream.Read($buffer, 0, [int][Math]::Min([long]$buffer.Length, $remaining))
                        if ($n -eq 0) { throw "Unexpected end of image reading $($item.path)." }
                        $out.Write($buffer, 0, $n)
                        $remaining -= $n
                    }
                } finally { $out.Dispose() }
            }
            if ((Get-Item -LiteralPath $file).Length -ne $item.size -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $item.sha256) {
                throw "Retail file failed verification: $($item.path). Use the original, unpatched European multilingual disc files."
            }
            $verified++
            if ($verified % 25 -eq 0) { Set-InstallStatus "Verifying game files ($verified / $($manifest.files.Count))..." (0.10 + 0.40 * $verified / $manifest.files.Count) }
        }
    } finally { if ($stream) { $stream.Dispose() } }
    $sourceBuilder = Join-Path $sourceRoot 'build-from-disc.ps1'
    $buildFromDisc = Test-Path -LiteralPath $sourceBuilder -PathType Leaf
    [void](New-Item -ItemType Directory -Path $Destination -ErrorAction Stop)
    $createdDestination = $true
    Set-InstallStatus 'Copying program files...' 0.55
    Copy-Item -Path (Join-Path $sourceRoot 'payload\*') -Destination $Destination -Recurse
    $game = Join-Path $Destination 'Amped-Host\run-game'
    [IO.Directory]::Move($discStage, $game)
    $discStage = $null
    $xbe = Join-Path $game 'default.xbe'
    $installedExe = Join-Path $Destination 'Amped-Host\bin\amped_recomp.exe'
    if ($buildFromDisc) {
        & (Join-Path $sourceRoot 'get-build-tools.ps1') -ReportStatus { param($message) Set-InstallStatus $message 0.58 }
        Set-InstallStatus 'Building AMPEDUP! from your disc. This takes several minutes...' 0.65
        $buildLog = Join-Path $Destination 'setup-build.log'
        try {
            & $sourceBuilder -Xbe $xbe -Executable $installedExe -Destination $Destination -BuildLog $buildLog
        } catch {
            $detail = $_.Exception.Message
            if (Test-Path -LiteralPath $buildLog -PathType Leaf) { $detail += " See $buildLog." }
            throw $detail
        }
    }
    if (-not (Test-Path -LiteralPath $installedExe -PathType Leaf)) { throw 'The game executable is missing from the installation.' }
    if ($progress) { $label.Text = 'Preparing game resources...'; $progress.Refresh(); [Windows.Forms.Application]::DoEvents() }
    Set-InstallStatus 'Preparing the game...' 0.94
    if (-not $buildFromDisc) {
        & (Join-Path $Destination 'prepare-assets.ps1') -GameRoot $game -Destination $Destination
        if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the game resources.' }
    }
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
        $shortcut.IconLocation = (Join-Path $Destination 'ampedup.ico') + ',0'
        $shortcut.Save()
        [Windows.Forms.MessageBox]::Show("Installation complete. Start AMPEDUP! from your desktop.`n`n$Destination", 'AMPEDUP!', 'OK', 'Information') | Out-Null
    }
    Set-InstallStatus 'Installation complete.'
    Write-Output "Installed: $Destination"
    if ($LauncherSetup) { exit 0 }
} catch {
    $message = $_.Exception.Message
    if ($createdDestination) { $message += "`n`nThe incomplete installation is at: $Destination" }
    Set-InstallStatus ("ERROR: " + $message)
    if ($progress) { $progress.Close(); $progress.Dispose(); $progress = $null }
    if (-not $NonInteractive) { [Windows.Forms.MessageBox]::Show($message, 'Amped setup failed', 'OK', 'Error') | Out-Null }
    Write-Error $message
    exit 1
} finally {
    if ($discStage) { Remove-Item -LiteralPath $discStage -Recurse -Force }
}
