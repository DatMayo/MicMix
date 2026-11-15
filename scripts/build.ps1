<#
.SYNOPSIS
    One-shot build for MicMix: fetch dependencies, configure, build, test and package.

.DESCRIPTION
    Runs the complete pipeline that CI and the release workflow use, so a local build
    produces exactly the same artifact:

      1. Check prerequisites (git, CMake, Visual Studio 2022+ C++ toolset)
      2. Fetch the TeamSpeak 3 plugin SDK and speexdsp at pinned commits into third_party/
      3. Configure with CMake (generator matching the installed Visual Studio, x64)
      4. Build
      5. Run the unit tests (ctest)
      6. Package dist/MicMix-<version>-win64.ts3_plugin (Release only)
      7. Optionally install the DLL into the local TeamSpeak 3 client (-Install)

    Works with Windows PowerShell 5.1 and PowerShell 7+.

.EXAMPLE
    .\scripts\build.ps1
    Full Release build with tests and .ts3_plugin package.

.EXAMPLE
    .\scripts\build.ps1 -Configuration Debug -SkipTests
    Debug build without tests (no package is created for Debug).

.EXAMPLE
    .\scripts\build.ps1 -Clean -Install
    Fresh Release build, then copy the plugin into %APPDATA%\TS3Client\plugins.
#>
[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Configuration = "Release",

    # Explicit version (e.g. 1.3.0). Defaults to the VERSION file.
    [string]$Version = "",

    # Delete the build directory before configuring.
    [switch]$Clean,

    [switch]$SkipTests,

    # Do not create the .ts3_plugin package.
    [switch]$NoPackage,

    # Copy the built plugin into the local TeamSpeak 3 client plugin folder.
    [switch]$Install,

    # CMake generator. Default: picked from the newest installed Visual Studio
    # (2022 -> "Visual Studio 17 2022", 2026 -> "Visual Studio 18 2026").
    [string]$Generator = "",

    [string]$Arch = "x64"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"
$thirdPartyDir = Join-Path $repoRoot "third_party"

# Pinned third-party revisions (keep in sync with THIRD_PARTY_NOTICES.md).
$dependencies = @(
    @{
        Name   = "TeamSpeak 3 plugin SDK"
        Repo   = "https://github.com/teamspeak/ts3client-pluginsdk.git"
        Dir    = "ts3_sdk"
        Commit = "4aa90a53aa150cbf81e13bc97e68c0431b26499f"
        Probe  = "include/ts3_functions.h"
    },
    @{
        Name   = "speexdsp"
        Repo   = "https://github.com/xiph/speexdsp.git"
        Dir    = "speexdsp"
        Commit = "7a158783df74efe7c2d1c6ee8363c1e695c71226"
        Probe  = "libspeexdsp/resample.c"
    }
)

$script:stepIndex = 0
$script:stepTotal = 7
$script:timer = [System.Diagnostics.Stopwatch]::StartNew()

function Write-Step {
    param([Parameter(Mandatory = $true)][string]$Title)
    $script:stepIndex++
    Write-Host ""
    Write-Host ("[{0}/{1}] {2}" -f $script:stepIndex, $script:stepTotal, $Title) -ForegroundColor Cyan
}

function Write-Info {
    param([string]$Message)
    Write-Host "      $Message" -ForegroundColor DarkGray
}

function Write-Skip {
    param([string]$Message)
    Write-Host "      skipped: $Message" -ForegroundColor DarkYellow
}

# Runs a native executable and throws on a non-zero exit code. Arguments are passed as an
# explicit array so tokens like "-S" or "--build" reach the executable verbatim.
function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$Exe,
        [string[]]$ArgumentList = @()
    )
    # Windows PowerShell 5.1 turns native stderr output into terminating errors under "Stop".
    $ErrorActionPreference = "Continue"
    & $Exe @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Exe $($ArgumentList -join ' ')"
    }
}

# Runs git and returns the first line of stdout (trimmed), or $null on failure. Never throws.
function Get-GitOutput {
    param([string[]]$ArgumentList = @())
    $ErrorActionPreference = "Continue"
    $output = & git @ArgumentList 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $output) {
        return $null
    }
    return ([string]($output | Select-Object -First 1)).Trim()
}

function Find-VsWhere {
    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\Installer\vswhere.exe")
    )
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }
    return $null
}

function Resolve-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    # Fall back to the CMake that ships with Visual Studio.
    $vswhere = Find-VsWhere
    if ($vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            $bundled = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path -LiteralPath $bundled) {
                return $bundled
            }
        }
    }
    throw "CMake not found. Install CMake 3.21+ (https://cmake.org/download/) or the 'C++ CMake tools' Visual Studio component."
}

