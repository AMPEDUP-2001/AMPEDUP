param([string]$SelfTestImage, [string]$SelfTestDestination, [string]$SelfTestDesktop, [switch]$SelfTestNoShortcut)
$ErrorActionPreference = 'Stop'
$script:selfTest = [bool]($SelfTestImage -or $SelfTestDestination -or $SelfTestDesktop)
if ($script:selfTest -and (-not $SelfTestImage -or -not $SelfTestDestination -or -not $SelfTestDesktop)) { throw 'Self-test requires all three test paths.' }
$script:selfTestFailed = $false
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$script:installing = $false
$script:finished = $false
$script:worker = $null
$script:statusFile = $null
$script:errorFile = $null
$script:outputFile = $null
$script:installPath = $null

$form = New-Object Windows.Forms.Form
$form.Text = 'AMPEDUP! Setup'
$form.Icon = New-Object Drawing.Icon((Join-Path $PSScriptRoot 'ampedup.ico'))
$form.Name = 'AmpedSetup'
$form.StartPosition = 'CenterScreen'
$form.ClientSize = New-Object Drawing.Size(700, 465)
$form.MinimumSize = New-Object Drawing.Size(716, 504)
$form.MaximizeBox = $false
$form.FormBorderStyle = 'FixedDialog'
$form.ShowInTaskbar = $true
$form.Font = New-Object Drawing.Font('Segoe UI', 9)
$form.BackColor = [Drawing.Color]::FromArgb(10, 23, 39)
$form.ForeColor = [Drawing.Color]::FromArgb(226, 242, 252)

function Label([string]$text, [int]$x, [int]$y, [int]$w, [int]$h) {
    $v = New-Object Windows.Forms.Label
    $v.Text = $text; $v.Location = New-Object Drawing.Point($x, $y)
    $v.Size = New-Object Drawing.Size($w, $h)
    return $v
}
function Button([string]$text, [int]$x, [int]$y, [int]$w) {
    $v = New-Object Windows.Forms.Button
    $v.Text = $text; $v.Location = New-Object Drawing.Point($x, $y)
    $v.Size = New-Object Drawing.Size($w, 29)
    $v.FlatStyle = 'Flat'; $v.FlatAppearance.BorderColor = [Drawing.Color]::FromArgb(80, 155, 195)
    $v.BackColor = [Drawing.Color]::FromArgb(26, 53, 78); $v.ForeColor = [Drawing.Color]::White
    return $v
}
$title = Label 'Install AMPEDUP!' 24 18 650 35
$title.Font = New-Object Drawing.Font('Segoe UI', 16, [Drawing.FontStyle]::Bold)
$title.ForeColor = [Drawing.Color]::FromArgb(128, 216, 241)
$form.Controls.Add($title)
$form.Controls.Add((Label 'Choose your Amped image and where you want the game installed.' 26 59 640 27))
$form.Controls.Add((Label 'Your Amped XISO image' 26 100 600 22))
$iso = New-Object Windows.Forms.TextBox
$iso.Name = 'IsoPath'; $iso.Location = New-Object Drawing.Point(28, 124); $iso.Size = New-Object Drawing.Size(520, 26)
$iso.BackColor = [Drawing.Color]::FromArgb(26, 42, 59); $iso.ForeColor = [Drawing.Color]::White
$form.Controls.Add($iso)
$browseIso = Button 'Browse...' 560 122 110
$browseIso.Name = 'BrowseIso'; $form.Controls.Add($browseIso)
$form.Controls.Add((Label 'Installation folder' 26 174 600 22))
$destination = New-Object Windows.Forms.TextBox
$destination.Name = 'InstallPath'; $destination.Location = New-Object Drawing.Point(28, 198); $destination.Size = New-Object Drawing.Size(520, 26)
$destination.Text = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Programs\AMPEDUP'
$destination.BackColor = [Drawing.Color]::FromArgb(26, 42, 59); $destination.ForeColor = [Drawing.Color]::White
$form.Controls.Add($destination)
$browseDestination = Button 'Browse...' 560 196 110
$browseDestination.Name = 'BrowseInstall'; $form.Controls.Add($browseDestination)
$shortcut = New-Object Windows.Forms.CheckBox
$shortcut.Name = 'DesktopShortcut'; $shortcut.Text = 'Create a desktop shortcut'
$shortcut.Checked = $true; $shortcut.Location = New-Object Drawing.Point(28, 248); $shortcut.Size = New-Object Drawing.Size(450, 28)
$form.Controls.Add($shortcut)
$note = Label 'Setup uses your own game image. Your image is not changed.' 28 291 640 28
$form.Controls.Add($note)
$status = Label 'Ready to install.' 28 323 640 43
$status.Name = 'Status'; $form.Controls.Add($status)
$bar = New-Object Windows.Forms.ProgressBar
$bar.Location = New-Object Drawing.Point(28, 369); $bar.Size = New-Object Drawing.Size(642, 15)
$bar.Style = 'Continuous'; $bar.Minimum = 0; $bar.Maximum = 100; $bar.Value = 0
$form.Controls.Add($bar)
$install = Button 'Install' 448 407 105
$install.BackColor = [Drawing.Color]::FromArgb(41, 100, 71)
$install.Name = 'InstallButton'; $form.Controls.Add($install)
$cancel = Button 'Cancel' 565 407 105
$cancel.Name = 'CancelButton'; $form.Controls.Add($cancel)
$form.AcceptButton = $install
$form.CancelButton = $cancel

