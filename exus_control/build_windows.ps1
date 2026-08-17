$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue | Select-Object -First 1
if ($pythonCommand) {
  $python = $pythonCommand.Source
} else {
  $python = Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python*\python.exe" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $python) {
  throw "Python 3 não foi encontrado. Instale-o e execute este script novamente."
}

& $python -m pip install -r "$PSScriptRoot\requirements-dev.txt"
& $python -m PyInstaller --noconfirm --clean --windowed --name Exus-Control `
  --distpath "$PSScriptRoot\dist" --workpath "$PSScriptRoot\build" `
  --specpath "$PSScriptRoot" --paths $repoRoot --collect-all bleak --collect-all bleak_winrt `
  "$repoRoot\exus_control_app.py"

Copy-Item "$PSScriptRoot\README.md" "$PSScriptRoot\dist\Exus-Control\README.md" -Force
$archive = "$PSScriptRoot\dist\Exus-Control-Windows.zip"
Compress-Archive -Path "$PSScriptRoot\dist\Exus-Control" -DestinationPath $archive -Force
Write-Host "Pacote pronto em exus_control\dist\Exus-Control\Exus-Control.exe"
Write-Host "Distribuição pronta em exus_control\dist\Exus-Control-Windows.zip"
