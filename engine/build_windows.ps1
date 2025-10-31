param(
    [switch]$SkipResources,
    [switch]$SkipOptimize,
    [switch]$StartGame
)
$opt_level = if ($SkipOptimize) { @("-O0") } else { @("-flto", "-O3") }
$path_game = "..\bin\kuma.exe"
$path_yuri = "..\bin\yuri.exe"
$path_glsl = "..\game\build_shaders.ps1"
$path_data = ".\resources\include.res"

# Compile Resources
if (-not $SkipResources) {
    & $path_glsl -Path "..\game\shaders"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Shader Error ($('{0:X}' -f ($LASTEXITCODE -band 0xFFFFFFFF)))"
        exit $LASTEXITCODE
    }
    & $path_yuri package "..\game" "..\bin\assets.yuri"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Packager Error ($('{0:X}' -f ($LASTEXITCODE -band 0xFFFFFFFF)))"
        exit $LASTEXITCODE
    }
    & windres ".\resources\resources.rc" -O coff -o $path_data
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Resource Error ($('{0:X}' -f ($LASTEXITCODE -band 0xFFFFFFFF)))"
        exit $LASTEXITCODE
    }
}

# Compile Program
$inputFiles = Get-ChildItem -Recurse -Include *.c -Path "source" | ForEach-Object { $_.FullName }

& gcc $inputFiles $path_data `
    -Wall -Wextra -Werror -pedantic -std=c23 `
    -Wundef -Wdouble-promotion -Wnull-dereference `
    -Wswitch-enum -Wmissing-prototypes -Wmissing-declarations `
    -Iinclude -IC:\lib\lua\src -I$env:VULKAN_SDK\Include `
    -lgdi32 -lxinput -lole32 -loleaut32 `
    -m64 -mwindows $opt_level `
    -o $path_game

if ($LASTEXITCODE -ne 0) {
    Write-Error "Compiler Error ($('{0:X}' -f ($LASTEXITCODE -band 0xFFFFFFFF)))"
    exit $LASTEXITCODE
}

# Start Program
if ($StartGame) {
    Start-Process -FilePath $path_game `
        -ArgumentList '--console' `
        -WorkingDirectory '..\bin'
}
