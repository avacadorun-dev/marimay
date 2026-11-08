param([Parameter(Mandatory = $true)][string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

$header = Get-Content -Raw -Path (Join-Path $BuildDirectory 'generated/mmay/build_config.h')
if ($header -notmatch '#define MMAY_NATIVE_C_COMPILER "([^"]+)"') {
    throw 'Configured native compiler path is missing.'
}
$compiler = $Matches[1]
if ($header -notmatch '#define MMAY_NATIVE_C_COMPILER_VERSION "([^"]+)"') {
    throw 'Configured native compiler version is missing.'
}
$expected = $Matches[1]
$identity = (& $compiler '/?' 2>&1 | Out-String)
$queryExit = $LASTEXITCODE
$queryBytes = [Text.Encoding]::UTF8.GetByteCount($identity)
$versions = [regex]::Matches($identity, '(?<![0-9.])[0-9]+\.[0-9]+\.[0-9]+(?:\.[0-9]+)?(?![0-9.])') |
    ForEach-Object { $_.Value } | Select-Object -Unique -First 8
# Report bounded identity facts, never compiler installation paths or the
# complete output/environment. CTest still owns the real identity gate.
Write-Host "MSVC identity query: exit=$queryExit; bytes=$queryBytes; expected=$expected; versions=$($versions -join ',')"
