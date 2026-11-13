param(
    [Parameter(Mandatory = $true)]
    [string] $Port,

    [int] $Baud = 115200,

    [string] $Output = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Output)) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $Output = Join-Path (Get-Location) "monitor_$stamp.txt"
}

$outputDirectory = Split-Path -Parent $Output
if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

Write-Host "Capturing sensor values from ESP-IDF monitor on $Port at $Baud baud."
Write-Host "Saving only [DATA] records to $Output"
Write-Host "Stop with Ctrl+C."

"type,time_us,values" | Set-Content -LiteralPath $Output -Encoding utf8

idf.py -p $Port -b $Baud monitor 2>&1 | ForEach-Object {
    if ($_ -match '\[DATA\]\s+(?<record>.+)$') {
        $record = $Matches['record'].Trim()
        $record | Tee-Object -FilePath $Output -Append
    }
}
