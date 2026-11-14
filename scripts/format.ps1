<#
.SYNOPSIS
Formats StarGBC's C++ sources with clang-format.

.DESCRIPTION
Runs clang-format (using the repository's .clang-format) over every .h, .hpp and .cpp
file under core, diagnostics, frontends, scripting and tests, then undoes the spacing
clang-format inserts into C++26 reflection annotations ([[= Foo]] -> [[=Foo]]).

Every file is fed through stdin with a ".cpp" assumed filename. The [[=...]] annotations
make clang-format guess that .h files are Objective-C and refuse to format them, and the
assumed filename forces C++ while still locating the repository's .clang-format.

Files are written back as UTF-8 without a byte-order mark, with line endings preserved.

.PARAMETER Path
Files or directories to format. Defaults to the source trees listed above.

.PARAMETER Check
Lists files that are not formatted and exits with code 1 instead of rewriting them.

.PARAMETER ClangFormat
Path to clang-format.exe. Defaults to $env:CLANG_FORMAT, then the LLVM installation under
Program Files, then PATH, then the copy bundled with Visual Studio.

.EXAMPLE
.\scripts\format.ps1
Formats the whole repository.

.EXAMPLE
.\scripts\format.ps1 -Check
Reports unformatted files without changing them (for CI or pre-commit checks).

.EXAMPLE
.\scripts\format.ps1 core\src\Audio.cpp tests
Formats one file and one directory.
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$Path,
    [switch]$Check,
    [string]$ClangFormat
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourceDirs = 'core', 'diagnostics', 'frontends', 'scripting', 'tests'
$extensions = '.h', '.hpp', '.cpp'
$utf8NoBom = New-Object System.Text.UTF8Encoding $false

function Find-ClangFormat {
    param([string]$Override)

    $candidates = @()
    if ($Override) { $candidates += $Override }
    if ($env:CLANG_FORMAT) { $candidates += $env:CLANG_FORMAT }
    $candidates += Join-Path $env:ProgramFiles 'LLVM\bin\clang-format.exe'
    $onPath = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($onPath) { $candidates += $onPath.Source }
    $candidates += Get-ChildItem (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\*\*\VC\Tools\Llvm\x64\bin\clang-format.exe') -ErrorAction SilentlyContinue |
        ForEach-Object FullName

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'clang-format not found. Install LLVM, add clang-format to PATH, or pass -ClangFormat / set CLANG_FORMAT.'
}

function Get-SourceFiles {
    param([string[]]$Paths)

    $files = foreach ($p in $Paths) {
        $item = Get-Item -LiteralPath $p
        if ($item.PSIsContainer) {
            Get-ChildItem -LiteralPath $item.FullName -Recurse -File | Where-Object { $extensions -contains $_.Extension }
        } else {
            $item
        }
    }
    $files | Sort-Object FullName -Unique
}

function Invoke-ClangFormat {
    param([string]$Exe, [string]$File, [string]$Content)

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Exe
    $startInfo.Arguments = "--style=file --assume-filename=""$File.cpp"""
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardOutputEncoding = $utf8NoBom
    $startInfo.StandardErrorEncoding = $utf8NoBom

    $process = [System.Diagnostics.Process]::Start($startInfo)
    # Write stdin through our own UTF-8 writer: the default StandardInput writer uses the console code page.
    $stdin = New-Object System.IO.StreamWriter $process.StandardInput.BaseStream, $utf8NoBom
    $stdin.Write($Content)
    $stdin.Close()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    if ($process.ExitCode -ne 0) {
        throw "clang-format failed on ${File}:`n$stderr"
    }
    return $stdout
}

function Repair-Annotations {
    param([string]$Text)

    # clang-format does not understand C++26 reflection annotations and spaces them out:
    # [[=NotStateAware]] becomes [[= NotStateAware]] and [[=A, =B]] becomes [[= A, = B]].
    [regex]::Replace($Text, '\[\[=[^\]]*\]\]', { param($match) $match.Value -replace '(?<=^\[\[|,\s*)=\s+', '=' })
}

$exe = Find-ClangFormat $ClangFormat
if (-not $Path) {
    $Path = $sourceDirs | ForEach-Object { Join-Path $root $_ } | Where-Object { Test-Path -LiteralPath $_ -PathType Container }
}
$files = @(Get-SourceFiles $Path)
if ($files.Count -eq 0) {
    Write-Warning 'No .h, .hpp or .cpp files found.'
    exit 0
}

$unformatted = @()
foreach ($file in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $hadBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
    $original = [System.IO.File]::ReadAllText($file.FullName)   # strips the BOM if present
    $formatted = Repair-Annotations (Invoke-ClangFormat $exe $file.FullName $original)

    if ($formatted -ne $original -or $hadBom) {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\', '/')
        $unformatted += $relative
        if (-not $Check) {
            [System.IO.File]::WriteAllText($file.FullName, $formatted, $utf8NoBom)
            Write-Host "Formatted $relative"
        }
    }
}

if ($Check) {
    if ($unformatted.Count -gt 0) {
        $unformatted | ForEach-Object { Write-Host "Needs formatting: $_" }
        Write-Host "$($unformatted.Count) of $($files.Count) files need formatting."
        exit 1
    }
    Write-Host "All $($files.Count) files are formatted."
} else {
    Write-Host "Formatted $($unformatted.Count) of $($files.Count) files."
}
