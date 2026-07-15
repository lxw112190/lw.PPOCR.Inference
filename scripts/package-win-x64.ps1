param(
    [string]$BuildDirectory = "build/ninja-x64",
    [string]$OutputDirectory = "dist/lw.PPOCR.Inference-win-x64",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildPath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
$outputPath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory))
$distPath = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "dist"))
$cliProject = Join-Path $projectRoot "bindings/dotnet/samples/UnifiedCli/UnifiedCli.csproj"
$demoProject = Join-Path $projectRoot "bindings/dotnet/samples/UnifiedDemo/UnifiedDemo.csproj"

if (-not $outputPath.StartsWith($distPath + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDirectory must be a child of the project dist directory: $distPath"
}

if (Test-Path -LiteralPath $outputPath) {
    Get-ChildItem -LiteralPath $outputPath -Force |
        Where-Object { $_.Name -ne "test" } |
        Remove-Item -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
& cmake --install $buildPath --prefix $outputPath --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "CMake install failed with exit code $LASTEXITCODE" }

& dotnet publish $cliProject -c Release -o $outputPath
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed with exit code $LASTEXITCODE" }

& dotnet publish $demoProject -c Release -o $outputPath
if ($LASTEXITCODE -ne 0) { throw "WinForms demo publish failed with exit code $LASTEXITCODE" }

$manifestPath = Join-Path $outputPath "package-files.sha256"
$testPath = Join-Path $outputPath "test"
$lines = Get-ChildItem -Path $outputPath -Recurse -File |
    Where-Object {
        $_.FullName -ne $manifestPath -and
        -not $_.FullName.StartsWith(
            $testPath + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)
    } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($outputPath.Length).TrimStart('\', '/').Replace('\', '/')
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
        "$hash  $relative"
    }
[System.IO.File]::WriteAllLines($manifestPath, $lines, [System.Text.UTF8Encoding]::new($false))

Write-Host "Package created: $outputPath"