# Finds the newest Visual Studio (2022 or later) with the C++ x64 toolset and, unless the
# caller passed -Generator, selects the matching CMake Visual Studio generator.
function Resolve-VisualStudio {
    if ($Generator -and $Generator -notlike "Visual Studio*") {
        return
    }
    $vswhere = Find-VsWhere
    if (-not $vswhere) {
        if (-not $Generator) {
            $script:Generator = "Visual Studio 17 2022"
        }
        Write-Warning "vswhere.exe not found; assuming '$script:Generator'. CMake will report missing toolsets."
        return
    }
    $ErrorActionPreference = "Continue"
    $vsJson = & $vswhere -latest -products * -version "[17.0," -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json 2>$null
    $ErrorActionPreference = "Stop"
    $vs = $null
    if ($vsJson) {
        $parsed = ($vsJson -join "`n") | ConvertFrom-Json
        $vs = @($parsed) | Select-Object -First 1
    }
    if (-not $vs) {
        throw "Visual Studio 2022 or newer with the 'Desktop development with C++' workload is required (Build Tools are enough)."
    }
    $major = [int](([string]$vs.installationVersion).Split(".")[0])
    $productYear = ""
    if ($vs.PSObject.Properties["catalog"] -and $vs.catalog.PSObject.Properties["productLineVersion"]) {
        $productYear = [string]$vs.catalog.productLineVersion
    }
    if (-not $Generator) {
        # Known majors map directly; VS 2026 reports productLineVersion "18", not a year.
        $knownYear = switch ($major) { 17 { "2022" } 18 { "2026" } default { "" } }
        if ($knownYear) {
            $productYear = $knownYear
        } elseif ($productYear -notmatch '^\d{4}$') {
            $productYear = ""
        }
        if (-not $productYear) {
            throw "Unknown Visual Studio version $($vs.installationVersion). Pass -Generator explicitly."
        }
        $script:Generator = "Visual Studio $major $productYear"
    }
    Write-Info "Visual Studio: $($vs.displayName) $($vs.installationVersion)"
    Write-Info "Generator:     $script:Generator"
}

function Sync-Dependency {
    param([Parameter(Mandatory = $true)][hashtable]$Dep)

    $target = Join-Path $thirdPartyDir $Dep.Dir
    $probe = Join-Path $target $Dep.Probe

    if (Test-Path -LiteralPath (Join-Path $target ".git")) {
        $head = Get-GitOutput -ArgumentList @("-C", $target, "rev-parse", "HEAD")
        if ($head -eq $Dep.Commit -and (Test-Path -LiteralPath $probe)) {
            Write-Info ("{0}: ok ({1})" -f $Dep.Name, $Dep.Commit.Substring(0, 12))
            return
        }
        Write-Info ("{0}: switching to pinned commit {1}" -f $Dep.Name, $Dep.Commit.Substring(0, 12))
        $hasCommit = Get-GitOutput -ArgumentList @("-C", $target, "cat-file", "-t", $Dep.Commit)
        if ($hasCommit -ne "commit") {
            Invoke-Native -Exe "git" -ArgumentList @("-C", $target, "fetch", "--quiet", "origin")
        }
        Invoke-Native -Exe "git" -ArgumentList @("-C", $target, "-c", "advice.detachedHead=false", "checkout", "--quiet", $Dep.Commit)
        return
    }

    if (Test-Path -LiteralPath $probe) {
        # Present but not a git checkout (e.g. extracted archive): trust it.
        Write-Info ("{0}: using existing copy (not a git checkout, revision not verified)" -f $Dep.Name)
        return
    }

    $maxAttempts = 3
    for ($attempt = 1; $attempt -le $maxAttempts; $attempt++) {
        Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction SilentlyContinue
        Write-Info ("{0}: cloning (attempt {1}/{2})" -f $Dep.Name, $attempt, $maxAttempts)
        try {
            Invoke-Native -Exe "git" -ArgumentList @("clone", "--quiet", $Dep.Repo, $target)
            break
        } catch {
            if ($attempt -eq $maxAttempts) {
                throw "git clone failed after $maxAttempts attempts: $($Dep.Repo)"
            }
            Start-Sleep -Seconds (2 * $attempt)
        }
    }
    Invoke-Native -Exe "git" -ArgumentList @("-C", $target, "-c", "advice.detachedHead=false", "checkout", "--quiet", $Dep.Commit)
    if (-not (Test-Path -LiteralPath $probe)) {
        throw "$($Dep.Name) checkout is missing $($Dep.Probe)"
    }
}

function Get-TeamSpeakPluginDir {
    if (-not $env:APPDATA) {
        throw "APPDATA is not set; cannot locate the TeamSpeak 3 client folder."
    }
    return (Join-Path $env:APPDATA "TS3Client\plugins")
}

