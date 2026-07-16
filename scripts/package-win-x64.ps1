param(
    [string]$BuildDirectory = "build/ninja-x64",
    [string]$OutputDirectory = "dist/lw.PPOCR.Inference-win-x64",
    [string]$Configuration = "Release",
    [string]$Runtime = "",        # comma-separated: opencv,directml,openvino,tensorrt
    [switch]$Split,               # if set, produce per-backend packages
    [switch]$Archive              # also create a versioned ZIP + SHA-256
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildPath   = [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
$distPath    = [IO.Path]::GetFullPath((Join-Path $projectRoot "dist"))
$cliProject  = Join-Path $projectRoot "bindings/dotnet/samples/UnifiedCli/UnifiedCli.csproj"
$demoProject = Join-Path $projectRoot "bindings/dotnet/samples/UnifiedDemo/UnifiedDemo.csproj"
$demoQuickStart = Join-Path $projectRoot "bindings/dotnet/samples/UnifiedDemo/QUICKSTART.md"
$cmakeText   = Get-Content (Join-Path $projectRoot "CMakeLists.txt") -Raw
$versionMatch = [regex]::Match(
    $cmakeText,
    'project\(lw\.PPOCR\.Inference\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $versionMatch.Success) {
    throw "Could not determine the project version from CMakeLists.txt"
}
$projectVersion = $versionMatch.Groups[1].Value

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

function Verify-Sha256($directory, $manifestName = "package-files.sha256") {
    $manifestPath = Join-Path $directory $manifestName
    foreach ($line in Get-Content -LiteralPath $manifestPath) {
        if ($line -notmatch '^([a-f0-9]{64})  (.+)$') {
            throw "Malformed SHA-256 manifest line: $line"
        }
        $path = Join-Path $directory $matches[2]
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "SHA-256 manifest references a missing file: $($matches[2])"
        }
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $matches[1]) {
            throw "SHA-256 mismatch: $($matches[2])"
        }
    }
}

function Assert-Package($directory, $kind, $runtimes = @()) {
    $required = @(
        "lw.PPOCR.dll",
        "LICENSE",
        "licenses/clipper-BSL-1.0.txt",
        "licenses/opencv-APACHE-2.0.txt",
        "licenses/onnxruntime-MIT.txt",
        "licenses/onnxruntime-ThirdPartyNotices.txt",
        "licenses/DirectML-MIT.txt",
        "licenses/openvino-APACHE-2.0.txt",
        "licenses/openvino-EULA.txt",
        "licenses/TensorRT-License-Reference.txt",
        "licenses/TensorRT-ThirdPartyNotices.txt",
        "licenses/CUDA-EULA.txt",
        "licenses/PaddleOCR-models-APACHE-2.0.txt",
        "licenses/cpp-httplib-MIT.txt",
        "licenses/nlohmann-json-MIT.txt"
    )
    if ($kind -in @("unified", "core")) {
        $required += @(
            "include/lw/ppocr/ppocr_api.h",
            "docs/abi-v1-stability.md",
            "docs/license-audit.md",
            "docs/http-service.md",
            "docs/v1.0.0.md",
            "docs/v1.1.0.md",
            "models/model-manifest.schema.json",
            "examples/c/main.c",
            "examples/python/ppocr.py",
            "lw.PPOCR.HttpService.exe",
            "http-service.json",
            "run-http-service.cmd",
            "install-service.cmd",
            "uninstall-service.cmd",
            "www/index.html"
        )
    }
    if ($kind -eq "runtime") {
        $required += @(
            "lw.PPOCR.Demo.exe",
            "demo-backend.txt",
            "QUICKSTART.md",
            "lw.PPOCR.HttpService.exe",
            "http-service.json",
            "run-http-service.cmd",
            "install-service.cmd",
            "uninstall-service.cmd",
            "www/index.html",
            "models/ppocrv6-tiny/model.json",
            "models/ppocrv6-tiny/sample.jpg"
        )
    }
    foreach ($relative in $required) {
        if (-not (Test-Path -LiteralPath (Join-Path $directory $relative) -PathType Leaf)) {
            throw "Package is missing required file: $relative"
        }
    }

    $loaderVersion = (Get-Item (Join-Path $directory "lw.PPOCR.dll")).VersionInfo.FileVersion
    if ($loaderVersion -ne $projectVersion) {
        throw "Loader version is $loaderVersion; expected $projectVersion"
    }
    $httpServicePath = Join-Path $directory "lw.PPOCR.HttpService.exe"
    if (Test-Path -LiteralPath $httpServicePath -PathType Leaf) {
        $httpServiceVersion = (Get-Item $httpServicePath).VersionInfo.FileVersion
        if ($httpServiceVersion -ne $projectVersion) {
            throw "HTTP service version is $httpServiceVersion; expected $projectVersion"
        }
    }

    foreach ($runtime in $runtimes) {
        $runtimeDirectory = Join-Path $directory "runtimes/win-x64/$runtime"
        $runtimeDll = Get-ChildItem -LiteralPath $runtimeDirectory -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "lw.PPOCR.Runtime.*.dll" } |
            Select-Object -First 1
        if (-not $runtimeDll) {
            throw "Package is missing the '$runtime' Runtime DLL"
        }
        if ($runtimeDll.VersionInfo.FileVersion -ne $projectVersion) {
            throw "$($runtimeDll.Name) version is $($runtimeDll.VersionInfo.FileVersion); expected $projectVersion"
        }
    }
}

