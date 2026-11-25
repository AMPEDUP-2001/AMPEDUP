param(
    [Parameter(Mandatory=$true)][string]$Bundle,
    [string]$Archives,
    [Parameter(Mandatory=$true)][string]$WindowsDeps,
    [switch]$Online
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../..')).Path
if (git -C $repo status --porcelain --untracked-files=no) {
    throw 'Commit the release source before packaging the source installer.'
}
if ($LASTEXITCODE -ne 0) { throw 'A Git checkout is required for source packaging.' }
$revision = git -C $repo rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Could not identify the release source.' }
& git -C $repo archive --format=zip "-o$(Join-Path $Bundle 'source.zip')" $revision -- . ':(exclude)docs/screenshots/**'
if ($LASTEXITCODE -ne 0) { throw 'Could not archive the release source.' }
$revision | Set-Content -LiteralPath (Join-Path $Bundle 'source-revision.txt') -Encoding Ascii
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'build-from-disc.ps1') -Destination $Bundle
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'get-build-tools.ps1') -Destination $Bundle
$manifest = Join-Path $PSScriptRoot 'build-tool-archives.json'
Copy-Item -LiteralPath $manifest -Destination $Bundle
$deps = Join-Path $Bundle 'windows-deps'

if ($Online) {
    [IO.File]::WriteAllText((Join-Path $Bundle 'download-build-tools.flag'), '')
} else {
    $tools = Join-Path $Bundle 'tool-archives'
    [void][IO.Directory]::CreateDirectory($tools)
    $archivesToBundle = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    foreach ($item in $archivesToBundle) {
        $folder = if ($item.name -like 'llvm-mingw-*') { Join-Path $WindowsDeps 'dl' } else { $Archives }
        $archive = Join-Path $folder $item.name
        if ((Get-FileHash -LiteralPath $archive).Hash -ne $item.sha256) { throw "Unexpected build tool: $($item.name)" }
        Copy-Item -LiteralPath $archive -Destination $tools
    }
}
foreach ($relative in @('SDL2\x86_64-w64-mingw32', 'glslang', 'vulkan\include', 'vulkan\lib\libvulkan-1.dll.a')) {
    $destination = Join-Path $deps $relative
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $destination))
    Copy-Item -LiteralPath (Join-Path $WindowsDeps $relative) -Destination $destination -Recurse
}
