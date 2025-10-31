param(
    [string]$Path
)
$inputPath  = if ($Path) { $Path } else { "shaders" }
$inputFiles = Get-ChildItem -Recurse -Path $inputPath -Include "*.frag", "*.vert"

foreach ($shader in $inputFiles) {
    $filename  = $shader.FullName
    Write-Host "Compiling: $FileName"

    & glslc $filename `
        -Werror -x glsl -mfmt=bin -O -Os `
        --target-env=vulkan1.1 --target-spv=spv1.3 `
        -o "$filename.spv"
        
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compiler Error ($('{0:X}' -f ($LASTEXITCODE -band 0xFFFFFFFF)))"
        break
    }
}
