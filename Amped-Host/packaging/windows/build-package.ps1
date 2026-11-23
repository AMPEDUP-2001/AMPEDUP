param(
    [ValidatePattern('^[a-zA-Z0-9_-]+(\.[a-zA-Z0-9_-]+)*$')][string]$PackageName = 'AMPEDUP-Private',
    [Parameter(Mandatory=$true)][string]$DependencyRoot,
    [string]$ExecutablePath,
    [ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedExecutableSha256,
    [string]$ToolArchives,
    [switch]$Online,
    [switch]$RecompLauncher,
    [switch]$SnowGather = $true
)
$ErrorActionPreference = 'Stop'
$hostDir = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$dependencyDir = (Resolve-Path -LiteralPath $DependencyRoot).Path
$winDeps = Join-Path $dependencyDir 'windows-deps'
if (-not (Test-Path -LiteralPath $winDeps -PathType Container)) {
    $winDeps = Join-Path $dependencyDir 'takedawn-windeps'
}
$package = Join-Path $hostDir ('dist\' + $PackageName)
$zip = $package + '-Windows-x64.zip'
if ((Test-Path -LiteralPath $package) -or ($RecompLauncher -and (Test-Path -LiteralPath $zip))) {
    throw "Package already exists: $package"
}
$target = $package
if ($RecompLauncher) {
    $target = Join-Path $package 'setup'
    $Online = $true
}
$sourceSetup = $Online -or $ToolArchives
if ($sourceSetup -eq [bool]$ExecutablePath -or ($Online -and $ToolArchives)) {
    throw 'Choose RecompLauncher, Online, ToolArchives, or ExecutablePath.'
}
if ($sourceSetup) {
    if ($ToolArchives) { $archiveRoot = (Resolve-Path -LiteralPath $ToolArchives).Path }
    $sdl = Join-Path $winDeps 'SDL2/x86_64-w64-mingw32/bin/SDL2.dll'
} else {
    if (-not $ExpectedExecutableSha256) { throw 'Provide ExpectedExecutableSha256 when selecting a game executable.' }
    $exe = (Resolve-Path -LiteralPath $ExecutablePath).Path
    if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $ExpectedExecutableSha256) { throw 'Unexpected game executable.' }
    $sdl = Join-Path (Split-Path -Parent $exe) 'SDL2.dll'
}
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
Copy-Item (Join-Path $PSScriptRoot 'game-language.ps1') $payload
Copy-Item (Join-Path $PSScriptRoot 'prepare-assets.ps1') $payload
Copy-Item (Join-Path $PSScriptRoot 'hud-patches.json') $payload
if ($sourceSetup) {
    & (Join-Path $PSScriptRoot 'stage-build-tools.ps1') -Bundle $target -Archives $archiveRoot -WindowsDeps $winDeps -Online:$Online
} else {
    Copy-Item -LiteralPath $exe -Destination (Join-Path $payload 'Amped-Host\bin\amped_recomp.exe')
    $pack = Join-Path (Split-Path -Parent $exe) 'snow-summit-visible.ahd'
    if (Test-Path -LiteralPath $pack -PathType Leaf) {
        Copy-Item -LiteralPath $pack -Destination (Join-Path $payload 'Amped-Host\bin')
    }
    if ((Get-FileHash -LiteralPath (Join-Path $payload 'Amped-Host\bin\amped_recomp.exe') -Algorithm SHA256).Hash -ne $ExpectedExecutableSha256) { throw 'Packaged executable hash does not match.' }
}
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
if ($RecompLauncher) {
    Copy-Item -Path (Join-Path $payload '*') -Destination $package -Recurse
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'prepare-for-launcher.ps1') -Destination $package
    Copy-Item -LiteralPath (Join-Path $hostDir '../recomplauncher.json') -Destination $package
    [IO.File]::WriteAllText((Join-Path $package 'portable.txt'), '')
} else {
    $bundle = Join-Path (Split-Path -Parent $target) ($PackageName + '-' + [Guid]::NewGuid().ToString('N') + '.zip')
    try {
        [IO.Compression.ZipFile]::CreateFromDirectory($target, $bundle, [IO.Compression.CompressionLevel]::Optimal, $false)
        & $csc @common "/resource:$bundle,AMPEDUP.Setup.zip" "/out:$target\Setup-AMPEDUP.exe" $bootstrap
        if ($LASTEXITCODE -ne 0) { throw 'Could not compile the standalone setup.' }
    } finally {
        if (Test-Path -LiteralPath $bundle -PathType Leaf) { Remove-Item -LiteralPath $bundle }
    }
}
$files = @(Get-ChildItem -LiteralPath $package -Recurse -File | Sort-Object FullName | ForEach-Object {
    @{ path = $_.FullName.Substring($package.Length + 1).Replace('\','/'); size = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
})
$exeHash = if ($sourceSetup) { $null } else { $ExpectedExecutableSha256.ToUpperInvariant() }
@{ schema = 1; executable_sha256 = $exeHash; build_from_disc = $sourceSetup; download_build_tools = [bool]$Online; snow_gather = [bool]$SnowGather; files = $files } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $package 'package-manifest.json') -Encoding Ascii
if ($RecompLauncher) {
    [IO.Compression.ZipFile]::CreateFromDirectory($package, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
    Get-Item -LiteralPath $zip | Select-Object Name,Length
}
Get-ChildItem -LiteralPath $package -Recurse -File | Measure-Object Length -Sum | Format-List Count,Sum
