param(
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$PackageName = 'AMPEDUP-Private',
    [Parameter(Mandatory=$true)][string]$DependencyRoot,
    [string]$ExecutablePath,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedExecutableSha256,
    [switch]$SnowGather = $true
)
$ErrorActionPreference = 'Stop'
$hostDir = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$dependencyDir = (Resolve-Path -LiteralPath $DependencyRoot).Path
$winDeps = Join-Path $dependencyDir 'takedawn-windeps'
$target = Join-Path $hostDir ('dist\' + $PackageName)
if (Test-Path -LiteralPath $target) { throw "Package already exists: $target" }
if ($ExecutablePath) {
    if (-not $ExpectedExecutableSha256) { throw 'Provide ExpectedExecutableSha256 when selecting a game executable.' }
    $exe = (Resolve-Path -LiteralPath $ExecutablePath).Path
} else {
    throw 'Select the build explicitly with ExecutablePath and ExpectedExecutableSha256. No implicit executable is packaged.'
}
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $ExpectedExecutableSha256) { throw 'Unexpected game executable.' }
$sdl = Join-Path (Split-Path -Parent $exe) 'SDL2.dll'
if (-not (Test-Path -LiteralPath $sdl -PathType Leaf)) { throw "SDL2.dll is missing beside the selected executable: $sdl" }
$icon = Join-Path $PSScriptRoot 'ampedup.ico'
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$compiler = Join-Path $winDeps 'llvm-mingw\bin\x86_64-w64-mingw32-clang.exe'
$extractor = Join-Path $dependencyDir 'extract-xiso\build-ucrt\extract-xiso.exe'
foreach ($path in @($icon, $csc, $compiler, $extractor)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing packaging input: $path" }
}
$payload = Join-Path $target 'payload'
foreach ($dir in @($target, (Join-Path $target 'tools'), $payload, (Join-Path $payload 'tools'), (Join-Path $payload 'Amped-Host\bin'), (Join-Path $payload 'notices'), (Join-Path $payload 'notices\LICENSES'))) { [void][IO.Directory]::CreateDirectory($dir) }
Copy-Item (Join-Path $PSScriptRoot 'setup.ps1') $target
Copy-Item (Join-Path $PSScriptRoot 'setup-wizard.ps1') $target
Copy-Item (Join-Path $PSScriptRoot 'README.txt') $target
Copy-Item (Join-Path $PSScriptRoot 'disc-manifest.json') $target
Copy-Item -LiteralPath $icon -Destination $target
Copy-Item -LiteralPath $icon -Destination $payload
Copy-Item (Join-Path $PSScriptRoot 'README.txt') $payload
Copy-Item (Join-Path $PSScriptRoot 'launch.ps1') $payload
$options = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'package-options.json') -Raw | ConvertFrom-Json
$options.snow_gather = [bool]$SnowGather
$options | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $payload 'package-options.json') -Encoding Ascii
Copy-Item (Join-Path $PSScriptRoot 'launcher-wizard.ps1') $payload
Copy-Item (Join-Path $PSScriptRoot 'prepare-assets.ps1') $payload
Copy-Item (Join-Path $PSScriptRoot 'hud-patches.json') $payload
Copy-Item -LiteralPath $exe -Destination (Join-Path $payload 'Amped-Host\bin\amped_recomp.exe')
if ((Get-FileHash -LiteralPath (Join-Path $payload 'Amped-Host\bin\amped_recomp.exe') -Algorithm SHA256).Hash -ne $ExpectedExecutableSha256) { throw 'Packaged executable hash does not match.' }
Copy-Item -LiteralPath $sdl -Destination (Join-Path $payload 'Amped-Host\bin\SDL2.dll')
if ((Get-FileHash -LiteralPath (Join-Path $payload 'Amped-Host\bin\SDL2.dll') -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $sdl -Algorithm SHA256).Hash) { throw 'Packaged SDL2.dll hash does not match.' }
$configParser = Join-Path $hostDir 'toolkit\src\enhance'
$reader = Join-Path $hostDir 'probes\perf_ae_v23\hud_settings_reader.c'
& $compiler -O2 -I $configParser $reader (Join-Path $configParser 'recomp_cfg.c') -o (Join-Path $payload 'tools\hud_settings_reader.exe')
if ($LASTEXITCODE -ne 0) { throw 'Could not compile the settings reader.' }
Copy-Item -LiteralPath $extractor -Destination (Join-Path $target 'tools')
Copy-Item (Join-Path $dependencyDir 'extract-xiso\source\LICENSE.TXT') (Join-Path $payload 'notices\extract-xiso-LICENSE.txt')
Copy-Item (Join-Path $winDeps 'SDL2\LICENSE.txt') (Join-Path $payload 'notices\SDL2-LICENSE.txt')
Copy-Item (Join-Path $hostDir 'toolkit\LICENSE') (Join-Path $payload 'notices\xboxrecomp-LICENSE.txt')
Copy-Item (Join-Path $hostDir 'toolkit\NOTICE') (Join-Path $payload 'notices\xboxrecomp-NOTICE.txt')
Copy-Item (Join-Path $hostDir 'toolkit\LICENSES\*') (Join-Path $payload 'notices\LICENSES') -Recurse
foreach ($component in @('dejavu\LICENSE.txt','roboto_condensed\OFL.txt','smaa\LICENSE.txt','imgui\LICENSE.txt')) {
    $dest = Join-Path $payload ('notices\' + ($component -replace '\\','-'))
    Copy-Item (Join-Path $hostDir ('toolkit\third_party\' + $component)) $dest
}
Copy-Item (Join-Path $winDeps 'glslang-src\LICENSE.txt') (Join-Path $payload 'notices\glslang-LICENSE.txt')
Copy-Item (Join-Path $winDeps 'glslang-src\LICENSES') (Join-Path $payload 'notices\glslang-LICENSES') -Recurse
$bootstrap = Join-Path $PSScriptRoot 'Bootstrap.cs'
$common = @('/nologo', '/target:winexe', '/platform:x64', '/optimize+', "/win32icon:$icon",
    '/reference:System.Windows.Forms.dll', '/reference:System.IO.Compression.dll', '/reference:System.IO.Compression.FileSystem.dll')
& $csc @common "/out:$payload\AMPEDUP!.exe" $bootstrap
if ($LASTEXITCODE -ne 0) { throw 'Could not compile the game launcher.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$bundle = Join-Path (Split-Path -Parent $target) ($PackageName + '-' + [Guid]::NewGuid().ToString('N') + '.zip')
try {
    [IO.Compression.ZipFile]::CreateFromDirectory($target, $bundle, [IO.Compression.CompressionLevel]::Optimal, $false)
    & $csc @common "/resource:$bundle,AMPEDUP.Setup.zip" "/out:$target\Setup-AMPEDUP.exe" $bootstrap
    if ($LASTEXITCODE -ne 0) { throw 'Could not compile the standalone setup.' }
} finally {
    if (Test-Path -LiteralPath $bundle -PathType Leaf) { Remove-Item -LiteralPath $bundle }
}
$files = @(Get-ChildItem -LiteralPath $target -Recurse -File | Sort-Object FullName | ForEach-Object {
    @{ path = $_.FullName.Substring($target.Length + 1).Replace('\','/'); size = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
})
@{ schema = 1; executable_sha256 = $ExpectedExecutableSha256.ToUpperInvariant(); snow_gather = [bool]$SnowGather; files = $files } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $target 'package-manifest.json') -Encoding Ascii
Get-ChildItem $target -Recurse -File | Measure-Object Length -Sum | Format-List Count,Sum
