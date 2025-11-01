#!/usr/bin/env bash
set -e
mkdir -p "$(dirname "$OUTPUT")"

# Compile Resources
for file in $(find "resources" -type f -name "*.txt");
do
	filename=$(basename ${file} ".txt")
	ld -r -b binary resources/${filename}.txt -o source/${filename}.o
done

# Compile Executable
input_files=$(find "source" -type f -name "*.c")
extra_files=$(find "source" -type f -name "*.o")
gcc $input_files $extra_files \
    -Wall -Wextra -Werror -pedantic -std=c23 \
    -Iinclude -flto -O3  \
    -o "../bin/yuri.elf"

