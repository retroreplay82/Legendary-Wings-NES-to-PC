param([ValidatePattern('^[a-zA-Z0-9_-]+\.exe$')][string]$ExeName='LegendaryWings.exe')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
$zig = Join-Path $PSScriptRoot 'vendor/zig-x86_64-windows-0.15.2/zig.exe'
$framework = Join-Path $PSScriptRoot 'vendor/nesrecomp'
New-Item -ItemType Directory -Force build | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PSScriptRoot 'build/zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $PSScriptRoot 'build/zig-local'
$recompilerFiles = @('main_nes','rom_parser','cpu6502_decoder','function_finder','code_generator','cyc_codegen','function_dedup','annotations','symbol_table','game_config','coverage','toml') | ForEach-Object { "$framework/recompiler/src/$_.c" }
# Fold instruction bytes only. Other ROM reads stay runtime reads so graphics
# and tilemap data cannot become constants in the distributed executable.
$codegenPath=Join-Path $PSScriptRoot 'build/cyc_codegen_public.c'
$codegen=Get-Content "$framework/recompiler/src/cyc_codegen.c" -Raw
$functionStart=$codegen.IndexOf('static bool known_byte(')
$functionEnd=$codegen.IndexOf('static const char *read_const(', $functionStart)
if($functionStart -lt 0 -or $functionEnd -lt 0){throw 'ROM folding hook changed'}
$body=$codegen.Substring($functionStart,$functionEnd-$functionStart)
$match='    Pos to;'
if(!$body.Contains($match)){throw 'ROM folding guard changed'}
$body=$body.Replace($match, "    uint16_t own_byte = (uint16_t)(addr - e->P);`n    if (own_byte >= (uint16_t)op_length(e->opcode)) return false;`n    Pos to;")
$codegen=$codegen.Substring(0,$functionStart)+$body+$codegen.Substring($functionEnd)
Set-Content -LiteralPath $codegenPath -Value $codegen -NoNewline
$recompilerFiles=$recompilerFiles | ForEach-Object {if($_ -eq "$framework/recompiler/src/cyc_codegen.c"){$codegenPath}else{$_}}
$argsRecompiler = @('cc','-O2','-std=c11',"-I$framework/recompiler/src",'-o',"$PSScriptRoot/build/NESRecomp.exe") + $recompilerFiles
& ./run-hidden.ps1 -Executable $zig -Arguments $argsRecompiler
& ./run-hidden.ps1 -Executable ./build/NESRecomp.exe -Arguments @("$PSScriptRoot/roms/Legendary Wings (USA).nes",'--game',"$PSScriptRoot/game.toml",'--cycle-accurate','--output-prefix','wings')
$cmake = Get-Content "$framework/runner/cyc/cyc.cmake" -Raw
$sourceBlock = [regex]::Match($cmake, '(?s)set\(NESRECOMP_CYC_SOURCES\s+(.*?)\n\)').Groups[1].Value
$sources = [regex]::Matches($sourceBlock, '\$\{NESRECOMP_CYC_DIR\}/([^\s]+\.c)') | ForEach-Object { "$framework/runner/cyc/" + $_.Groups[1].Value }
$sdl = "$framework/runner/external/SDL2"
$sdlSource=Get-Content -LiteralPath "$framework/runner/cyc/cyc_sdl.c" -Raw
$integerScale='SDL_RenderSetIntegerScale(s_ren, s_set.integer_scale ? SDL_TRUE : SDL_FALSE);'
if(!$sdlSource.Contains($integerScale)){throw 'SDL scaling hook changed; inspect before building'}
# Keep this game-specific policy reproducible without editing the vendor.
$sdlSource=$sdlSource.Replace($integerScale,'SDL_RenderSetIntegerScale(s_ren, SDL_FALSE);')
Set-Content -LiteralPath "$PSScriptRoot/build/cyc_sdl_wings.c" -Value $sdlSource -NoNewline
$argsGame = @('cc','-O2','-std=c11','-DCYC_WITH_SDL','-DSDL_MAIN_HANDLED','-include',"$framework/runner/cyc/cyc_trace.h","-I$framework/runner/cyc","-I$sdl/include",'-o',"$PSScriptRoot/build/$ExeName") + $sources + @("$PSScriptRoot/generated/wings_cyc.c","$framework/runner/cyc/cyc_sdl.c","$framework/runner/cyc/cyc_input.c","$framework/runner/cyc/cyc_settings.c","$framework/runner/cyc/cyc_tcp.c","$sdl/lib/x64/SDL2.lib",'-lws2_32')
$argsGame += @(Get-ChildItem "$PSScriptRoot/generated/wings_cyc_b*.c" | ForEach-Object FullName)
$argsGame=$argsGame | ForEach-Object {if($_ -eq "$framework/runner/cyc/cyc_sdl.c"){"$PSScriptRoot/build/cyc_sdl_wings.c"}else{$_}}
$argsGame += @("$PSScriptRoot/enhanced-presentation.c", "$PSScriptRoot/hero-animation.c", "$PSScriptRoot/ground-animation.c", "$PSScriptRoot/wyvern-animation.c", "$PSScriptRoot/pod-animation.c", "$PSScriptRoot/artwork-priority.c", "$PSScriptRoot/guardian-animation.c", "$PSScriptRoot/green-animation.c", "$PSScriptRoot/cycling-animation.c", "$PSScriptRoot/flash-animation.c", "$PSScriptRoot/swirl-animation.c", "$PSScriptRoot/halo-animation.c", "$PSScriptRoot/chamber-animation.c", "$PSScriptRoot/turret-animation.c", "$PSScriptRoot/missile-animation.c", "$PSScriptRoot/serpent-animation.c", "$PSScriptRoot/burst-animation.c", "$PSScriptRoot/desktop-entry.c", "$PSScriptRoot/respawn-collision.c", '-Dmain=wings_engine_main', '-luser32', '-Wl,--subsystem,windows', "-I$framework/runner/src")
& ./run-hidden.ps1 -Executable $zig -Arguments $argsGame
$dllSource="$sdl/lib/x64/SDL2.dll"
$dllTarget="$PSScriptRoot/build/SDL2.dll"
if(!(Test-Path -LiteralPath $dllTarget) -or
   (Get-FileHash -LiteralPath $dllSource).Hash -ne (Get-FileHash -LiteralPath $dllTarget).Hash) {
    Copy-Item -LiteralPath $dllSource -Destination $dllTarget
}
New-Item -ItemType Directory -Force build/assets | Out-Null
$assetNames=(Get-Content -LiteralPath assets-manifest.json -Raw | ConvertFrom-Json).assets
foreach($assetName in $assetNames){Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('assets/'+$assetName)) -Destination (Join-Path $PSScriptRoot ('build/assets/'+$assetName))}
} finally { Pop-Location }
