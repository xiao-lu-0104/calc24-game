# 计算 24 点游戏 —— 一键构建脚本（Windows / PowerShell）
# 用法：在仓库根目录执行  .\build.ps1
#       或在 VS Code 中按 Ctrl+Shift+B

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$tmp  = Join-Path $root ".build-tmp"
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

# ---------- 定位 MinGW 工具链 ----------
function Find-Tool([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidates = @(
        "D:\TDM-GCC-64\bin",
        "C:\TDM-GCC-64\bin",
        "C:\mingw64\bin",
        "C:\msys64\mingw64\bin",
        "C:\Program Files\mingw64\bin",
        (Join-Path $env:USERPROFILE "w64devkit\bin"),
        (Join-Path $env:USERPROFILE "scoop\apps\mingw\current\bin")
    )
    foreach ($dir in $candidates) {
        $f = Join-Path $dir "$name.exe"
        if (Test-Path $f) { return $f }
    }
    return $null
}

$gxx      = Find-Tool "g++"
$windres  = Find-Tool "windres"

if (-not $gxx) {
    Write-Host "找不到 g++。请先安装 MinGW-w64 / TDM-GCC，并把它的 bin 目录加入 PATH。" -ForegroundColor Red
    exit 1
}

$src = Join-Path $root "src\计算24点游戏.cpp"
$rc  = Join-Path $root "src\计算24点游戏.rc"
$obj = Join-Path $tmp "resource.o"
$out = Join-Path $root "计算24点游戏.exe"

if (-not (Test-Path $src)) {
    Write-Host "找不到源文件：$src" -ForegroundColor Red
    exit 1
}

Write-Host "编译器：$gxx"
& $gxx --version | Select-Object -First 1
Write-Host ""

Push-Location $root
try {
    # ---------- 1. 打包声音资源 ----------
    $hasRes = $false
    if ((Test-Path $windres) -and (Test-Path $rc)) {
        Write-Host "[1/2] windres  编译声音资源 ..."
        & $windres "src\计算24点游戏.rc" -O coff -o $obj --codepage=936
        if ($LASTEXITCODE -eq 0 -and (Test-Path $obj)) {
            $hasRes = $true
        } else {
            Write-Host "      资源编译失败，将生成不带内嵌声音的 exe（仍会读取 音频\ 目录）" -ForegroundColor Yellow
        }
    } else {
        Write-Host "[1/2] 跳过资源编译（缺 windres 或 .rc）" -ForegroundColor Yellow
    }

    # ---------- 2. 编译主程序 ----------
    # -static 系列：把 libstdc++ / libgcc 静态链接进来，生成的 exe 换台机器也能跑
    # -s        ：去掉符号表，体积更小
    $args = @(
        "-std=c++11", "-O2", "-Wall", "-Wextra",
        "-mwindows",
        "-static", "-static-libgcc", "-static-libstdc++", "-s",
        "-o", "计算24点游戏.exe", "src\计算24点游戏.cpp"
    )
    if ($hasRes) { $args += $obj }
    $args += @("-lgdi32", "-lwinmm")

    Write-Host "[2/2] g++      编译主程序 ..."
    & $gxx @args
    if ($LASTEXITCODE -ne 0) { throw "编译失败，退出码 $LASTEXITCODE" }

    Write-Host ""
    $size = [math]::Round((Get-Item $out).Length / 1KB, 1)
    Write-Host "构建成功：$out  ($size KB)" -ForegroundColor Green
}
finally {
    Pop-Location
}
