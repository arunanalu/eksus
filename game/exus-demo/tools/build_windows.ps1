param(
    [Parameter(Mandatory = $true)]
    [string]$GodotPath
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDirectory = Join-Path $projectRoot 'build\windows'
$outputFile = Join-Path $outputDirectory 'Exus-Demo.exe'

if (-not (Test-Path -LiteralPath $GodotPath)) {
    throw "Godot nao encontrado em: $GodotPath"
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
& $GodotPath --headless --path $projectRoot --export-release 'Windows Desktop' $outputFile
if ($LASTEXITCODE -ne 0) {
    throw "A exportacao do Exus Demo falhou (codigo $LASTEXITCODE). Instale os export templates do Godot 4.5.2."
}

Write-Host "Build criada: $outputFile"
