param([string]$GamePath, [switch]$NoPrompt)
$ErrorActionPreference = 'Stop'
try {
$encoding = [Text.Encoding]::GetEncoding(28591)
if (-not $GamePath) {
    $GamePath = Join-Path $PSScriptRoot 'JK'
    if (-not (Test-Path (Join-Path $GamePath 'Episode/JK1MP.GOB'))) {
        if ($NoPrompt) { throw 'Specify -GamePath with your own Jedi Knight installation folder.' }
        $GamePath = (Read-Host 'Folder containing your Jedi Knight Episode and Resource folders').Trim('"')
    }
}
$GamePath = (Resolve-Path -LiteralPath $GamePath).Path
if ($GamePath.Contains("`r") -or $GamePath.Contains("`n") -or $GamePath.Contains('%')) { throw 'Use a game folder without percent signs or line breaks.' }
$episode = Join-Path $GamePath 'Episode'
$resource = Join-Path $GamePath 'Resource'
if (-not (Test-Path -LiteralPath $episode) -or -not (Test-Path -LiteralPath $resource)) { throw 'The selected folder must contain Episode and Resource.' }
$multiplayer = Get-ChildItem -LiteralPath $episode -File | Where-Object { $_.Name -ieq 'JK1MP.GOB' } | Select-Object -First 1
if (-not $multiplayer) { throw 'JK1MP.GOB was not found in Episode. Install Jedi Knight first.' }

# Check before reading the archives: the editor also needs to save levels here.
function Assert-WritableFolder([string]$Folder, [string[]]$Files) {
    $probe = Join-Path $Folder ('.geosith-write-test-' + [guid]::NewGuid().ToString('N'))
    try {
        [void][IO.Directory]::CreateDirectory($Folder)
        $stream = [IO.File]::Open($probe,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
        $stream.Dispose()
        foreach ($name in $Files) {
            $target = Join-Path $Folder $name
            if (Test-Path -LiteralPath $target) {
                $stream = [IO.File]::Open($target,[IO.FileMode]::Open,[IO.FileAccess]::Write)
                $stream.Dispose()
            }
        }
    } catch {
        throw "GeoSith cannot write to '$Folder'. Move the entire GeoSith folder, including JK, to a writable location such as Documents\GeoSith, then run Setup GeoSith.cmd again. If using a separate game installation, copy it to a writable folder and select that copy. Also clear Read-only on existing editor catalogs. Program Files is usually protected; administrator access is not required when using a writable folder."
    } finally {
        if (Test-Path -LiteralPath $probe) { Remove-Item -LiteralPath $probe }
    }
}
Assert-WritableFolder $PSScriptRoot @('game-path.txt')
Assert-WritableFolder $GamePath @('editor-resources.jkl','editor-materials.tsv')
Assert-WritableFolder $episode @()
Assert-WritableFolder $resource @()
Assert-WritableFolder (Join-Path $GamePath 'misc') @('sithed_templates.tpl')

function Read-GobIndex([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = New-Object IO.BinaryReader($stream)
    try {
        if ([Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)) -ne "GOB " -or $reader.ReadUInt32() -ne 20) { throw "Invalid GOB: $Path" }
        $table = $reader.ReadUInt32()
        if ($table -gt $stream.Length - 4) { throw "Invalid GOB directory: $Path" }
        $stream.Position = $table
        $count = $reader.ReadUInt32()
        if ($count -gt 100000 -or ($table + 4L + 136L * $count) -gt $stream.Length) { throw "Invalid GOB entry count: $Path" }
        for ($i = 0; $i -lt $count; $i++) {
            $offset = $reader.ReadUInt32(); $size = $reader.ReadUInt32()
            $name = [Text.Encoding]::ASCII.GetString($reader.ReadBytes(128)).TrimEnd([char]0).ToLowerInvariant()
            if (([long]$offset + $size) -gt $stream.Length) { throw "Invalid GOB entry: $name" }
            [pscustomobject]@{ Archive=$Path; Name=$name; Offset=$offset; Size=$size }
        }
    } finally { $reader.Dispose(); $stream.Dispose() }
}
function Read-GobText($Entry) {
    if ($Entry.Size -gt 32MB) { throw "Unexpectedly large level: $($Entry.Name)" }
    $stream = [IO.File]::OpenRead($Entry.Archive)
    try {
        $stream.Position = $Entry.Offset
        $buffer = New-Object byte[] $Entry.Size
        $position = 0
        while ($position -lt $buffer.Length) { $n=$stream.Read($buffer,$position,$buffer.Length-$position); if (-not $n) { throw 'Unexpected end of GOB.' }; $position += $n }
        $encoding.GetString($buffer).Replace("`r",'')
    } finally { $stream.Dispose() }
}

Write-Host 'Preparing GeoSith catalogs from your own game files...'
$stockNames = @('jk1mp.gob','jk1.gob','res1.gob','res1hi.gob','res2.gob')
$archives = @($multiplayer) + @(Get-ChildItem -LiteralPath $episode,$resource -File | Where-Object { $_.Name.ToLowerInvariant() -in $stockNames -and $_.FullName -ne $multiplayer.FullName })
$templates = [ordered]@{}; $materials = @{}; $usage = @{}; $baseline = $null
foreach ($archive in $archives) {
    foreach ($entry in (Read-GobIndex $archive.FullName)) {
        $name = $entry.Name
        if ($name.EndsWith('.mat')) { $base=$name.Split('\')[-1]; $kind='level'; if ($name.StartsWith('3do\')) { $kind='models' }; $materials[$base]=$kind }
        if (-not $name.EndsWith('.jkl')) { continue }
        $text = Read-GobText $entry
        if (-not $baseline -and $archive.FullName -eq $multiplayer.FullName) { $baseline=$text }
        $block = [regex]::Match($text,'(?ims)^World templates[^\n]*\n(.*?)^end\s*$')
        if ($block.Success) { foreach ($line in $block.Groups[1].Value.Split("`n")) {
            $definition=$line.Trim(); if ($definition -and -not $definition.StartsWith('#')) { $key=($definition -split '\s+')[0]; if (-not $templates.Contains($key)) { $templates[$key]=$definition } }
        } }
        $cmp=[regex]::Match($text,'(?im)^World colormaps[^\n]*\n\s*0:\s*(\S+)')
        $mats=[regex]::Match($text,'(?ims)^World materials[^\n]*\n(.*?)^end\s*$')
        $faces=[regex]::Match($text,'(?ims)^World surfaces[^\n]*\n(.*?)^end\s*$')
        if (-not ($cmp.Success -and $mats.Success -and $faces.Success)) { continue }
        $ids=@{}; foreach ($match in [regex]::Matches($mats.Groups[1].Value,'(?m)^\s*(\d+):\s*(\S+)')) { $ids[$match.Groups[1].Value]=$match.Groups[2].Value.ToLowerInvariant() }
        foreach ($match in [regex]::Matches($faces.Groups[1].Value,'(?m)^\s*\d+:\s*(-?\d+)\s+0x')) {
            $id=$match.Groups[1].Value; if (-not $ids.ContainsKey($id)) { continue }; $material=$ids[$id]; $color=$cmp.Groups[1].Value.ToLowerInvariant()
            if (-not $usage.ContainsKey($material)) { $usage[$material]=@{} }; if (-not $usage[$material].ContainsKey($color)) { $usage[$material][$color]=0 }; $usage[$material][$color]++
        }
    }
}
if (-not $baseline -or -not $templates.Count) { throw 'Could not find game level resources and templates.' }
$output = New-Object Text.StringBuilder
[void]$output.AppendLine('# GeoSith resources, generated locally from your Jedi Knight installation.')
$sections=[regex]::Split($baseline,'(?im)^SECTION:\s*(\w+)\s*$')
for ($i=1; $i -lt $sections.Length; $i+=2) {
    $name=$sections[$i]; if ($name.ToLowerInvariant() -in @('georesource','sectors','things','cogs')) { continue }
    [void]$output.AppendLine("`nSECTION: $name")
    if ($name -ieq 'templates') { [void]$output.AppendLine("World templates 2048`n$($templates.Values -join "`n")`nend") }
    else { [void]$output.AppendLine($sections[$i+1]) }
}
$manifest=$output.ToString()
$capacities=@{sounds=4096;materials=4096;models=1024;sprites=256;keyframes=4096;puppets=256;soundclasses=256;aiclasses=256;scripts=512}
foreach ($key in $capacities.Keys) { $manifest=[regex]::Replace($manifest,"(?i)World $key\s+\d+",("World $key " + $capacities[$key])) }
$manifest=[regex]::Replace($manifest,'(?im)^SECTION: TEMPLATES',"SECTION: COGS`nWorld cogs 128`nend`n`nSECTION: TEMPLATES")
[IO.File]::WriteAllText((Join-Path $GamePath 'editor-resources.jkl'),$manifest,$encoding)
$catalog = New-Object Text.StringBuilder
[void]$catalog.AppendLine('# material kind colormap; generated from installed levels')
foreach ($name in ($materials.Keys | Sort-Object)) {
    $color='dflt.cmp'; if ($usage.ContainsKey($name)) { $color=($usage[$name].GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1).Key }
    [void]$catalog.AppendLine("$name`t$($materials[$name])`t$color")
}
[IO.File]::WriteAllText((Join-Path $GamePath 'editor-materials.tsv'),$catalog.ToString(),$encoding)
$misc=Join-Path $GamePath 'misc'; [void][IO.Directory]::CreateDirectory($misc)
[IO.File]::WriteAllText((Join-Path $misc 'sithed_templates.tpl'),($templates.Values -join "`r`n"),$encoding)
$marker=Join-Path $resource 'jk_.cd'
if (-not (Test-Path -LiteralPath $marker)) { $original=Join-Path $GamePath 'JK_.CD'; if (-not (Test-Path -LiteralPath $original)) { throw 'Your installation is missing JK_.CD.' }; Copy-Item -LiteralPath $original -Destination $marker }
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'game-path.txt'),$GamePath,[Text.Encoding]::Default)
Write-Host "Ready: $($templates.Count) templates and $($materials.Count) materials. Run Launch GeoSith.cmd."
} catch {
    Write-Host "Setup failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
