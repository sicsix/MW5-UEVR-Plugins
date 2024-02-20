param(
    [String]$sourcePath,
    [String]$destPath,
    [int]$retryCount = 100,
    [int]$delayInSeconds = 1
)

$retry = 0
while ($true) {
    try {
        Copy-Item -Path $sourcePath -Destination $destPath -Force
        Write-Host "Copy succeeded"
        break
    } catch {
        if ($retry -lt $retryCount) {
            Write-Host "Copy failed, retrying in $delayInSeconds seconds..."
            Start-Sleep -Seconds $delayInSeconds
            $retry++
        } else {
            Write-Host "Copy failed after $retryCount retries."
            exit 1
        }
    }
}
