param(
    [switch]$SkipOptimize,
    [switch]$SkipResources
)
$opt_level = if ($SkipOptimize) { @("-O0") } else { @("-flto", "-O3") }
$resources = ".\resources\include.res"
 
# Compile Resources
if (-not $SkipResources) {

    # Generate Objects
    Get-ChildItem -Recurse -Include "*.txt" -Path "resources" | ForEach-Object {
        & ld -r -b binary "resources/$($_.Name)" -o "source/$($_.BaseName).o"
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Object Error ($('{0:X}' -f ($LASTEXITCODE -band 0xFFFFFFFF)))"
            exit $LASTEXITCODE
        }
    }

    # Generate Resources
    & windres ".\resources\resources.rc" -O coff -o $resources
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Resource Error ($('{0:X}' -f ($LASTEXITCODE -band 0xFFFFFFFF)))"
        exit $LASTEXITCODE
    }
}

# Compile Program
$inputFiles = Get-ChildItem -Recurse -Include "*.c" -Path "source" | ForEach-Object { $_.FullName }
$extraFiles = Get-ChildItem -Recurse -Include "*.o" -Path "source" | ForEach-Object { $_.FullName }

gcc $inputFiles $extraFiles $resources `
    -Wall -Wextra -Werror -pedantic -std=c23 `
    -Iinclude $opt_level `
    -o "..\bin\yuri.exe"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Compiler Error ($('{0:X}' -f ($LASTEXITCODE -band 0xFFFFFFFF)))"
    exit $LASTEXITCODE
}