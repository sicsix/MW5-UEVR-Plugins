param(
    [string] $SourcePath
)

$destRoots = @(
    'C:\Users\Tim\AppData\Roaming\UnrealVRMod\MechWarrior-Win64-Shipping\plugins\shaders'
)

$pattern = [regex] '\\shaders\\(.*)$'
$m = $pattern.Match($SourcePath)
if (-not $m.Success) {
    Write-Host "CopyShader: skipping, not under a Shaders folder: $SourcePath"
    exit 0
}
$rel = $m.Groups[1].Value

foreach ($root in $destRoots) {
    $dest = Join-Path $root $rel
    $dir  = Split-Path  $dest
    if (!(Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    Copy-Item -Path $SourcePath -Destination $dest -Force
    Write-Host "Copied `"$SourcePath`" → `"$dest`""
}
