param([string]$AssetArchive='')
$ErrorActionPreference='Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Force vendor,build,assets,roms | Out-Null
    $frameworkCommit='3ef948cd0a3aee6df7c665587540c4b18006cae6'
    if(!(Test-Path vendor/nesrecomp/recompiler/src/main_nes.c)) {
        if(Test-Path .git) {
            & git submodule update --init vendor/nesrecomp
        } else {
            & git clone https://github.com/mstan/nesrecomp.git vendor/nesrecomp
            if($LASTEXITCODE){throw 'NESRecomp download failed'}
            & git -C vendor/nesrecomp checkout --detach $frameworkCommit
        }
        if($LASTEXITCODE){throw 'NESRecomp setup failed'}
    }
    $actualCommit=(& git -C vendor/nesrecomp rev-parse HEAD).Trim()
    if($actualCommit -ne $frameworkCommit){throw 'NESRecomp checkout does not match the pinned revision'}
    if(!(Test-Path vendor/zig-x86_64-windows-0.15.2/zig.exe)) {
        $zigArchive=Join-Path $PSScriptRoot 'build/zig-0.15.2.zip'
        Invoke-WebRequest https://ziglang.org/download/0.15.2/zig-x86_64-windows-0.15.2.zip -OutFile $zigArchive
        Expand-Archive -LiteralPath $zigArchive -DestinationPath vendor -Force
    }
    if(!$AssetArchive) {
        $AssetArchive=Join-Path $PSScriptRoot 'build/beta1-assets.zip'
        Invoke-WebRequest https://github.com/retroreplay82/Legendary-Wings-NES-to-PC/releases/download/v0.1.0-beta.1/LegendaryWings-windows-x64-beta.1.zip -OutFile $AssetArchive
    }
    $expected='4dc2ea45c4bd0a5512136a8d37bdef02e8165b68756e0b08916a4eacc8381ff0'
    if((Get-FileHash -LiteralPath $AssetArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected){throw 'Release asset checksum mismatch'}
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip=[IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $AssetArchive).Path)
    try {
        $names=(Get-Content assets-manifest.json -Raw | ConvertFrom-Json).assets
        foreach($name in $names) {
            if($name -notmatch '^[a-zA-Z0-9_-]+\.png$'){throw 'Invalid asset filename'}
            $entry=$zip.GetEntry('assets/'+$name)
            if(!$entry){throw "Missing release artwork: $name"}
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,(Join-Path $PSScriptRoot ('assets/'+$name)),$true)
        }
    } finally {$zip.Dispose()}
    Write-Host 'Dependencies and release artwork ready. Supply your own ROM, then run ./build.ps1.'
} finally {Pop-Location}
