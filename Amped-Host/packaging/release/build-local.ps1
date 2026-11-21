param(
    [Parameter(Mandatory=$true)][string]$Xbe,
    [Parameter(Mandatory=$true)][string]$DependencyRoot,
    [string]$Python = 'python',
    [ValidateRange(1,16)][int]$BuildJobs = 4,
    [ValidateRange(1,6)][int]$AnalysisJobs = 2,
    [switch]$ReuseGenerated,
    [switch]$Installer
)
$ErrorActionPreference = 'Stop'
$hostDir = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$xbePath = (Resolve-Path -LiteralPath $Xbe).Path
if ((Get-FileHash -LiteralPath $xbePath).Hash -ne 'D825274C3A52B203E6065EF7B0109BCFFFDC175FE8B826FAC215A2ADB779E58E') {
    throw 'That default.xbe does not match the supported European retail executable.'
}
$dependencyDir = (Resolve-Path -LiteralPath $DependencyRoot).Path
$winDeps = Join-Path $dependencyDir 'windows-deps'
if (-not (Test-Path -LiteralPath $winDeps -PathType Container)) {
    $winDeps = Join-Path $dependencyDir 'takedawn-windeps'
}
$llvm = Join-Path $winDeps 'llvm-mingw'
$sdl = Join-Path $winDeps 'SDL2/x86_64-w64-mingw32'
$glslang = Join-Path $winDeps 'glslang'
$vulkan = Join-Path $winDeps 'vulkan'
$build = Join-Path $hostDir 'build'
$pythonPath = (Get-Command $Python -CommandType Application).Source
& $pythonPath -B (Join-Path $PSScriptRoot 'visual/generate.py') (Split-Path -Parent $xbePath) $build
if ($LASTEXITCODE -ne 0) { throw 'Visual asset generation failed.' }
$env:PATH = (Join-Path $llvm 'bin') + ';' + $env:PATH
$generated = [IO.Path]::GetFullPath((Join-Path $hostDir '../Analysis/seeded-v23/generated')).Replace('\','/')
$regenArgs = @((Join-Path $PSScriptRoot 'regenerate.py'), $xbePath, '--jobs', $AnalysisJobs)
if ($ReuseGenerated) { $regenArgs += '--verify-only' }
& $pythonPath @regenArgs
if ($LASTEXITCODE -ne 0) { throw 'Guest regeneration or verification failed. See the logs under Analysis.' }
$flags = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'windows-flags.json') -Raw | ConvertFrom-Json
$cmakeArgs = @(
    '-S', $hostDir, '-B', $build, '-G', 'Ninja',
    "-DAMPED_GENERATED_DIR:PATH=$generated",
    "-DXBOXRECOMP_DIR:PATH=$hostDir/toolkit",
    "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=$PSScriptRoot/windows-toolchain.cmake",
    "-DLLVM_MINGW_ROOT:PATH=$llvm",
    "-DCMAKE_PREFIX_PATH:STRING=$sdl;$glslang",
    "-DSDL2_DIR:PATH=$sdl/lib/cmake/SDL2",
    "-DVulkan_INCLUDE_DIR:PATH=$vulkan/include",
    "-DVulkan_LIBRARY:FILEPATH=$vulkan/lib/libvulkan-1.dll.a",
    "-DPython3_EXECUTABLE:FILEPATH=$pythonPath"
)
foreach ($entry in $flags.PSObject.Properties) {
    $cmakeArgs += "-D$($entry.Name):$($entry.Value.type)=$($entry.Value.value)"
}
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw 'Windows configuration failed.' }
& cmake --build $build --target amped_recomp --parallel $BuildJobs
if ($LASTEXITCODE -ne 0) { throw 'Windows compilation failed.' }
Copy-Item -LiteralPath (Join-Path $sdl 'bin/SDL2.dll') -Destination $build -Force
$exe = Join-Path $build 'amped_recomp.exe'
$sha = (Get-FileHash -LiteralPath $exe).Hash
Write-Output "Built $exe ($sha)"
if ($Installer) {
    & (Join-Path $hostDir 'packaging/windows/build-package.ps1') -DependencyRoot $DependencyRoot `
        -ExecutablePath $exe -ExpectedExecutableSha256 $sha
}
