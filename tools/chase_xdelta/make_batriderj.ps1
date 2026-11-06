# Builds Armed Police Batrider CHASE!'s batriderj.zip for MAME from your own MAME batrider ROMs and the four
# xdelta patches beside this script. Usage (or double-click make_batriderj.bat):
#   powershell -ExecutionPolicy Bypass -File make_batriderj.ps1 [-Roms <folder with batrider.zip>] [-Xdelta <xdelta3.exe>]
# Needs xdelta3.exe (https://github.com/jmacd/xdelta-gpl/releases) beside this script, on the PATH, or given.
# Reads batrider.zip (merged set: batriderj\prg0b.u22 inside) or, for a split set, batrider.zip + batriderj.zip.
# Writes out\batriderj.zip beside this script; your own ROM files are never changed.
param([string]$Roms = $PSScriptRoot, [string]$Xdelta = "")
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (-not $Xdelta) {
	$Xdelta = Join-Path $PSScriptRoot "xdelta3.exe"
	if (-not (Test-Path $Xdelta)) {
		$c = Get-Command xdelta3 -ErrorAction SilentlyContinue
		if ($c) { $Xdelta = $c.Source } else { throw "xdelta3.exe not found: put it beside this script (https://github.com/jmacd/xdelta-gpl/releases)" }
	}
}

# each ROM: the names it may have inside the zips (merged set first, then split / parent)
$files = [ordered]@{
	"prg0b.u22" = @("batriderj/prg0b.u22", "prg0b.u22")
	"prg1b.u23" = @("prg1b.u23")
	"prg2.u21"  = @("prg2.u21")
	"prg3.u24"  = @("prg3.u24")
}
$zips = @("batrider.zip", "batriderj.zip") | ForEach-Object { Join-Path $Roms $_ } | Where-Object { Test-Path $_ }
if (-not $zips) { throw "no batrider.zip in $Roms (use -Roms <folder>)" }

$tmp = Join-Path ([IO.Path]::GetTempPath()) ("batrider_chase_" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path (Join-Path $tmp "stock"), (Join-Path $tmp "chase") | Out-Null
try {
	foreach ($name in $files.Keys) {
		$found = $false
		foreach ($z in $zips) {
			$zip = [IO.Compression.ZipFile]::OpenRead($z)
			try {
				foreach ($entryName in $files[$name]) {
					$e = $zip.Entries | Where-Object { $_.FullName -eq $entryName } | Select-Object -First 1
					if ($e) {
						[IO.Compression.ZipFileExtensions]::ExtractToFile($e, (Join-Path $tmp "stock\$name"), $true)
						$found = $true
						break
					}
				}
			} finally { $zip.Dispose() }
			if ($found) { break }
		}
		if (-not $found) { throw "$name not found in $($zips -join ', ')" }
		$patch = Join-Path $PSScriptRoot "$name.xdelta"
		& $Xdelta -d -f -s (Join-Path $tmp "stock\$name") $patch (Join-Path $tmp "chase\$name")
		if ($LASTEXITCODE -ne 0) { throw "xdelta3 failed on $name (is it the stock MAME ROM?)" }
		Write-Host "patched $name"
	}
	$out = Join-Path $PSScriptRoot "out"
	New-Item -ItemType Directory -Force -Path $out | Out-Null
	$dst = Join-Path $out "batriderj.zip"
	if (Test-Path $dst) { Remove-Item $dst }
	Compress-Archive -Path (Join-Path $tmp "chase\*") -DestinationPath $dst
	Write-Host "written $dst"
} finally {
	Remove-Item -Recurse -Force $tmp
}
