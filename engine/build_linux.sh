set -euo pipefail

# ----- Parse Arguments -----
BUILD_RESOURCES=true
BUILD_OPTIMIZED=true
BUILD_START=false

OUTPUT_FOLDER="../bin"
RESOURCE_FOLDER="../game"
RESOURCE_OUTPUT="$OUTPUT_FOLDER/assets.yuri"
EXECUTABLE_SHADER="$RESOURCE_FOLDER/build_shaders.sh"
EXECUTABLE_OUTPUT="$OUTPUT_FOLDER/kuma.elf"
EXECUTABLE_YURI="$OUTPUT_FOLDER/yuri.elf"
EXECUTABLE_LEVEL=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-resources) BUILD_RESOURCES=false  ;;
        --skip-optimize)  BUILD_OPTIMIZED=false  ;;
        --start-game)     BUILD_START=true       ;;
    esac
    shift
done

if $BUILD_OPTIMIZED; then
    EXECUTABLE_LEVEL+=("-flto" "-O3")
else
    EXECUTABLE_LEVEL+=("-O0")
fi

# ----- Sanity Checks -----
mkdir -p $OUTPUT_FOLDER
if [[ ! -f $EXECUTABLE_YURI ]]; then
    echo "Missing Executable for YURI Toolkit"
    exit 1
fi

# ----- Compile Resources -----
if ! $BUILD_RESOURCES; then
    # Compile Shaders
    "$EXECUTABLE_SHADER" || { 
        echo "Shader Compilation Error ($?)" >&2;
        exit $?;
    }
    # Package Assets
    "$EXECUTABLE_YURI $RESOURCE_FOLDER $RESOURCE_OUTPUT" || { 
        echo "Asset Packaging Error ($?)" >&2; 
        exit $?; 
    }
fi

# ----- Compile Program -----
mapfile -t INPUT_FILES < <(find "source" -type f -name "*.c")
gcc "${INPUT_FILES[@]}" \
    -Wall -Wextra -Werror -pedantic -std=c23 \
    -Wundef -Wdouble-promotion -Wnull-dereference \
    -Wswitch-enum -Wmissing-prototypes -Wmissing-declarations \
    -Iinclude -I$VULKAN_SDK/Include \
    -m64 $EXECUTABLE_LEVEL -o $EXECUTABLE_OUTPUT || {
        echo "Compilation Error ($?)" >&2;
        exit $?;
    }

# ----- Start Game -----
if $BUILD_START; then
    (cd "$OUTPUT_FOLDER" && "$EXECUTABLE_OUTPUT" --console &)
fi