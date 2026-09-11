[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Configuration = "Release",

    [string]$QtBin = "C:\Qt\5.15.2\msvc2019_64\bin",

    [string]$BuildDir = "",

    [string]$OutputDir = "",

    [switch]$Force,

    [switch]$Zip
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $PSScriptRoot "..\build"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $PSScriptRoot "..\dist\KRLmonitor-$Configuration"
}

function Resolve-FullPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$BaseDirectory
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $BaseDirectory $Path))
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildRoot = Resolve-FullPath -Path $BuildDir -BaseDirectory $projectRoot
$outputRoot = Resolve-FullPath -Path $OutputDir -BaseDirectory $projectRoot
$distRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "dist"))

$sourceExe = Join-Path (Join-Path $buildRoot $Configuration) "KRLmonitor.exe"
$windeployqt = Join-Path $QtBin "windeployqt.exe"

if (-not (Test-Path -LiteralPath $sourceExe -PathType Leaf)) {
    throw "找不到构建产物：$sourceExe。请先构建 $Configuration 配置。"
}
if (-not (Test-Path -LiteralPath $windeployqt -PathType Leaf)) {
    throw "找不到 windeployqt：$windeployqt。请通过 -QtBin 指定 Qt 的 bin 目录。"
}

if (Test-Path -LiteralPath $outputRoot) {
    if (-not $Force) {
        throw "输出目录已存在：$outputRoot。确认可删除后重试，或增加 -Force。"
    }

    $safePrefix = $distRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $isUnderDist = $outputRoot.StartsWith($safePrefix, [System.StringComparison]::OrdinalIgnoreCase)
    if (-not $isUnderDist) {
        throw "拒绝删除 dist 目录之外的路径：$outputRoot"
    }
    Remove-Item -LiteralPath $outputRoot -Recurse -Force
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$targetExe = Join-Path $outputRoot "KRLmonitor.exe"
Copy-Item -LiteralPath $sourceExe -Destination $targetExe -Force

$deployMode = if ($Configuration -eq "Release" -or $Configuration -eq "RelWithDebInfo") {
    "--release"
} else {
    "--debug"
}

if ([string]::IsNullOrWhiteSpace($env:VCINSTALLDIR)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
        if (-not [string]::IsNullOrWhiteSpace($vsInstall)) {
            $env:VCINSTALLDIR = (Join-Path $vsInstall "VC") + [System.IO.Path]::DirectorySeparatorChar
            $redistRoot = Join-Path $vsInstall "VC\Redist\MSVC"
            if (Test-Path -LiteralPath $redistRoot -PathType Container) {
                $latestRedist = Get-ChildItem -LiteralPath $redistRoot -Directory |
                    Where-Object { $_.Name -match '^\d+(\.\d+)+$' } |
                    Sort-Object { [version]$_.Name } -Descending |
                    Select-Object -First 1
                if ($latestRedist) {
                    $env:VCToolsRedistDir = $latestRedist.FullName + [System.IO.Path]::DirectorySeparatorChar
                }
            }
        }
    }
}

Write-Host "正在收集 Qt 运行库：$targetExe"
& $windeployqt $deployMode --compiler-runtime --no-translations $targetExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt 执行失败，退出码：$LASTEXITCODE"
}

$platformPlugin = Join-Path $outputRoot "platforms\qwindows.dll"
if (-not (Test-Path -LiteralPath $platformPlugin -PathType Leaf)) {
    throw "部署结果缺少 platforms\qwindows.dll：$outputRoot"
}

$hasCompilerRuntime = @("vcruntime140.dll", "msvcp140.dll", "vc_redist.x64.exe") |
    Where-Object { Test-Path -LiteralPath (Join-Path $outputRoot $_) } |
    Select-Object -First 1
if (-not $hasCompilerRuntime) {
    Write-Warning "windeployqt 未复制 MSVC 运行库。请在目标机安装 Microsoft Visual C++ Redistributable，或从已初始化 Visual Studio 环境的终端重新运行脚本。"
}

Copy-Item -LiteralPath (Join-Path $PSScriptRoot "runtime-instructions.txt") -Destination (Join-Path $outputRoot "运行说明.txt") -Force

$zipPath = "$outputRoot.zip"
if ($Zip) {
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -Path (Join-Path $outputRoot "*") -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Host "已生成 ZIP：$zipPath"
}

Write-Host "部署完成：$outputRoot"