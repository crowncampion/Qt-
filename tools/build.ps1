<#
.SYNOPSIS
    Build the project with Qt's uic / moc / rcc and MinGW g++ directly.

.DESCRIPTION
    Useful when the CMake configure step cannot run (for example a sandbox that
    forbids cmake.exe from writing its user package registry under %APPDATA%).
    Normal development should use Qt Creator or `cmake -S . -B build`.

    This script only verifies compilation; it does not deploy Qt DLLs.

.PARAMETER Clean
    Remove the output directory before building.
#>
[CmdletBinding()]
param(
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$root     = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build-manual'
$mocDir   = Join-Path $buildDir 'moc'
$uicDir   = Join-Path $buildDir 'uic'
$objDir   = Join-Path $buildDir 'obj'

$qt    = 'D:\Qt\6.11.1\mingw_64'
$mingw = 'D:\Qt\Tools\mingw1310_64\bin'

foreach ($tool in @("$qt\bin\uic.exe", "$qt\bin\moc.exe", "$qt\bin\rcc.exe", "$mingw\g++.exe")) {
    if (-not (Test-Path $tool)) { throw "Missing build tool: $tool" }
}

if ($Clean -and (Test-Path $buildDir)) { Remove-Item -Recurse -Force $buildDir }
foreach ($dir in @($buildDir, $mocDir, $uicDir, $objDir)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$env:Path = "$mingw;$qt\bin;" + $env:Path

$includeDirs = @(
    (Join-Path $root 'src'),
    $uicDir,
    "$qt\include",
    "$qt\include\QtCore",
    "$qt\include\QtGui",
    "$qt\include\QtWidgets",
    "$qt\include\QtSql"
)
$includeArgs = $includeDirs | ForEach-Object { "-I$_" }

# --- 1. uic: .ui -> ui_<name>.h ---------------------------------------------
$uiFiles = @(Get-ChildItem -Path (Join-Path $root 'src') -Recurse -Filter '*.ui')
Write-Host "==> uic ($($uiFiles.Count) forms)"
foreach ($ui in $uiFiles) {
    $out = Join-Path $uicDir ("ui_{0}.h" -f $ui.BaseName)
    & "$qt\bin\uic.exe" @($ui.FullName, '-o', $out)
    if ($LASTEXITCODE -ne 0) { throw "uic failed: $($ui.Name)" }
}

# --- 2. moc: headers with Q_OBJECT -> moc_<name>.cpp ------------------------
Write-Host '==> moc'
$mocSources = @()
$headers = @(Get-ChildItem -Path (Join-Path $root 'src') -Recurse -Include '*.h', '*.hpp')
foreach ($header in $headers) {
    if ((Get-Content $header.FullName -Raw) -notmatch 'Q_OBJECT') { continue }
    $out = Join-Path $mocDir ("moc_{0}.cpp" -f $header.BaseName)
    & "$qt\bin\moc.exe" @($header.FullName, '-o', $out) @includeArgs
    if ($LASTEXITCODE -ne 0) { throw "moc failed: $($header.Name)" }
    $mocSources += $out
}

# --- 3. rcc: resources ------------------------------------------------------
Write-Host '==> rcc'
$qrcOut = Join-Path $buildDir 'qrc_resources.cpp'
& "$qt\bin\rcc.exe" @('--name', 'app_resources', (Join-Path $root 'resources\resources.qrc'), '-o', $qrcOut)
if ($LASTEXITCODE -ne 0) { throw 'rcc failed' }

# --- 4. compile -------------------------------------------------------------
$cppFiles = @(Get-ChildItem -Path (Join-Path $root 'src') -Recurse -Filter '*.cpp' |
    ForEach-Object { $_.FullName })
$allSources = $cppFiles + $mocSources + @($qrcOut)
Write-Host "==> compiling $($allSources.Count) translation units"

$objects = @()
$failed = @()
foreach ($source in $allSources) {
    $object = Join-Path $objDir (([System.IO.Path]::GetFileNameWithoutExtension($source)) + '.o')
    $objects += $object

    $upToDate = (Test-Path $object) -and ((Get-Item $object).LastWriteTime -gt (Get-Item $source).LastWriteTime)
    if ($upToDate) { continue }

    & "$mingw\g++.exe" -std=c++17 -O1 -g -fPIC -DUNICODE -D_UNICODE -DQT_NO_DEBUG_OUTPUT `
        @includeArgs -c $source -o $object
    if ($LASTEXITCODE -ne 0) { $failed += $source }
}

if ($failed.Count -gt 0) {
    Write-Host 'COMPILE FAILED:' -ForegroundColor Red
    $failed | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

# --- 5. link ----------------------------------------------------------------
Write-Host '==> linking'
$exe = Join-Path $buildDir 'mysql_window.exe'
& "$mingw\g++.exe" @('-o', $exe) @objects `
    "-L$qt\lib" -lQt6Widgets -lQt6Gui -lQt6Sql -lQt6Core -mwindows
if ($LASTEXITCODE -ne 0) { throw 'link failed' }

Write-Host "BUILD OK: $exe" -ForegroundColor Green

# --- 6. test binary ---------------------------------------------------------
# The test target only needs core + data + service (no widgets, no forms).
Write-Host '==> building test binary'
$testObjDir = Join-Path $buildDir 'obj-test'
New-Item -ItemType Directory -Force -Path $testObjDir | Out-Null

$testSources = @(
    (Join-Path $root 'tests\test_library_service.cpp'),
    (Join-Path $root 'tests\TestRunner.cpp')
)
foreach ($layer in @('core', 'data', 'service')) {
    $testSources += @(Get-ChildItem -Path (Join-Path $root "src\$layer") -Filter '*.cpp' |
        ForEach-Object { $_.FullName })
}

$testObjects = @()
$testFailed = @()
foreach ($source in $testSources) {
    $object = Join-Path $testObjDir (([System.IO.Path]::GetFileNameWithoutExtension($source)) + '.o')
    $testObjects += $object

    $upToDate = (Test-Path $object) -and ((Get-Item $object).LastWriteTime -gt (Get-Item $source).LastWriteTime)
    if ($upToDate) { continue }

    & "$mingw\g++.exe" -std=c++17 -O1 -g -fPIC -DUNICODE -D_UNICODE `
        @includeArgs -I(Join-Path $root 'tests') -c $source -o $object
    if ($LASTEXITCODE -ne 0) { $testFailed += $source }
}
if ($testFailed.Count -gt 0) {
    Write-Host 'TEST COMPILE FAILED:' -ForegroundColor Red
    $testFailed | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

# Console subsystem (no -mwindows) so the runner can print its report.
$testExe = Join-Path $buildDir 'library_service_tests.exe'
& "$mingw\g++.exe" @('-o', $testExe) @testObjects "-L$qt\lib" -lQt6Sql -lQt6Core
if ($LASTEXITCODE -ne 0) { throw 'test link failed' }

Write-Host "TEST BUILD OK: $testExe" -ForegroundColor Green

# --- 7. GUI smoke test ------------------------------------------------------
# Needs the full app (widgets + forms) plus a programmatic main().
# Reuses the application objects already compiled above.
Write-Host '==> building GUI smoke test'
$guiObjDir = Join-Path $buildDir 'obj-gui'
New-Item -ItemType Directory -Force -Path $guiObjDir | Out-Null

$guiSources = @(Join-Path $root 'tests\test_gui_smoke.cpp')
foreach ($source in ($allSources | Where-Object { $_ -notlike '*\main.cpp' })) {
    $guiSources += $source
}

$guiObjects = @()
$guiFailed = @()
foreach ($source in $guiSources) {
    $object = Join-Path $guiObjDir (([System.IO.Path]::GetFileNameWithoutExtension($source)) + '.o')
    $guiObjects += $object

    $upToDate = (Test-Path $object) -and ((Get-Item $object).LastWriteTime -gt (Get-Item $source).LastWriteTime)
    if ($upToDate) { continue }

    & "$mingw\g++.exe" -std=c++17 -O1 -g -fPIC -DUNICODE -D_UNICODE `
        @includeArgs -c $source -o $object
    if ($LASTEXITCODE -ne 0) { $guiFailed += $source }
}
if ($guiFailed.Count -gt 0) {
    Write-Host 'GUI TEST COMPILE FAILED:' -ForegroundColor Red
    $guiFailed | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

$guiExe = Join-Path $buildDir 'gui_smoke_tests.exe'
& "$mingw\g++.exe" @('-o', $guiExe) @guiObjects `
    "-L$qt\lib" -lQt6Widgets -lQt6Gui -lQt6Sql -lQt6Core
if ($LASTEXITCODE -ne 0) { throw 'gui test link failed' }

Write-Host "GUI TEST BUILD OK: $guiExe" -ForegroundColor Green