$browseIso.Add_Click({
    $dialog = New-Object Windows.Forms.OpenFileDialog
    $dialog.Title = 'Select your Amped XISO image'
    $dialog.Filter = 'Xbox disc images (*.iso)|*.iso|All files (*.*)|*.*'
    if ($dialog.ShowDialog($form) -eq [Windows.Forms.DialogResult]::OK) { $iso.Text = $dialog.FileName }
    $dialog.Dispose()
    $form.Activate()
})
$browseDestination.Add_Click({
    $dialog = New-Object Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Choose the parent folder where AMPEDUP! will be installed.'
    $dialog.ShowNewFolderButton = $true
    $parent = Split-Path -Parent $destination.Text
    if ($parent -and (Test-Path -LiteralPath $parent -PathType Container)) { $dialog.SelectedPath = $parent }
    if ($dialog.ShowDialog($form) -eq [Windows.Forms.DialogResult]::OK) {
        $destination.Text = Join-Path $dialog.SelectedPath 'AMPEDUP'
    }
    $dialog.Dispose()
    $form.Activate()
})

$timer = New-Object Windows.Forms.Timer
$timer.Interval = 250
$timer.Add_Tick({
    if (-not $script:worker) { return }
    if ($script:statusFile -and (Test-Path -LiteralPath $script:statusFile)) {
        try { $stream = [IO.File]::Open($script:statusFile, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite); try { $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::UTF8); try { $status.Text = $reader.ReadToEnd() } finally { $reader.Dispose() } } finally { $stream.Dispose() } } catch { }
    }
    if (-not $script:worker.HasExited) { return }
    $timer.Stop(); $script:installing = $false; $bar.MarqueeAnimationSpeed = 0
    $cancel.Enabled = $true
    if ($script:worker.ExitCode -ne 0) {
        if ($status.Text -notlike 'ERROR:*') { $status.Text = 'Installation failed. The installation log is at: ' + $script:errorFile }
        $status.ForeColor = [Drawing.Color]::LightCoral
        if (-not $script:selfTest) { [Windows.Forms.MessageBox]::Show($form, $status.Text, 'AMPEDUP! setup failed', 'OK', 'Error') | Out-Null }
        else { $script:selfTestFailed = $true; Write-Output $status.Text; $form.Close() }
        $iso.Enabled = $true; $destination.Enabled = $true; $browseIso.Enabled = $true
        $browseDestination.Enabled = $true; $shortcut.Enabled = $true; $install.Enabled = $true
        return
    }
    $bar.Style = 'Continuous'; $bar.Value = $bar.Maximum
    $status.ForeColor = [Drawing.Color]::LightGreen
    $status.Text = 'Installation complete. Your game is ready.'
    if ($shortcut.Checked) {
        try {
            $desktop = if ($script:selfTest) { $SelfTestDesktop } else { [Environment]::GetFolderPath('DesktopDirectory') }
            $link = Join-Path $desktop 'AMPEDUP!.lnk'
            if (Test-Path -LiteralPath $link) { $link = Join-Path $desktop ('AMPEDUP! ' + [Guid]::NewGuid().ToString('N').Substring(0,8) + '.lnk') }
            $sc = (New-Object -ComObject WScript.Shell).CreateShortcut($link)
            $sc.TargetPath = Join-Path $script:installPath 'AMPEDUP!.exe'
            $sc.WorkingDirectory = $script:installPath
            $sc.IconLocation = (Join-Path $script:installPath 'ampedup.ico') + ',0'
            $sc.Save()
        } catch {
            $status.Text = 'Installed, but the desktop shortcut could not be created. Start AMPEDUP!.exe in the installation folder.'
            if ($script:selfTest) { $script:selfTestFailed = $true }
        }
    }
    $script:finished = $true
    foreach ($temp in @($script:statusFile, $script:outputFile, $script:errorFile)) {
        if ($temp -and (Test-Path -LiteralPath $temp -PathType Leaf)) {
            try { Remove-Item -LiteralPath $temp -ErrorAction Stop } catch { }
        }
    }
    $install.Text = 'Finish'; $install.Enabled = $true; $cancel.Visible = $false
    $note.Text = 'Your saves and settings will be kept in the installation folder.'
    if ($script:selfTest) { if ($bar.Style -ne [Windows.Forms.ProgressBarStyle]::Continuous -or $bar.Value -ne 100) { $script:selfTestFailed = $true; Write-Output 'FAIL: completion bar is not full.' }; Write-Output $status.Text; $form.Close() }
})
$install.Add_Click({
    if ($script:finished) { $form.Close(); return }
    try {
        $imagePath = [IO.Path]::GetFullPath($iso.Text.Trim())
        $script:installPath = [IO.Path]::GetFullPath($destination.Text.Trim())
        if (-not (Test-Path -LiteralPath $imagePath -PathType Leaf)) { throw 'Select an existing Amped XISO image.' }
        if (Test-Path -LiteralPath $script:installPath) { throw 'The installation folder already exists. Choose a new folder.' }
        if ($imagePath -match '[^\x20-\x7e]' -or $script:installPath -match '[^\x20-\x7e]' -or $script:installPath.Length -gt 105 -or $script:installPath.Contains(',')) {
            throw 'Use short ASCII-only paths without commas for this build.'
        }
        $tag = 'AmpedSetup-' + [Guid]::NewGuid().ToString('N')
        $script:statusFile = Join-Path ([IO.Path]::GetTempPath()) ($tag + '.status')
        $script:errorFile = Join-Path ([IO.Path]::GetTempPath()) ($tag + '.err.log')
        $script:outputFile = Join-Path ([IO.Path]::GetTempPath()) ($tag + '.out.log')
        $ps = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $backend = Join-Path $PSScriptRoot 'setup.ps1'
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"' + $backend + '"'),'-Image',('"' + $imagePath + '"'),'-Destination',('"' + $script:installPath + '"'),'-NonInteractive','-StatusFile',('"' + $script:statusFile + '"'))
        $script:worker = Start-Process -FilePath $ps -ArgumentList $arguments -WindowStyle Hidden -RedirectStandardOutput $script:outputFile -RedirectStandardError $script:errorFile -PassThru
        $null = $script:worker.Handle
        $script:installing = $true
        $iso.Enabled = $false; $destination.Enabled = $false; $browseIso.Enabled = $false
        $browseDestination.Enabled = $false; $shortcut.Enabled = $false; $install.Enabled = $false; $cancel.Enabled = $false
        $status.ForeColor = [Drawing.Color]::LightSkyBlue; $status.Text = 'Starting installation...'
        $bar.Style = 'Marquee'; $bar.MarqueeAnimationSpeed = 30
        $timer.Start()
    } catch {
        $status.ForeColor = [Drawing.Color]::LightCoral
        $status.Text = $_.Exception.Message
        if ($script:selfTest) { $script:selfTestFailed = $true; Write-Output $status.Text; $form.Close() }
    }
})
$cancel.Add_Click({ $form.Close() })
$form.Add_FormClosing({ param($sender, $e) if ($script:installing) { $e.Cancel = $true } })
if ($script:selfTest) {
    $form.Add_Shown({
        $iso.Text = $SelfTestImage
        $destination.Text = $SelfTestDestination
        $shortcut.Checked = -not $SelfTestNoShortcut
        $install.PerformClick()
    })
}
[void]$form.ShowDialog()
$timer.Dispose(); $form.Dispose()
if ($script:selfTestFailed) { exit 1 }
