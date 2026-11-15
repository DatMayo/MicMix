<#
.SYNOPSIS
    Cuts a MicMix release from the changes since the last release tag.

.DESCRIPTION
    1. Collects the commits since the last tag `v*` (or all commits if there is none).
    2. Picks the next version per VERSIONING.md (Conventional Commits):
         breaking change (`type!:` or `BREAKING CHANGE`) -> MAJOR
         any `feat`                                       -> MINOR
         anything else                                    -> PATCH
       Override with -Bump or -Version.
    3. Updates CHANGELOG.md: a filled "## [Unreleased]" section becomes the new version;
       without one, a section is generated from the commit subjects.
    4. Writes VERSION and the default version in CMakeLists.txt.
    5. Builds, tests and packages with scripts/build.ps1 (skip with -SkipBuild).
    6. Commits "chore(release): vX.Y.Z" and creates the annotated tag vX.Y.Z.
    7. With -Push: pushes the branch and the tag. The "Release" GitHub workflow then
       builds the plugin and publishes the GitHub release with the .ts3_plugin.

    Works with Windows PowerShell 5.1 and PowerShell 7+ (also on Linux/macOS with -SkipBuild).

.EXAMPLE
    .\scripts\release.ps1 -DryRun
    Show the next version and the changelog section without changing anything.

.EXAMPLE
    .\scripts\release.ps1 -Push
    Release the latest changes and publish them via GitHub Actions.

.EXAMPLE
    .\scripts\release.ps1 -Bump minor -Push
