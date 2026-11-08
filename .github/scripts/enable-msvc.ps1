$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installation) {
    throw 'An x64 MSVC installation is required.'
}
$developerCommand = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$developerEnvironment = & $env:ComSpec /d /c `
    "call `"$developerCommand`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw 'MSVC developer environment initialization failed.'
}

# Import only compiler/SDK variables. Never print or persist a complete
# environment dump, which can include unrelated runner credentials.
$allowed = @(
    'PATH', 'INCLUDE', 'LIB', 'LIBPATH',
    'VCToolsInstallDir', 'VCToolsRedistDir', 'VCINSTALLDIR', 'VSINSTALLDIR',
    'VisualStudioVersion', 'WindowsSdkDir', 'WindowsSDKVersion',
    'UniversalCRTSdkDir', 'UCRTVersion', 'ExtensionSdkDir'
)
$imported = @{}
foreach ($line in $developerEnvironment) {
    if ($line -match '^([^=]+)=(.*)$' -and $allowed -contains $Matches[1]) {
        $name = $Matches[1]
        $value = $Matches[2]
        [Environment]::SetEnvironmentVariable($name, $value, 'Process')
        Add-Content -Path $env:GITHUB_ENV -Value "$name=$value" -Encoding utf8
        $imported[$name] = $true
    }
}
foreach ($required in @('PATH', 'INCLUDE', 'LIB')) {
    if (-not $imported.ContainsKey($required)) {
        throw 'MSVC developer environment is incomplete.'
    }
}
Write-Host 'MSVC x64 developer environment enabled.'
