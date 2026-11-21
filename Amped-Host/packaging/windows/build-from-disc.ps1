param(
    [Parameter(Mandatory=$true)][string]$Xbe,
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$Destination,
    [Parameter(Mandatory=$true)][string]$BuildLog
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$tools = Join-Path $PSScriptRoot 'tool-archives'
$deps = Join-Path $PSScriptRoot 'windows-deps'
$source = Join-Path $PSScriptRoot 'src'
$pythonDir = Join-Path $PSScriptRoot 'py'
$ninjaDir = Join-Path $PSScriptRoot 'ninja'
function Expand-BuildZip([string]$File, [string]$Destination) {
    [IO.Compression.ZipFile]::ExtractToDirectory($File, $Destination)
}
Expand-BuildZip (Join-Path $PSScriptRoot 'source.zip') $source
Expand-BuildZip (Join-Path $tools 'llvm-mingw-20260922-ucrt-x86_64.zip') $deps
$llvmFolder = Join-Path $deps 'llvm-mingw-20260922-ucrt-x86_64'
Rename-Item -LiteralPath $llvmFolder -NewName 'llvm-mingw'
Expand-BuildZip (Join-Path $tools 'cmake-4.4.3-windows-x86_64.zip') $PSScriptRoot
Expand-BuildZip (Join-Path $tools 'ninja-win.zip') $ninjaDir
Expand-BuildZip (Join-Path $tools 'python-3.11.9-embed-amd64.zip') $pythonDir
foreach ($wheel in @('capstone-5.0.9-py3-none-win_amd64.whl',
    'numpy-2.2.6-cp311-cp311-win_amd64.whl', 'pillow-11.3.0-cp311-cp311-win_amd64.whl')) {
    Expand-BuildZip (Join-Path $tools $wheel) $pythonDir
}
# The isolated Python distribution ignores PYTHONPATH. Give its tools package
# an explicit path inside this install's temporary source directory.
@('python311.zip', '.', '..\src\Amped-Host\toolkit') |
    Set-Content -LiteralPath (Join-Path $pythonDir 'python311._pth') -Encoding Ascii
$oldPath = $env:PATH
try {
    $cmake = Join-Path $PSScriptRoot 'cmake-4.4.3-windows-x86_64\bin'
    $env:PATH = "$cmake;$ninjaDir;$env:SystemRoot\System32;$env:SystemRoot"
    $python = Join-Path $pythonDir 'python.exe'
    $ps = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $builder = Join-Path $source 'Amped-Host\packaging\release\build-local.ps1'
    $cores = [Environment]::ProcessorCount
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ('"' + $builder + '"'),
        '-Xbe', ('"' + $Xbe + '"'), '-DependencyRoot', ('"' + $PSScriptRoot + '"'),
        '-Python', ('"' + $python + '"'), '-AnalysisJobs', [Math]::Min(3, $cores),
        '-BuildJobs', [Math]::Min(8, $cores))
    $errors = $BuildLog + '.stderr'
    $process = Start-Process -FilePath $ps -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $BuildLog -RedirectStandardError $errors
    if ((Get-Item -LiteralPath $errors).Length -gt 0) {
        Get-Content -LiteralPath $errors | Add-Content -LiteralPath $BuildLog
    }
    Remove-Item -LiteralPath $errors
    if ($process.ExitCode -ne 0) { throw "Local compilation exited with code $($process.ExitCode)." }
    $game = Join-Path $source 'Amped-Host\build\amped_recomp.exe'
    $menus = Join-Path $source 'Amped-Host\packaging\windows\build_native_menus.py'
    & $python -B $menus --game-root (Split-Path -Parent $Xbe) --destination $Destination | Add-Content -LiteralPath $BuildLog
    if ($LASTEXITCODE -ne 0) { throw 'Could not generate the native game menus.' }
    $pack = Join-Path (Split-Path -Parent $game) 'snow-summit-visible.ahd'
    Copy-Item -LiteralPath $pack -Destination (Split-Path -Parent $Executable)
    Copy-Item -LiteralPath $game -Destination $Executable
} finally {
    $env:PATH = $oldPath
}
