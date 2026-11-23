function Get-AmpedGameLanguage($Reader, $Settings) {
    if (-not [IO.File]::Exists($Settings)) { return @{ Line = 0; Language = 'english' } }
    $info = & $Reader --language-info $Settings
    if ($LASTEXITCODE -ne 0 -or $info -notmatch '^(\d+) (english|german|french|italian|spanish)$') {
        throw 'Could not read the game language from profile/settings.toml.'
    }
    return @{ Line = [int]$Matches[1]; Language = $Matches[2] }
}

# Called with the normal launch mutex held, after the profile has been initialized.
function Set-AmpedGameLanguage($Reader, $Settings,
    [ValidateSet('english','german','french','italian','spanish')][string]$Language) {
    $previous = Get-AmpedGameLanguage $Reader $Settings
    $text = [IO.File]::ReadAllText($Settings)
    $newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $entry = 'language = "' + $Language.ToLowerInvariant() + '"'
    $insert = $true
    if ($previous.Line -gt 0) {
        $lines = [regex]::Split($text, '(?<=\n)')
        $i = $previous.Line - 1
        $value = '^(?<key>[^=]+=[ \t]*)(?:"(?:\\.|[^"\\])*"|''[^'']*''|[^\s#"''\[]+)(?<tail>[ \t]*(?:#[^\r\n]*)?(?:\r?\n)?)\z'
        $match = [regex]::Match($lines[$i], $value)
        if (-not $match.Success) { throw 'The game.language entry could not be updated safely.' }
        $entry += $match.Groups['tail'].Value.TrimEnd([char[]]"`r`n")
        $insert = $match.Groups['key'].Value -notmatch '^\s*(?:language|"language"|''language'')\s*='
        if ($insert) { $lines[$i] = '' }
        else { $lines[$i] = $entry + [regex]::Match($lines[$i], '\r?\n\z').Value }
        $text = $lines -join ''
    }
    # RecompLauncher reads section/key pairs literally. Use a real [game]
    # table even when the older profile used a dotted key or quoted header.
    $section = [regex]::Match($text, '(?m)^(?<indent>[ \t]*)\[[ \t]*(?:game|"game"|''game'')[ \t]*\](?<tail>[ \t]*(?:#[^\r\n]*)?(?:\r?\n|\z))')
    if ($section.Success) {
        $header = $section.Groups['indent'].Value + '[game]' + $section.Groups['tail'].Value
        if ($insert) {
            if (-not $header.EndsWith("`n")) { $header += $newline }
            $header += $entry + $newline
        }
        $updated = $text.Remove($section.Index, $section.Length).Insert($section.Index, $header)
    } else {
        if (-not $insert) { throw 'The section containing game.language could not be recognized.' }
        if ($text.Length -and -not $text.EndsWith("`n")) { $text += $newline }
        $updated = $text + '[game]' + $newline + $entry + $newline
    }
    $temporary = Join-Path ([IO.Path]::GetDirectoryName($Settings)) ('.language-' + [Guid]::NewGuid().ToString('N') + '.tmp')
    try {
        [IO.File]::WriteAllText($temporary, $updated, [Text.UTF8Encoding]::new($false))
        $check = Get-AmpedGameLanguage $Reader $temporary
        if ($check.Line -lt 1 -or $check.Language -ne $Language) { throw 'Game language validation failed.' }
        [IO.File]::Replace($temporary, $Settings, [NullString]::Value)
    } finally {
        if ([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) }
    }
}