try {
    Set-Location -LiteralPath $repoRoot

    if ([string]::IsNullOrWhiteSpace($Version)) {
        $Version = (Get-Content -LiteralPath (Join-Path $repoRoot "VERSION") -Raw).Trim()
    }
    if ($Version -notmatch '^[0-9A-Za-z][0-9A-Za-z._-]{0,63}$') {
        throw "Invalid version '$Version'. Allowed characters: A-Z a-z 0-9 . _ -"
    }

    Write-Host "MicMix build  v$Version  ($Configuration, $Arch)" -ForegroundColor White

    # 1 -------------------------------------------------------------------------
    Write-Step "Checking prerequisites"
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        throw "git not found. Install Git for Windows (https://git-scm.com/download/win)."
    }
    $cmake = Resolve-CMake
    $ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
    if (-not (Test-Path -LiteralPath $ctest)) {
        $ctestCmd = Get-Command ctest -ErrorAction SilentlyContinue
        if ($ctestCmd) { $ctest = $ctestCmd.Source } else { $ctest = "ctest" }
    }
    Write-Info "CMake: $cmake"
    Resolve-VisualStudio

    # 2 -------------------------------------------------------------------------
    Write-Step "Fetching third-party dependencies"
    New-Item -ItemType Directory -Path $thirdPartyDir -Force | Out-Null
    foreach ($dep in $dependencies) {
        Sync-Dependency -Dep $dep
    }

    # 3 -------------------------------------------------------------------------
    Write-Step "Configuring (CMake)"
    if ($Clean -and (Test-Path -LiteralPath $buildDir)) {
        Write-Info "Removing $buildDir"
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }
    $configureArgs = @("-S", $repoRoot, "-B", $buildDir, "-G", $Generator)
    if ($Generator -like "Visual Studio*") {
        $configureArgs += @("-A", $Arch)
    } else {
        $configureArgs += "-DCMAKE_BUILD_TYPE=$Configuration"
    }
    $configureArgs += "-DMICMIX_VERSION_OVERRIDE=$Version"
    $configureArgs += ("-DMICMIX_BUILD_TESTS=" + $(if ($SkipTests) { "OFF" } else { "ON" }))
    Invoke-Native -Exe $cmake -ArgumentList $configureArgs

    # 4 -------------------------------------------------------------------------
    Write-Step "Building"
    Invoke-Native -Exe $cmake -ArgumentList @("--build", $buildDir, "--config", $Configuration, "--parallel")

    # 5 -------------------------------------------------------------------------
    Write-Step "Running tests"
    if ($SkipTests) {
        Write-Skip "-SkipTests"
    } else {
        Invoke-Native -Exe $ctest -ArgumentList @("--test-dir", $buildDir, "-C", $Configuration, "--output-on-failure")
    }

    $dll = Join-Path $buildDir "$Configuration\micmix.dll"
    if (-not (Test-Path -LiteralPath $dll)) {
        $dll = Join-Path $buildDir "micmix.dll"
    }
    if (-not (Test-Path -LiteralPath $dll)) {
        throw "Build finished but micmix.dll was not found under $buildDir"
    }

    # 6 -------------------------------------------------------------------------
    Write-Step "Packaging"
    $packagePath = $null
    if ($NoPackage) {
        Write-Skip "-NoPackage"
    } elseif ($Configuration -ne "Release") {
        Write-Skip "packages are only created for Release builds"
    } else {
        & (Join-Path $PSScriptRoot "package_ts3_plugin.ps1") -Version $Version -Configuration $Configuration
        $packagePath = Join-Path $repoRoot "dist\MicMix-$Version-win64.ts3_plugin"
    }

    # 7 -------------------------------------------------------------------------
    Write-Step "Installing into TeamSpeak 3"
    if (-not $Install) {
        Write-Skip "pass -Install to copy the plugin into your TeamSpeak 3 client"
    } else {
        $running = Get-Process -Name "ts3client_win64", "ts3client_win32" -ErrorAction SilentlyContinue
        if ($running) {
            throw "TeamSpeak 3 is running. Close it first so the plugin DLL can be replaced."
        }
        $pluginDir = Get-TeamSpeakPluginDir
        $iconDir = Join-Path $pluginDir "micmix_win64"
        New-Item -ItemType Directory -Path $iconDir -Force | Out-Null
        Copy-Item -LiteralPath $dll -Destination (Join-Path $pluginDir "micmix_win64.dll") -Force
        $icon16 = Join-Path $repoRoot "assets\branding\MicMixIcon16.png"
        foreach ($name in @("1.png", "t.png", "menu_icon.png")) {
            Copy-Item -LiteralPath $icon16 -Destination (Join-Path $iconDir $name) -Force
        }
        Write-Info "Installed to $pluginDir"
    }

    $script:timer.Stop()
    Write-Host ""
    Write-Host ("Build succeeded in {0:N1}s" -f $script:timer.Elapsed.TotalSeconds) -ForegroundColor Green
    Write-Host "  DLL:     $dll"
    if ($packagePath) {
        Write-Host "  Package: $packagePath"
    }
    exit 0
} catch {
    Write-Host ""
    Write-Host "Build FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