#>
[CmdletBinding()]
param(
    [ValidateSet("auto", "patch", "minor", "major")]
    [string]$Bump = "auto",

    # Explicit version (X.Y.Z); overrides -Bump.
    [string]$Version = "",

    # Push the release commit and tag to the remote.
    [switch]$Push,

    # Do not run scripts/build.ps1 before committing.
    [switch]$SkipBuild,

    # Print what would happen, change nothing.
    [switch]$DryRun,

    # Allow uncommitted changes in the working tree.
    [switch]$AllowDirty,

    [string]$Remote = "origin"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

function Invoke-Git {
    param([string[]]$ArgumentList)
    $ErrorActionPreference = "Continue"
    $output = & git @ArgumentList 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "git $($ArgumentList -join ' ') failed: $($output -join "`n")"
    }
    return $output
}

function Get-GitLines {
    param([string[]]$ArgumentList)
    $ErrorActionPreference = "Continue"
    $output = & git @ArgumentList 2>$null
    if ($LASTEXITCODE -ne 0) {
        return @()
    }
    # Callers wrap the call in @() so empty or single results are still arrays.
    return @($output | Where-Object { $_ -ne $null })
}

function ConvertTo-SemVer {
    param([string]$Text)
    if ($Text -notmatch '^(\d+)\.(\d+)\.(\d+)$') {
        throw "Version '$Text' is not in MAJOR.MINOR.PATCH form."
    }
    return @([int]$Matches[1], [int]$Matches[2], [int]$Matches[3])
}

# Conventional commit: "type(scope)!: description"
$ccPattern = '^(?<type>[a-zA-Z]+)(\((?<scope>[^)]*)\))?(?<bang>!)?:\s*(?<desc>.+)$'

function Get-CommitInfo {
    param([string]$Subject, [string]$Body, [string]$Hash)
    $info = [ordered]@{ Type = "other"; Scope = ""; Breaking = $false; Description = $Subject; Hash = $Hash }
    if ($Subject -match $ccPattern) {
        $info.Type = $Matches['type'].ToLowerInvariant()
        $info.Scope = $Matches['scope']
        $info.Breaking = [bool]$Matches['bang']
        $info.Description = $Matches['desc']
    }
    if ($Body -match 'BREAKING[ -]CHANGE') {
        $info.Breaking = $true
    }
    return [pscustomobject]$info
}

try {
    Set-Location -LiteralPath $repoRoot

    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        throw "git not found."
    }

    if (-not $AllowDirty -and -not $DryRun) {
        $dirty = @(Get-GitLines -ArgumentList @("status", "--porcelain", "--untracked-files=no"))
        if ($dirty.Count -gt 0) {
            throw "Working tree has uncommitted changes. Commit or stash them, or pass -AllowDirty."
        }
    }

    $branch = @(Get-GitLines -ArgumentList @("rev-parse", "--abbrev-ref", "HEAD")) | Select-Object -First 1
    if (-not $branch -or $branch -eq "HEAD") {
        throw "Detached HEAD: check out the branch you want to release from."
    }

    # 1. Commits since the last release tag ------------------------------------------
    $lastTag = @(Get-GitLines -ArgumentList @("describe", "--tags", "--abbrev=0", "--match", "v*")) | Select-Object -First 1
    $range = if ($lastTag) { "$lastTag..HEAD" } else { "HEAD" }
    $sep = [char]0x1f
    $end = [char]0x1e
    $raw = @(Get-GitLines -ArgumentList @("log", $range, "--no-merges", "--format=%h$sep%s$sep%b$end")) -join "`n"
    $commits = @()
    foreach ($entry in ($raw -split [regex]::Escape([string]$end))) {
        $entry = $entry.Trim()
        if (-not $entry) { continue }
        $parts = $entry -split [regex]::Escape([string]$sep), 3
        $hash = $parts[0]
        $subject = if ($parts.Count -gt 1) { $parts[1].Trim() } else { "" }
        $body = if ($parts.Count -gt 2) { $parts[2] } else { "" }
        if ($subject -match '^chore\(release\)') { continue }
        $commits += Get-CommitInfo -Subject $subject -Body $body -Hash $hash
    }

    $changelogPath = Join-Path $repoRoot "CHANGELOG.md"
    $changelog = if (Test-Path -LiteralPath $changelogPath) { [System.IO.File]::ReadAllText($changelogPath) } else { "# Changelog`n" }
    $changelog = $changelog -replace "`r`n", "`n"
    $unreleasedRegex = [regex]'(?ms)^## \[Unreleased\][^\n]*\n(?<body>.*?)(?=^## \[|\z)'
    $unreleasedMatch = $unreleasedRegex.Match($changelog)
    $hasCuratedNotes = $unreleasedMatch.Success -and ($unreleasedMatch.Groups['body'].Value.Trim().Length -gt 0)

    if ($commits.Count -eq 0 -and -not $hasCuratedNotes) {
        throw "Nothing to release: no commits since $(if ($lastTag) { $lastTag } else { 'the beginning' })."
    }

    # 2. Next version -------------------------------------------------------------
    $current = ([System.IO.File]::ReadAllText((Join-Path $repoRoot "VERSION"))).Trim()
    $sem = ConvertTo-SemVer $current
    if ($lastTag) {
        $tagVersion = $lastTag.TrimStart('v')
        if ($tagVersion -match '^\d+\.\d+\.\d+$') {
            $sem = ConvertTo-SemVer $tagVersion
            $current = $tagVersion
        }
    }

    $effectiveBump = $Bump
    if ($effectiveBump -eq "auto") {
        $effectiveBump = "patch"
        if (@($commits | Where-Object { $_.Type -eq "feat" }).Count -gt 0) { $effectiveBump = "minor" }
        if (@($commits | Where-Object { $_.Breaking }).Count -gt 0) { $effectiveBump = "major" }
    }

    if ($Version) {
        ConvertTo-SemVer $Version | Out-Null
        $newVersion = $Version
        $effectiveBump = "explicit"
    } else {
        switch ($effectiveBump) {
            "major" { $newVersion = "{0}.0.0" -f ($sem[0] + 1) }
            "minor" { $newVersion = "{0}.{1}.0" -f $sem[0], ($sem[1] + 1) }
            default { $newVersion = "{0}.{1}.{2}" -f $sem[0], $sem[1], ($sem[2] + 1) }
        }
    }
    $tag = "v$newVersion"
    if (@(Get-GitLines -ArgumentList @("tag", "--list", $tag)).Count -gt 0) {
        throw "Tag $tag already exists."
    }

    # 3. Changelog section ----------------------------------------------------------
    $date = Get-Date -Format "yyyy-MM-dd"
    if ($hasCuratedNotes) {
        $sectionBody = $unreleasedMatch.Groups['body'].Value.Trim()
        $source = "curated [Unreleased] notes"
    } else {
        $groups = [ordered]@{ "Breaking" = @(); "Added" = @(); "Fixed" = @(); "Changed" = @() }
        foreach ($c in $commits) {
            if (-not $c.Description) { continue }
            $text = $c.Description.Substring(0, 1).ToUpperInvariant() + $c.Description.Substring(1)
            $line = "- $text ($($c.Hash))"
            if ($c.Breaking) { $groups["Breaking"] += $line }
            if ($c.Type -eq "feat") { $groups["Added"] += $line }
            elseif ($c.Type -eq "fix") { $groups["Fixed"] += $line }
            else { $groups["Changed"] += $line }
        }
        $blocks = @()
        foreach ($name in $groups.Keys) {
            if ($groups[$name].Count -gt 0) {
                $blocks += "### $name`n`n" + ($groups[$name] -join "`n")
            }
        }
        $sectionBody = $blocks -join "`n`n"
        $source = "$($commits.Count) commit(s) since $(if ($lastTag) { $lastTag } else { 'the beginning' })"
    }
    $section = "## [$newVersion] - $date`n`n$sectionBody`n"

    Write-Host "Release   : $current -> $newVersion ($effectiveBump)" -ForegroundColor Cyan
    Write-Host "Branch    : $branch"
    Write-Host "Notes from: $source"
    Write-Host ""
    Write-Host $section

    if ($DryRun) {
        Write-Host "Dry run: nothing was changed." -ForegroundColor Yellow
        exit 0
    }

    $replacement = "## [Unreleased]`n`n$section`n"
    if ($unreleasedMatch.Success) {
        # Evaluator instead of a replacement string: "$" in the notes must stay literal.
        $evaluator = [System.Text.RegularExpressions.MatchEvaluator] ({ param($m) $replacement }.GetNewClosure())
        $changelog = $unreleasedRegex.Replace($changelog, $evaluator, 1)
    } else {
        $firstSection = [regex]::Match($changelog, '(?m)^## \[')
        if ($firstSection.Success) {
            $changelog = $changelog.Insert($firstSection.Index, "## [Unreleased]`n`n$section`n")
        } else {
            $changelog = $changelog.TrimEnd() + "`n`n## [Unreleased]`n`n$section"
        }
    }
    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($changelogPath, $changelog, $utf8)

    # 4. Version files -----------------------------------------------------------------
    [System.IO.File]::WriteAllText((Join-Path $repoRoot "VERSION"), "$newVersion`n", $utf8)
    $cmakePath = Join-Path $repoRoot "CMakeLists.txt"
    $cmake = [System.IO.File]::ReadAllText($cmakePath)
    $cmake = [regex]::Replace($cmake, 'set\(MICMIX_VERSION "[^"]*"\)', "set(MICMIX_VERSION `"$newVersion`")", 1)
    [System.IO.File]::WriteAllText($cmakePath, $cmake, $utf8)

    # 5. Build ---------------------------------------------------------------------------
    if (-not $SkipBuild) {
        & (Join-Path $PSScriptRoot "build.ps1") -Configuration Release -Version $newVersion
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed; the version files were updated but nothing was committed. Revert with: git checkout -- VERSION CMakeLists.txt CHANGELOG.md"
        }
    }

    # 6. Commit and tag ------------------------------------------------------------------
    Invoke-Git -ArgumentList @("add", "VERSION", "CMakeLists.txt", "CHANGELOG.md") | Out-Null
    Invoke-Git -ArgumentList @("commit", "--quiet", "-m", "chore(release): $tag") | Out-Null
    Invoke-Git -ArgumentList @("tag", "-a", $tag, "-m", "MicMix $newVersion") | Out-Null
    Write-Host "Created commit and tag $tag" -ForegroundColor Green

    # 7. Push ----------------------------------------------------------------------------
    if ($Push) {
        Invoke-Git -ArgumentList @("push", $Remote, "HEAD:refs/heads/$branch") | Out-Null
        Invoke-Git -ArgumentList @("push", $Remote, "refs/tags/$tag") | Out-Null
        Write-Host "Pushed $branch and $tag to $Remote." -ForegroundColor Green
    } else {
        Write-Host "Not pushed. Publish with: git push $Remote $branch; git push $Remote $tag"
    }

    if ($env:GITHUB_OUTPUT) {
        Add-Content -LiteralPath $env:GITHUB_OUTPUT -Value "version=$newVersion"
        Add-Content -LiteralPath $env:GITHUB_OUTPUT -Value "tag=$tag"
    }
    exit 0
} catch {
    Write-Host ""
    Write-Host "Release FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
