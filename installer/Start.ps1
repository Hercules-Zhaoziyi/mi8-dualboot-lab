$ErrorActionPreference='Stop'
Set-Location $PSScriptRoot
$python=Get-Command python -ErrorAction SilentlyContinue
if(-not $python){
 Write-Host '需要 Python 3.11+（含 Tcl/Tk）。可通过官方安装器或 winget 安装：'
 Write-Host 'winget install --exact --id Python.Python.3.12 --source winget'
 Start-Process 'https://www.python.org/downloads/windows/'
 Read-Host '安装后重新运行此脚本；按回车结束';exit 1
}
& $python.Source "$PSScriptRoot\mi8_install_desk.py"
if($LASTEXITCODE -ne 0){Read-Host '软件未启动成功；检查 Python/Tk 环境，按回车结束'}
