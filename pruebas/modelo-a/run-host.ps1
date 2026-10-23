$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$build = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$exe = Join-Path $build 'modelo_a_tests.exe'
& gcc -std=c11 -Wall -Wextra -Werror -pedantic "-I$root/firmware/componentes/modelo_a/include" "$root/firmware/componentes/modelo_a/modelo_a.c" "$PSScriptRoot/host_tests.c" -lm -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Host tests failed' }
