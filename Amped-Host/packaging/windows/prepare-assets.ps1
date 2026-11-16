param([Parameter(Mandatory=$true)][string]$GameRoot, [Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$patch = Get-Content -LiteralPath (Join-Path $Destination 'hud-patches.json') -Raw | ConvertFrom-Json
$sourcePath = Join-Path $GameRoot 'Resource\Shell\Menus.res'
if ((Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash -ne $patch.source_sha256.ToUpperInvariant()) { throw 'Original menu resource hash mismatch.' }
$source = [IO.File]::ReadAllBytes($sourcePath)
if ($source.Length -ne $patch.size) { throw 'Original menu resource length mismatch.' }
foreach ($mode in @('standard','wide')) {
    $bytes = [byte[]]$source.Clone()
    $entry = $patch.modes.$mode
    foreach ($edit in $entry.edits) {
        $index = [int]$edit[0]
        if ($index -lt 0 -or $index -ge $bytes.Length -or $bytes[$index] -ne [byte]$edit[1]) { throw "Invalid $mode resource patch." }
        $bytes[$index] = [byte]$edit[2]
    }
    if ($patch.menu_edits) {
        $assembled = [IO.MemoryStream]::new()
        try {
            $last = 0
            foreach ($edit in $patch.menu_edits) {
                $index = [int]$edit[0]
                $length = [int]$edit[1]
                $hex = [string]$edit[2]
                if ($index -lt $last -or $length -lt 0 -or $index + $length -gt $bytes.Length -or
                    $hex.Length % 2 -ne 0 -or $hex -cnotmatch '^[a-f0-9]*$') {
                    throw 'Invalid menu entry patch.'
                }
                $assembled.Write($bytes, $last, $index - $last)
                for ($i = 0; $i -lt $hex.Length; $i += 2) {
                    $assembled.WriteByte([Convert]::ToByte($hex.Substring($i, 2), 16))
                }
                $last = $index + $length
            }
            $assembled.Write($bytes, $last, $bytes.Length - $last)
            $bytes = $assembled.ToArray()
        } finally { $assembled.Dispose() }
    }
    $dir = Join-Path $Destination ('overrides\' + $mode)
    [void][IO.Directory]::CreateDirectory($dir)
    $path = Join-Path $dir 'Menus.res'
    $stream = [IO.File]::Open($path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($bytes, 0, $bytes.Length) } finally { $stream.Dispose() }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256.ToUpperInvariant()) { throw "Generated $mode resource hash mismatch." }
}
exit 0
