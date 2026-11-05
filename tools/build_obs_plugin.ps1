param([string]$BuildDir = "")
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$sourceRoot = Join-Path $repoRoot "obs-plugin"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake nao encontrado. Use o terminal em que voce compilou o plugin anteriormente."
}
if (-not $BuildDir) {
    $candidates = @("obs-plugin\build_x64", "obs-plugin\build", "build_x64", "build")
    $buildMatches = @()
    foreach ($candidate in $candidates) {
        $path = Join-Path $repoRoot $candidate
        $cache = Join-Path $path "CMakeCache.txt"
        if (Test-Path $cache) {
            $line = Get-Content $cache | Where-Object { $_ -like "CMAKE_HOME_DIRECTORY:INTERNAL=*" }
            if ($line) {
                $configuredSource = ($line -replace "^CMAKE_HOME_DIRECTORY:INTERNAL=", "")
                if ([IO.Path]::GetFullPath($configuredSource).TrimEnd("\", "/") -eq
                    [IO.Path]::GetFullPath($sourceRoot).TrimEnd("\", "/")) {
                    $buildMatches += $path
                }
            }
        }
    }
    if ($buildMatches.Count -ne 1) {
        throw "Nao foi possivel escolher o build existente. Execute: .\tools\build_obs_plugin.ps1 -BuildDir 'CAMINHO_DA_PASTA_QUE_CONTEM_CMakeCache.txt'"
    }
    $BuildDir = $buildMatches[0]
}
$BuildDir = (Resolve-Path $BuildDir).Path
$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (-not (Test-Path $cacheFile)) { throw "CMakeCache.txt nao encontrado em $BuildDir" }
$homeLine = Get-Content $cacheFile | Where-Object { $_ -like "CMAKE_HOME_DIRECTORY:INTERNAL=*" }
$homeDirectory = ($homeLine -replace "^CMAKE_HOME_DIRECTORY:INTERNAL=", "")
if (-not $homeDirectory -or
    [IO.Path]::GetFullPath($homeDirectory).TrimEnd("\", "/") -ne
    [IO.Path]::GetFullPath($sourceRoot).TrimEnd("\", "/")) {
    throw "Esse cache pertence a outro projeto. Escolha o build configurado para $sourceRoot"
}
# Reconfigure with the existing cache, preserving OBS_SOURCE_ROOT, OBS_DEPS_ROOT,
# generator, architecture, libobs_DIR and CMAKE_PREFIX_PATH from the user's setup.
& cmake -S $sourceRoot -B $BuildDir
if ($LASTEXITCODE -ne 0) { throw "Falha ao reconfigurar o plugin. Nenhuma DLL foi instalada." }
& cmake --build $BuildDir --config RelWithDebInfo
if ($LASTEXITCODE -ne 0) { throw "Falha na compilacao. Nenhuma DLL foi instalada." }
$dlls = @(Get-ChildItem $BuildDir -Recurse -Filter "fora-de-cena-obs.dll" -File |
    Sort-Object LastWriteTime -Descending)
if (-not $dlls.Count) { throw "A compilacao terminou, mas a DLL nao foi encontrada." }
Write-Host "DLL compilada: $($dlls[0].FullName)"
Write-Host "Feche o OBS e substitua a DLL instalada por esse arquivo. Consulte obs-plugin/TESTE_IPC.md."
