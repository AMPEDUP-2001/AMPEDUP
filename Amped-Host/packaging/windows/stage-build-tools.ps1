param(
    [Parameter(Mandatory=$true)][string]$Bundle,
    [Parameter(Mandatory=$true)][string]$Archives,
    [Parameter(Mandatory=$true)][string]$WindowsDeps
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../..')).Path
if (git -C $repo status --porcelain --untracked-files=no) {
    throw 'Commit the release source before packaging the source installer.'
}
if ($LASTEXITCODE -ne 0) { throw 'A Git checkout is required for source packaging.' }
$revision = git -C $repo rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Could not identify the release source.' }
& git -C $repo archive --format=zip "-o$(Join-Path $Bundle 'source.zip')" $revision
if ($LASTEXITCODE -ne 0) { throw 'Could not archive the release source.' }
$revision | Set-Content -LiteralPath (Join-Path $Bundle 'source-revision.txt') -Encoding Ascii
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'build-from-disc.ps1') -Destination $Bundle

$tools = Join-Path $Bundle 'tool-archives'
$deps = Join-Path $Bundle 'windows-deps'
foreach ($folder in @($tools, $deps)) { [void][IO.Directory]::CreateDirectory($folder) }
$checksums = [ordered]@{
    'llvm-mingw-20260922-ucrt-x86_64.zip' = 'E3AD77D117A4BEA19A7A3B333341824D79A5A371004A10E25B8504E7B3047666'
    'cmake-4.4.3-windows-x86_64.zip' = '4D52EBAB7193A698651639ED80D8D04FD903358843572CF44C7FD234CB7C26AB'
    'python-3.11.9-embed-amd64.zip' = '009D6BF7E3B2DDCA3D784FA09F90FE54336D5B60F0E0F305C37F400BF83CFD3B'
    'capstone-5.0.9-py3-none-win_amd64.whl' = '732CEDBBB56D42E723F14D7AF6387F1454194A820B4B96B56D1E53F865EF85D0'
    'ninja-win.zip' = '07FC8261B42B20E71D1720B39068C2E14FFCEE6396B76FB7A795FB460B78DC65'
    'ninja-COPYING.txt' = 'EB7E9AB9690124C5C9F42BDC81383D886A3DEDE26345B6ED15BBAD7CAF81F7EA'
}
foreach ($item in $checksums.GetEnumerator()) {
    $folder = if ($item.Key -like 'llvm-mingw-*') { Join-Path $WindowsDeps 'dl' } else { $Archives }
    $archive = Join-Path $folder $item.Key
    if ((Get-FileHash -LiteralPath $archive).Hash -ne $item.Value) { throw "Unexpected build tool: $($item.Key)" }
    Copy-Item -LiteralPath $archive -Destination $tools
}
foreach ($relative in @('SDL2\x86_64-w64-mingw32', 'glslang', 'vulkan\include', 'vulkan\lib\libvulkan-1.dll.a')) {
    $destination = Join-Path $deps $relative
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destination))
    Copy-Item -LiteralPath (Join-Path $WindowsDeps $relative) -Destination $destination -Recurse
}