function New-ReleaseArchive($directory, $packageName) {
    $releaseDirectory = Join-Path $distPath "releases/v$projectVersion"
    New-Item -ItemType Directory -Force -Path $releaseDirectory | Out-Null
    $archiveName = "$packageName-v$projectVersion-win-x64.zip"
    $archivePath = Join-Path $releaseDirectory $archiveName
    $entries = Get-ChildItem -LiteralPath $directory -Force |
        Where-Object { $_.Name -ne "test" }
    Compress-Archive -LiteralPath $entries.FullName -DestinationPath $archivePath -Force
    $hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText(
        "$archivePath.sha256",
        "$hash  $archiveName`n",
        [Text.UTF8Encoding]::new($false))
    $releaseChecksums = Get-ChildItem -LiteralPath $releaseDirectory -Filter "*.zip" -File |
        Sort-Object Name |
        ForEach-Object {
            $archiveHash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            "$archiveHash  $($_.Name)"
        }
    [IO.File]::WriteAllLines(
        (Join-Path $releaseDirectory "SHA256SUMS.txt"),
        $releaseChecksums,
        [Text.UTF8Encoding]::new($false))
    Write-Host "Release archive created: $archivePath" -ForegroundColor Green
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
    & dotnet publish $demoProject -c Release -f net472 -o $outputPath
    if ($LASTEXITCODE -ne 0) { throw "dotnet publish Demo failed" }

    Assert-Package $outputPath "unified" $selectedRuntimes
    Generate-Sha256 $outputPath
    Verify-Sha256 $outputPath
    if ($Archive) {
        New-ReleaseArchive $outputPath "lw.PPOCR.Inference"
    }
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
    & dotnet publish $demoProject -c Release -f net472 -o $coreDir
    if ($LASTEXITCODE -ne 0) { throw "dotnet publish Demo failed" }
    Assert-Package $coreDir "core"
    Generate-Sha256 $coreDir
    Verify-Sha256 $coreDir
    if ($Archive) {
        New-ReleaseArchive $coreDir "lw.PPOCR.Inference-core"
    }
    Write-Host "  core: $coreDir" -ForegroundColor Green

    # Build one lightweight .NET Framework WinForms demo and reuse it in every
    # backend package. Supported Windows 10/11 installations already provide a
    # compatible .NET Framework 4.x runtime.
    $demoStaging = Join-Path $distPath "_split_demo_win-x64"
    Clean-Output $demoStaging
    & dotnet publish $demoProject -c Release -f net472 -o $demoStaging
    if ($LASTEXITCODE -ne 0) { throw "dotnet publish .NET Framework Demo failed" }

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

        foreach ($httpFile in @(
            "lw.PPOCR.HttpService.exe",
            "http-service.json",
            "run-http-service.cmd",
            "install-service.cmd",
            "uninstall-service.cmd")) {
            Copy-Item -LiteralPath (Join-Path $tempDir $httpFile) -Destination $rtDir
        }
        Copy-Item -LiteralPath (Join-Path $tempDir "www") -Destination $rtDir -Recurse

        $httpConfigPath = Join-Path $rtDir "http-service.json"
        $httpConfig = Get-Content -LiteralPath $httpConfigPath -Raw | ConvertFrom-Json
        $httpConfig.backend = $rt
        $httpConfig.service_name = "lw.PPOCR.HttpService.$rt"
        $httpConfig.service_display_name = "lw.PPOCR HTTP OCR Service ($rt)"
        if ($rt -eq "directml") {
            $httpConfig | Add-Member -Force NoteProperty backend_options `
                ([pscustomobject]@{ use_gpu = 1 })
        } elseif ($rt -eq "openvino") {
            $httpConfig | Add-Member -Force NoteProperty backend_options `
                ([pscustomobject]@{ device = "CPU" })
        } else {
            $httpConfig.PSObject.Properties.Remove("backend_options")
        }
        [IO.File]::WriteAllText(
            $httpConfigPath,
            ($httpConfig | ConvertTo-Json -Depth 8) + "`n",
            [Text.UTF8Encoding]::new($false))

        # Lightweight WinForms experience app, preset backend, and the
        # ready-to-run sample model/image.
        Get-ChildItem -LiteralPath $demoStaging -Force |
            Copy-Item -Destination $rtDir -Recurse -Force
        [IO.File]::WriteAllText(
            (Join-Path $rtDir "demo-backend.txt"),
            "$rt`n",
            [Text.UTF8Encoding]::new($false))
        Copy-Item -LiteralPath $demoQuickStart -Destination (Join-Path $rtDir "QUICKSTART.md")

        $sampleModelSource = Join-Path $tempDir "models/ppocrv6-tiny"
        if (-not (Test-Path -LiteralPath $sampleModelSource -PathType Container)) {
            throw "The sample model was not found in the install tree"
        }
        $modelRoot = Join-Path $rtDir "models"
        New-Item -ItemType Directory -Force -Path $modelRoot | Out-Null
        Copy-Item -LiteralPath $sampleModelSource -Destination $modelRoot -Recurse

        Remove-Item -Recurse -Force $tempDir
        Assert-Package $rtDir "runtime" @($rt)
        Generate-Sha256 $rtDir
        Verify-Sha256 $rtDir
        if ($Archive) {
            New-ReleaseArchive $rtDir "lw.PPOCR.Runtime-$rt"
        }
        Write-Host "  runtime $rt : $rtDir" -ForegroundColor Green
    }

    Remove-Item -LiteralPath $demoStaging -Recurse -Force
}

# ----- main -----

if ($Split) {
    Build-Split
} else {
    Build-Unified
}
