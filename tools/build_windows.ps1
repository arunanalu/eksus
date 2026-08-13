$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
Set-Location $PSScriptRoot

python -m pip install -r requirements-dev.txt
python -m PyInstaller --noconfirm --clean --windowed --name Exus-Control `
  --collect-all bleak --collect-all bleak_winrt exus_control.py

Copy-Item "$taskRoot\README.md" "dist\Exus-Control\README.md" -Force
Write-Host "Pacote pronto em tools\dist\Exus-Control\Exus-Control.exe"
