param([switch]$Managed)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$form = New-Object Windows.Forms.Form
$form.Text = 'AMPEDUP!'
$form.Icon = New-Object Drawing.Icon((Join-Path $PSScriptRoot 'ampedup.ico'))
$form.StartPosition = 'CenterScreen'
$form.ClientSize = New-Object Drawing.Size(570, 285)
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.BackColor = [Drawing.Color]::FromArgb(10, 23, 39)
$form.ForeColor = [Drawing.Color]::FromArgb(226, 242, 252)
$form.Font = New-Object Drawing.Font('Segoe UI', 10)
$title = New-Object Windows.Forms.Label
$title.Text = 'AMPEDUP!'; $title.Font = New-Object Drawing.Font('Segoe UI', 26, [Drawing.FontStyle]::Bold)
$title.ForeColor = [Drawing.Color]::FromArgb(128, 216, 241)
$title.Location = New-Object Drawing.Point(28, 20); $title.Size = New-Object Drawing.Size(510, 56)
$form.Controls.Add($title)
$subtitle = New-Object Windows.Forms.Label
$subtitle.Text = "FREESTYLE SNOWBOARDING  |  VERSION 0.8`n`nF1 or View + Menu opens the in-game settings."
$subtitle.Location = New-Object Drawing.Point(32, 91); $subtitle.Size = New-Object Drawing.Size(500, 65)
$form.Controls.Add($subtitle)
. (Join-Path $PSScriptRoot 'game-language.ps1')
$languages = [ordered]@{
    english = 'English'; german = 'Deutsch'; french = "Fran$([char]0xE7)ais"
    italian = 'Italiano'; spanish = "Espa$([char]0xF1)ol"
}
$label = New-Object Windows.Forms.Label
$label.Text = 'Game language'
$label.AutoSize = $true
$label.Location = New-Object Drawing.Point(32, 158)
$form.Controls.Add($label)
$language = New-Object Windows.Forms.ComboBox
$language.DropDownStyle = 'DropDownList'
$language.AccessibleName = $label.Text
$language.Location = New-Object Drawing.Point(158, 154)
$language.Size = New-Object Drawing.Size(180, 28)
$language.Items.AddRange([object[]]@($languages.Values))
$codes = @($languages.Keys)
$reader = Join-Path $PSScriptRoot 'tools\hud_settings_reader.exe'
$profileFile = Join-Path $PSScriptRoot 'profile\settings.toml'
$language.SelectedIndex = [array]::IndexOf($codes, (Get-AmpedGameLanguage $reader $profileFile).Language)
$initialLanguage = $language.SelectedIndex
$form.Controls.Add($language)
function New-LauncherButton([string]$text, [int]$x, [int]$w) {
    $button = New-Object Windows.Forms.Button
    $button.Text = $text; $button.Location = New-Object Drawing.Point($x, 195)
    $button.Size = New-Object Drawing.Size($w, 42); $button.FlatStyle = 'Flat'
    $button.FlatAppearance.BorderColor = [Drawing.Color]::FromArgb(80, 155, 195)
    $button.BackColor = [Drawing.Color]::FromArgb(26, 53, 78); $button.ForeColor = [Drawing.Color]::White
    return $button
}
$play = New-LauncherButton 'PLAY' 32 175
$play.BackColor = [Drawing.Color]::FromArgb(41, 100, 71)
$play.Add_Click({
    try {
        $ps = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $script = Join-Path $PSScriptRoot 'launch.ps1'
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"' + $script + '"'))
        if ($Managed) { $arguments += '-Managed' }
        if ($language.SelectedIndex -ne $initialLanguage) { $arguments += @('-GameLanguage', $codes[$language.SelectedIndex]) }
        Start-Process -FilePath $ps -ArgumentList $arguments -WorkingDirectory $PSScriptRoot -WindowStyle Hidden | Out-Null
        $form.Close()
    } catch { [Windows.Forms.MessageBox]::Show($form, $_.Exception.Message, 'AMPEDUP!', 'OK', 'Error') | Out-Null }
})
$form.Controls.Add($play)
$close = New-LauncherButton 'Close' 420 115
$close.Add_Click({ $form.Close() }); $form.Controls.Add($close)
$form.AcceptButton = $play; $form.CancelButton = $close
[void]$form.ShowDialog()
$form.Dispose()
