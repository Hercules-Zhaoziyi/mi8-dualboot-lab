param([string]$Plan='', [string]$Work='', [switch]$Apply)
$ErrorActionPreference='Stop'
if(-not $Work){$Work=Join-Path $env:LOCALAPPDATA 'HerculesMi8Install'}
if(-not $Plan){$Plan=Join-Path $PSScriptRoot 'prepare.plan.json'}
$python=Get-Command python -ErrorAction SilentlyContinue
if(-not $python){
 $winget=Get-Command winget -ErrorAction SilentlyContinue
 if(-not $winget){Start-Process 'https://www.python.org/downloads/windows/';throw 'Install official Python 3.11+ with Tcl/Tk, then launch again'}
 Write-Host 'Installing Python from winget package Python.Python.3.12. System/UAC prompts must be handled locally.'
 & $winget.Source install --exact --id Python.Python.3.12 --source winget
 if($LASTEXITCODE -ne 0){throw 'Python installation failed; no phone command was executed'}
 $env:Path=[Environment]::GetEnvironmentVariable('Path','Machine')+';'+[Environment]::GetEnvironmentVariable('Path','User')
 $python=Get-Command python -ErrorAction SilentlyContinue
 if(-not $python){throw 'Reopen the installer after Python installation'}
}
& $python.Source -c 'import sys; assert sys.version_info >= (3,11), "Python 3.11+ required"'
if($LASTEXITCODE -ne 0){throw 'Python runtime check failed'}
$argsForRun=@("$PSScriptRootworkflow.py",'--plan',$Plan,'--work',$Work)
if($Apply){$argsForRun+='--apply'}
& $python.Source @argsForRun
if($LASTEXITCODE -ne 0){throw 'Workflow stopped. Keep the work directory; inspect logs before resuming.'}
