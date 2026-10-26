param([Parameter(Mandatory)][string]$Source, [Parameter(Mandatory)][string]$Output)
$ErrorActionPreference = 'Stop'
$text = [IO.File]::ReadAllText($Source)
$marker = 'const char presenterShader[] = R"('
$start = $text.IndexOf($marker)
if ($start -lt 0) { throw 'Presenter shader block missing' }
$start += $marker.Length
$end = $text.IndexOf(')";', $start)
if ($end -lt $start) { throw 'Presenter shader terminator missing' }
[IO.File]::WriteAllText($Output, $text.Substring($start, $end-$start))
