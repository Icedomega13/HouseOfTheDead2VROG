param([ValidateRange(1,30)][int]$Seconds = 8, [switch]$Baseline)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$gameDir = Join-Path $projectRoot 'working/pcvr/game'
$reportPath = Join-Path $projectRoot $(if ($Baseline) { 'working/pcvr/startup-baseline.json' } else { 'working/pcvr/startup.json' })
$dll = Join-Path $gameDir 'ddraw.dll'
$disabled = Join-Path $gameDir 'ddraw.dll.disabled'
if ($Baseline) {
    if (Test-Path -LiteralPath $disabled) { throw 'A disabled probe already exists; refusing to overwrite.' }
    Move-Item -LiteralPath $dll -Destination $disabled
}
$game = $null
$report = [ordered]@{ timestamp = (Get-Date).ToString('o'); observationSeconds = $Seconds; baseline = [bool]$Baseline }
try {
    $game = Start-Process -FilePath (Join-Path $gameDir 'Hod2.exe') -WorkingDirectory $gameDir -WindowStyle Hidden -PassThru
    $report.pid = $game.Id
    $exited = $game.WaitForExit($Seconds * 1000)
    $game.Refresh()
    $report.exitedDuringObservation = $exited
    if ($exited) { $report.exitCode = $game.ExitCode }
    else {
        $report.windowTitle = $game.MainWindowTitle
        $report.responding = $game.Responding
        $report.cpuSeconds = $game.TotalProcessorTime.TotalSeconds
        $report.modules = @($game.Modules | ForEach-Object { [ordered]@{name=$_.ModuleName; path=$_.FileName} })
    }
} finally {
    if ($game -and !$game.HasExited) {
        $null = $game.CloseMainWindow()
        if (!$game.WaitForExit(2000)) { Stop-Process -Id $game.Id }
        $report.stoppedByProbe = $true
    }
    if ($Baseline) { Move-Item -LiteralPath $disabled -Destination $dll }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath
}
Write-Output "Startup evidence: $reportPath"
$report | ConvertTo-Json -Depth 5
