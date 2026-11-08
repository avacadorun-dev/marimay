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

# Match the raw Windows process owner's quoted argument and windowless mode.
# Read both byte streams without PowerShell's native-output decoding.
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $compiler
$start.Arguments = '"/?"'
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$process = [Diagnostics.Process]::new()
$process.StartInfo = $start
$rawOutput = [IO.MemoryStream]::new()
$rawError = [IO.MemoryStream]::new()
try {
    if (-not $process.Start()) { throw 'Raw MSVC identity query did not start.' }
    $outputCopy = $process.StandardOutput.BaseStream.CopyToAsync($rawOutput)
    $errorCopy = $process.StandardError.BaseStream.CopyToAsync($rawError)
    $finished = $process.WaitForExit(15000)
    if (-not $finished) {
        $process.Kill($true)
        $process.WaitForExit()
    }
    [Threading.Tasks.Task]::WaitAll(@($outputCopy, $errorCopy))
    $outputBytes = $rawOutput.ToArray()
    $errorBytes = $rawError.ToArray()
    $outputHead = [BitConverter]::ToString($outputBytes, 0, [Math]::Min(16, $outputBytes.Length)).Replace('-', '')
    $errorHead = [BitConverter]::ToString($errorBytes, 0, [Math]::Min(16, $errorBytes.Length)).Replace('-', '')
    Write-Host "MSVC quoted raw query: finished=$finished; exit=$($process.ExitCode); stdoutBytes=$($outputBytes.Length); stdoutHead=$outputHead; stderrBytes=$($errorBytes.Length); stderrHead=$errorHead"
} finally {
    $process.Dispose()
    $rawOutput.Dispose()
    $rawError.Dispose()
}
