param([Parameter(Mandatory=$true)][string]$Executable, [string[]]$Arguments = @())
$ErrorActionPreference = 'Stop'
$info = [Diagnostics.ProcessStartInfo]::new()
$info.FileName = (Resolve-Path -LiteralPath $Executable).Path
$info.WorkingDirectory = (Get-Location).Path
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.WindowStyle = 'Hidden'
$info.RedirectStandardOutput = $true
$info.RedirectStandardError = $true
foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
$process = [Diagnostics.Process]::Start($info)
$stdout = $process.StandardOutput.ReadToEndAsync()
$stderr = $process.StandardError.ReadToEndAsync()
$process.WaitForExit()
$stdout.Result
$stderr.Result
if ($process.ExitCode) { throw "Native process exited with code $($process.ExitCode)" }
