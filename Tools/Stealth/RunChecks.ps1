# Build and run native regression tests without modifying maps or starting PIE.
# Usage: .\Tools\Stealth\RunChecks.ps1 -EngineRoot 'E:\UE_5.8'
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$EngineRoot,
    [switch]$SkipBuild,
    [string]$TestFilter = 'SpacePirate',
    [ValidateRange(1, 120)][int]$TimeoutMinutes = 15
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$project = Join-Path $projectRoot 'SpacePirate.uproject'
$engine = (Resolve-Path -LiteralPath $EngineRoot).Path
$build = Join-Path $engine 'Engine/Build/BatchFiles/Build.bat'
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
foreach ($required in @($project, $build, $editor)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing file: $required" }
}

# Refuse a build against loaded project DLLs. Never close another person's editor.
if (-not $SkipBuild) {
    $running = Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'" |
        Where-Object { $_.CommandLine -and $_.CommandLine -match 'SpacePirate\.uproject' }
    if ($running) { throw 'Save your work and close the SpacePirate editor before building. Use -SkipBuild only with a current build.' }
}
$run = Join-Path $projectRoot ('Saved/StealthReview/Checks-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $run -Force | Out-Null
Write-Host "Results: $run"
if (-not $SkipBuild) {
    & $build SpacePirateEditor Win64 Development $project -WaitMutex -NoHotReloadFromIDE *> (Join-Path $run 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Build failed. Read $run/build.log" }
    Write-Host 'Build passed.'
}

$report = Join-Path $run 'Automation'
$arguments = @($project, '-unattended', '-nop4', '-nosplash', '-NullRHI',
    '-stdout', '-FullStdOutLogOutput', "-abslog=$run/automation.log",
    "-ReportExportPath=$report", "-ExecCmds=Automation RunTests $TestFilter",
    '-TestExit=Automation Test Queue Empty')
# Start-Process joins the argument list into one Windows command line.
$quotedArguments = ($arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }) -join ' '
$process = Start-Process -FilePath $editor -ArgumentList $quotedArguments -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $run 'stdout.log') -RedirectStandardError (Join-Path $run 'stderr.log')
if (-not $process.WaitForExit($TimeoutMinutes * 60000)) {
    # This process was created by this invocation; no user editor is stopped.
    Stop-Process -Id $process.Id
    throw "Automation timed out after $TimeoutMinutes minutes. Read $run/automation.log"
}
$process.WaitForExit()
$exitCode = $process.ExitCode
$indexPath = Join-Path $report 'index.json'
if (-not (Test-Path -LiteralPath $indexPath)) { throw "No automation report produced (exit $exitCode). Read $run/automation.log" }
$index = Get-Content -LiteralPath $indexPath -Raw -Encoding UTF8 | ConvertFrom-Json
$tests = @($index.tests)
$failed = @($tests | Where-Object { $_.state -ne 'Success' })
if ($exitCode -ne 0 -or $tests.Count -eq 0 -or $failed.Count -gt 0) {
    foreach ($test in $failed) { Write-Host "$($test.state): $($test.fullTestPath)" }
    throw "Automation failed: exit=$exitCode, tests=$($tests.Count), unsuccessful=$($failed.Count). Read $indexPath"
}
Write-Host "PASS: $($tests.Count) native tests. Report: $indexPath"
