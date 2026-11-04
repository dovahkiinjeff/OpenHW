function Get-PresenterShaderSource {
    param([Parameter(Mandatory)][string]$Source, [switch]$Legacy)
    $sourcePath = (Resolve-Path -LiteralPath $Source).Path
    $text = [IO.File]::ReadAllText($sourcePath)
    $markers = @('const char presenterBaseShader[] = R"(', 'const char presenterShader[] = R"(')
    if ($Legacy) { $markers = @('const char *legacyShader = R"(') }
    $start = -1
    foreach ($marker in $markers) {
        $start = $text.IndexOf($marker, [StringComparison]::Ordinal)
        if ($start -ge 0) { $start += $marker.Length; break }
    }
    if ($start -lt 0) { throw 'Presenter shader block missing' }
    $end = $text.IndexOf(')";', $start, [StringComparison]::Ordinal)
    if ($end -lt $start) { throw 'Presenter shader terminator missing' }
    $shader = $text.Substring($start, $end-$start)
    if (!$Legacy) {
        $headerPath = Join-Path (Split-Path -Parent $sourcePath) 'GoodGodRaysShader.h'
        $header = [IO.File]::ReadAllText($headerPath)
        $marker = 'inline constexpr char goodGodRaysShader[] = R"('
        $start = $header.IndexOf($marker, [StringComparison]::Ordinal)
        if ($start -lt 0) { throw 'God ray shader block missing' }
        $start += $marker.Length
        $end = $header.IndexOf(')";', $start, [StringComparison]::Ordinal)
        if ($end -lt $start) { throw 'God ray shader terminator missing' }
        $shader += $header.Substring($start, $end-$start)
    }
    return $shader.Replace("`r`n", "`n")
}
