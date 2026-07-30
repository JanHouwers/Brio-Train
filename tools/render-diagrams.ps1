<#
.SYNOPSIS
    Render every SVG in hardware/ to a 2x PNG next to it.

.DESCRIPTION
    The SVGs are the source; the PNGs are derived. This script re-renders them
    with Chrome (or Edge) in headless mode at twice the SVG's own pixel size.

    Run it on Windows. The diagrams use "Segoe UI, Arial, sans-serif" and were
    laid out against Segoe UI metrics, so a Linux renderer falls back to a
    different font and text starts to overflow its boxes. The GitHub Actions
    workflow runs on windows-latest for the same reason.

    Keep this file pure ASCII: Windows PowerShell 5.1 reads scripts without a
    BOM as ANSI, and a stray em dash is enough to break the parser.

.EXAMPLE
    pwsh tools/render-diagrams.ps1
    powershell -ExecutionPolicy Bypass -File tools\render-diagrams.ps1
#>
[CmdletBinding()]
param(
    # Output resolution multiplier. 2 gives a crisp image you can zoom into.
    [int] $Scale = 2
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repoRoot    = Split-Path -Parent $PSScriptRoot
$hardwareDir = Join-Path $repoRoot 'hardware'

if (-not (Test-Path $hardwareDir)) {
    throw "hardware/ not found next to tools/. Run this from a checkout of the repository."
}

$browser = @(
    "$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
    "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
    "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe",
    "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe",
    "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $browser) {
    throw "No Chrome or Edge found. Install one, or render the SVGs with another tool."
}
Write-Host "Renderer: $browser"

$svgFiles = Get-ChildItem -Path (Join-Path $hardwareDir '*.svg') | Sort-Object Name
if (-not $svgFiles) { throw "No SVG files in $hardwareDir" }

$failed = @()

foreach ($svg in $svgFiles) {
    $png = [IO.Path]::ChangeExtension($svg.FullName, '.png')

    # The opening svg tag carries the intended pixel size. Chrome's window must
    # match it exactly: too small crops the drawing, too large adds white margin.
    $text = Get-Content -LiteralPath $svg.FullName -Raw
    if ($text -notmatch '(?s)<svg\b[^>]*>') { throw ($svg.Name + ': no svg tag found') }
    $svgTag = $Matches[0]
    if ($svgTag -notmatch 'width="(\d+(?:\.\d+)?)"')  { throw ($svg.Name + ': svg tag has no width') }
    $width  = [int][math]::Ceiling([double]$Matches[1])
    if ($svgTag -notmatch 'height="(\d+(?:\.\d+)?)"') { throw ($svg.Name + ': svg tag has no height') }
    $height = [int][math]::Ceiling([double]$Matches[1])

    $uri = 'file:///' + $svg.FullName.Replace('\', '/').Replace(' ', '%20')

    Write-Host ("Rendering {0}  {1}x{2} -> {3}x{4}" -f $svg.Name, $width, $height,
                ($width * $Scale), ($height * $Scale))

    if (Test-Path $png) { Remove-Item -LiteralPath $png -Force }

    & $browser `
        --headless `
        --disable-gpu `
        --hide-scrollbars `
        --default-background-color=ffffffff `
        --force-device-scale-factor=$Scale `
        --virtual-time-budget=5000 `
        --window-size="$width,$height" `
        --screenshot="$png" `
        $uri | Out-Null

    if (-not (Test-Path $png)) {
        $failed += ($svg.Name + ': no PNG was written')
        continue
    }

    # Verify the render really is the expected size. A mismatch means the window
    # size or the scale factor did not take, and the image is wrong.
    $img = [System.Drawing.Image]::FromFile($png)
    try     { $actualW = $img.Width; $actualH = $img.Height }
    finally { $img.Dispose() }

    if ($actualW -ne ($width * $Scale) -or $actualH -ne ($height * $Scale)) {
        $failed += ("{0}: rendered {1}x{2}, expected {3}x{4}" -f `
                    $svg.Name, $actualW, $actualH, ($width * $Scale), ($height * $Scale))
        continue
    }

    Write-Host ("  ok  {0}  {1:N0} bytes" -f (Split-Path $png -Leaf), (Get-Item $png).Length)
}

if ($failed) {
    $failed | ForEach-Object { Write-Error $_ -ErrorAction Continue }
    throw ("{0} diagram(s) failed to render." -f $failed.Count)
}

Write-Host "All diagrams rendered."
