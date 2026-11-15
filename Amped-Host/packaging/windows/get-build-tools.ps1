param([scriptblock]$ReportStatus = {})
$ErrorActionPreference = 'Stop'

$tools = Join-Path $PSScriptRoot 'tool-archives'
$online = Test-Path -LiteralPath (Join-Path $PSScriptRoot 'download-build-tools.flag') -PathType Leaf
$archives = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build-tool-archives.json') -Raw | ConvertFrom-Json
if ($online) {
    [void][IO.Directory]::CreateDirectory($tools)
    $ProgressPreference = 'SilentlyContinue'
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
}

foreach ($item in $archives) {
    $archive = Join-Path $tools $item.name
    if ((Test-Path -LiteralPath $archive -PathType Leaf) -and
        (Get-FileHash -LiteralPath $archive).Hash -eq $item.sha256) {
        continue
    }
    if (-not $online) { throw "Missing or changed build tool: $($item.name)" }

    $partial = $archive + '.download'
    & $ReportStatus "Downloading $($item.label)..."
    try {
        try {
            Invoke-WebRequest -UseBasicParsing -Uri $item.url -OutFile $partial -TimeoutSec 900
        } catch {
            throw "Could not download $($item.label). Check your internet connection or use the all-in-one installer. $($_.Exception.Message)"
        }
        if ((Get-FileHash -LiteralPath $partial).Hash -ne $item.sha256) {
            throw "Build tool SHA-256 did not match: $($item.name). Retry setup to download it again."
        }
        Move-Item -LiteralPath $partial -Destination $archive -Force
    } finally {
        if (Test-Path -LiteralPath $partial -PathType Leaf) { Remove-Item -LiteralPath $partial }
    }
}
