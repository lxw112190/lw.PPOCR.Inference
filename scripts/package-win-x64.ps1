param(
    [string]$BuildDirectory = "build/ninja-x64",
    [string]$OutputDirectory = "dist/lw.PPOCR.Inference-win-x64",
    [string]$Configuration = "Release",
    [string]$Runtime = "",        # comma-separated: opencv,directml,openvino,tensorrt
    [switch]$Split                # if set, produce per-backend packages
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildPath   = [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
$distPath    = [IO.Path]::GetFullPath((Join-Path $projectRoot "dist"))
$cliProject  = Join-Path $projectRoot "bindings/dotnet/samples/UnifiedCli/UnifiedCli.csproj"
$demoProject = Join-Path $projectRoot "bindings/dotnet/samples/UnifiedDemo/UnifiedDemo.csproj"

$allRuntimes = @("opencv", "directml", "openvino", "tensorrt")
$selectedRuntimes = if ($Runtime) {
    $Runtime.Split(',', [StringSplitOptions]::RemoveEmptyEntries) |
        ForEach-Object { $_.Trim().ToLowerInvariant() } |
        Where-Object { $_ -in $allRuntimes }
} else { $allRuntimes }

if ($selectedRuntimes.Count -eq 0) {
    throw "Runtime must be one or more of: $($allRuntimes -join ', ')"
}

# ----- helpers -----

function Generate-Sha256($directory, $manifestName = "package-files.sha256") {
    $manifestPath = Join-Path $directory $manifestName
    $testPath     = Join-Path $directory "test"
    $lines = Get-ChildItem -Path $directory -Recurse -File |
        Where-Object {
            $_.FullName -ne $manifestPath -and
            -not $_.FullName.StartsWith(
                $testPath + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)
        } |
        Sort-Object FullName |
        ForEach-Object {
            $relative = $_.FullName.Substring($directory.Length).TrimStart('\', '/').Replace('\', '/')
            $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
            "$hash  $relative"
        }
    [IO.File]::WriteAllLines($manifestPath, $lines, [Text.UTF8Encoding]::new($false))
}

function Clean-Output($path) {
    if (Test-Path -LiteralPath $path) {
        Get-ChildItem -LiteralPath $path -Force |
            Where-Object { $_.Name -ne "test" } |
            Remove-Item -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $path | Out-Null
}

# ----- unified package (default) -----

function Build-Unified {
    $outputPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory))
    if (-not $outputPath.StartsWith($distPath + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "OutputDirectory must be under $distPath"
    }

    Write-Host "=== Unified package: $outputPath ===" -ForegroundColor Cyan
    Write-Host "Runtimes: $($selectedRuntimes -join ', ')"

    Clean-Output $outputPath
    & cmake --install $buildPath --prefix $outputPath --config $Configuration
    if ($LASTEXITCODE -ne 0) { throw "cmake --install failed" }

    # remove runtimes not selected
    $runtimeRoot = Join-Path $outputPath "runtimes/win-x64"
    if (Test-Path $runtimeRoot) {
        $allRuntimes | Where-Object { $_ -notin $selectedRuntimes } |
            ForEach-Object {
                $dir = Join-Path $runtimeRoot $_
                if (Test-Path $dir) {
                    Write-Host "  remove runtime: $_"
                    Remove-Item -Recurse -Force $dir
                }
            }
    }

    & dotnet publish $cliProject -c Release -o $outputPath
    if ($LASTEXITCODE -ne 0) { throw "dotnet publish CLI failed" }
    & dotnet publish $demoProject -c Release -o $outputPath
    if ($LASTEXITCODE -ne 0) { throw "dotnet publish Demo failed" }

    Generate-Sha256 $outputPath
    Write-Host "Unified package created: $outputPath" -ForegroundColor Green
}

# ----- split packages -----

function Build-Split {
    Write-Host "=== Split packages ===" -ForegroundColor Cyan

    # 1) core: loader, headers, CLI, demo, models, docs — no runtimes
    $coreDir = Join-Path $distPath "lw.PPOCR.Inference-core-win-x64"
    Clean-Output $coreDir
    & cmake --install $buildPath --prefix $coreDir --config $Configuration
    if ($LASTEXITCODE -ne 0) { throw "cmake --install core failed" }
    # remove all runtime directories
    $coreRuntimeRoot = Join-Path $coreDir "runtimes/win-x64"
    if (Test-Path $coreRuntimeRoot) {
        Get-ChildItem -Directory -Path $coreRuntimeRoot |
            ForEach-Object { Remove-Item -Recurse -Force $_.FullName }
    }
    & dotnet publish $cliProject -c Release -o $coreDir
    if ($LASTEXITCODE -ne 0) { throw "dotnet publish CLI failed" }
    & dotnet publish $demoProject -c Release -o $coreDir
    if ($LASTEXITCODE -ne 0) { throw "dotnet publish Demo failed" }
    Generate-Sha256 $coreDir
    Write-Host "  core: $coreDir" -ForegroundColor Green

    # 2) per-backend runtime packages
    $selectedRuntimes | ForEach-Object {
        $rt = $_
        $rtDir = Join-Path $distPath "lw.PPOCR.Runtime-$rt-win-x64"
        Clean-Output $rtDir

        # copy only this runtime's directory from the unified layout
        $rtSource = Join-Path $buildPath "bin"  # cmake --install source
        # re-run cmake install into a temp directory first
        $tempDir = Join-Path $distPath "_temp_$rt"
        Clean-Output $tempDir
        & cmake --install $buildPath --prefix $tempDir --config $Configuration
        if ($LASTEXITCODE -ne 0) { throw "cmake --install temp failed" }

        $tempRuntimeRoot = Join-Path $tempDir "runtimes/win-x64"
        $targetRuntimeRoot = Join-Path $rtDir "runtimes/win-x64"
        if (Test-Path (Join-Path $tempRuntimeRoot $rt)) {
            New-Item -ItemType Directory -Force -Path $targetRuntimeRoot | Out-Null
            Copy-Item -Recurse (Join-Path $tempRuntimeRoot $rt) $targetRuntimeRoot
        } else {
            Write-Warning "Runtime '$rt' was not found in build output — skipping"
        }

        # copy loader DLL (needed by all runtimes)
        $loaderDll = Get-ChildItem -Path $tempDir -Filter "lw.PPOCR.dll" -File |
            Select-Object -First 1
        if ($loaderDll) {
            Copy-Item $loaderDll.FullName $rtDir
        }

        # copy license
        if (Test-Path (Join-Path $tempDir "LICENSE")) {
            Copy-Item (Join-Path $tempDir "LICENSE") $rtDir
        }
        if (Test-Path (Join-Path $tempDir "licenses")) {
            Copy-Item -Recurse (Join-Path $tempDir "licenses") $rtDir
        }

        Remove-Item -Recurse -Force $tempDir
        Generate-Sha256 $rtDir
        Write-Host "  runtime $rt : $rtDir" -ForegroundColor Green
    }
}

# ----- main -----

if ($Split) {
    Build-Split
} else {
    Build-Unified
}
