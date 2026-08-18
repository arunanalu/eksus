$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$boatProfileSource = Join-Path $repoRoot 'game\boat-demo\config\haptics\boat-demo.v1.json'
$boatProfileTarget = Join-Path $PSScriptRoot 'profiles\boat-demo.v1.json'
if (Test-Path $boatProfileSource) {
  Copy-Item $boatProfileSource $boatProfileTarget -Force
}

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
  --specpath "$PSScriptRoot" --paths $repoRoot `
  --add-data "$PSScriptRoot\profiles;exus_control\profiles" `
  "$repoRoot\exus_control_app.py"

Copy-Item "$PSScriptRoot\README.md" "$PSScriptRoot\dist\Exus-Control\README.md" -Force
$archive = "$PSScriptRoot\dist\Exus-Control-Windows.zip"
Compress-Archive -Path "$PSScriptRoot\dist\Exus-Control" -DestinationPath $archive -Force
Write-Host "Pacote pronto em exus_control\dist\Exus-Control\Exus-Control.exe"
Write-Host "Distribuição pronta em exus_control\dist\Exus-Control-Windows.zip"
