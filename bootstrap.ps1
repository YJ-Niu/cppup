# First build of cppup on Windows.
# Detects the OS, optional Homebrew, then CMake + Ninja from
# PATH, Program Files, scoop, winget, or chocolatey.
# After this, rebuild with the tool itself:
#   .\build\cppup.exe build --release
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

Write-Host "cppup bootstrap"
Write-Host "  OS   windows"

function Test-Exe([string]$Path) {
  return ($Path -and (Test-Path -LiteralPath $Path -PathType Leaf))
}

function Find-BrewPrefix {
  $brew = Get-Command brew -ErrorAction SilentlyContinue
  $candidates = @()
  if ($brew) { $candidates += $brew.Source }
  $candidates += @(
    "$env:ProgramFiles\Git\usr\bin\brew",
    "C:\Program Files\Git\usr\bin\brew"
  )
  foreach ($b in $candidates) {
    if (-not (Test-Exe $b)) { continue }
    $prefix = & $b --prefix 2>$null
    if ($LASTEXITCODE -eq 0 -and $prefix) { return $prefix.Trim() }
  }
  return $null
}

$Brew = Find-BrewPrefix
if ($Brew) {
  Write-Host "  brew $Brew"
  $env:PATH = (Join-Path $Brew "bin") + ";" + $env:PATH
} else {
  Write-Host "  brew (not installed)"
}

function Find-Tool([string]$Name) {
  $cmd = Get-Command $Name -ErrorAction SilentlyContinue
  if ($cmd -and (Test-Exe $cmd.Source)) { return $cmd.Source }

  $file = $Name
  if (-not $file.EndsWith(".exe")) { $file = "$file.exe" }

  $pf = if ($env:ProgramFiles) { $env:ProgramFiles } else { "C:\Program Files" }
  $pf86 = if (${env:ProgramFiles(x86)}) { ${env:ProgramFiles(x86)} } else { "C:\Program Files (x86)" }
  $choco = if ($env:ChocolateyInstall) { $env:ChocolateyInstall } else { "C:\ProgramData\chocolatey" }
  $list = @(
    (Join-Path $pf "CMake\bin\$file"),
    (Join-Path $pf "Ninja\$file"),
    (Join-Path $pf "LLVM\bin\$file"),
    (Join-Path $pf86 "CMake\bin\$file"),
    (Join-Path $pf86 "LLVM\bin\$file"),
    (Join-Path $choco "bin\$file")
  )
  if ($env:USERPROFILE) {
    $list += (Join-Path $env:USERPROFILE "scoop\shims\$file")
  }
  if ($env:LOCALAPPDATA) {
    $list += (Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\$file")
  }
  if ($Brew) {
    $list = @(
      (Join-Path $Brew "bin\$file"),
      (Join-Path $Brew "opt\llvm\bin\$file")
    ) + $list
  }
  foreach ($p in $list) {
    if (Test-Exe $p) { return $p }
  }
  return $null
}

$Cmake = Find-Tool "cmake"
$Ninja = Find-Tool "ninja"
if (-not $Cmake) { throw "cmake not found. winget install Kitware.CMake" }
if (-not $Ninja) { throw "ninja not found. winget install Ninja-build.Ninja" }

$Cxx = Find-Tool "clang++"
if (-not $Cxx) { $Cxx = Find-Tool "cl" }
if (-not $Cxx) { throw "no C++ compiler (clang++ or cl). winget install LLVM.LLVM" }

Write-Host "  cmake $Cmake"
Write-Host "  ninja $Ninja"
Write-Host "  c++   $Cxx"

$cache = Join-Path $Root "build\CMakeCache.txt"
if (Test-Path -LiteralPath $cache) {
  $recorded = Select-String -Path $cache -Pattern '^CMAKE_CACHEFILE_DIR:INTERNAL=(.*)$' |
    Select-Object -First 1
  $here = (Resolve-Path (Join-Path $Root "build")).Path
  if ($recorded -and $recorded.Matches[0].Groups[1].Value.Trim() -ne $here) {
    Write-Host "  stale CMake cache — removing build/"
    Remove-Item -Recurse -Force (Join-Path $Root "build")
  }
}

$args = @(
  "-G", "Ninja",
  "-B", (Join-Path $Root "build"),
  "-S", $Root,
  "-DCMAKE_BUILD_TYPE=Release",
  "-DCMAKE_CXX_COMPILER=$Cxx",
  "-DCMAKE_MAKE_PROGRAM=$Ninja"
)

Write-Host "Configuring..."
& $Cmake @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Building..."
& $Cmake --build (Join-Path $Root "build") --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "cppup is ready: $(Join-Path $Root 'build\cppup.exe')"
Write-Host "Rebuild this repo with itself:"
Write-Host "  $(Join-Path $Root 'build\cppup.exe') build --release"

Write-Host ""
Write-Host "Installing vcpkg (C++ package manager)..."
$cppup = Join-Path $Root "build\cppup.exe"
$vcpkgArgs = @("vcpkg", "setup")
if ($env:CPPUP_VCPKG_SOURCE -eq "china") { $vcpkgArgs += "--china" }
& $cppup @vcpkgArgs
if ($LASTEXITCODE -ne 0) {
  Write-Host "warning: vcpkg setup failed — rerun '$cppup vcpkg setup'" -ForegroundColor Yellow
}
