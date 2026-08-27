param(
    [ValidateSet('dev', 'production')][string]$Configuration = 'dev',
    [string]$IdfProfile = 'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
. $IdfProfile
$env:IDF_COMPONENT_CACHE_PATH = Join-Path $ProjectRoot '.cache\idf-component'
$env:IDF_CCACHE_ENABLE = '0'
$IdfRoot = $env:IDF_PATH.Replace('\', '/')
$env:GIT_CONFIG_COUNT = '3'
$env:GIT_CONFIG_KEY_0 = 'safe.directory'
$env:GIT_CONFIG_VALUE_0 = $IdfRoot
$env:GIT_CONFIG_KEY_1 = 'safe.directory'
$env:GIT_CONFIG_VALUE_1 = "$IdfRoot/components/openthread/openthread"
$env:GIT_CONFIG_KEY_2 = 'safe.directory'
$env:GIT_CONFIG_VALUE_2 = $ProjectRoot.Replace('\', '/')
Push-Location $ProjectRoot
try {
    if ($Configuration -eq 'production') {
        $Key = Join-Path $ProjectRoot 'keys\production_signing_key.pem'
        if (-not (Test-Path -LiteralPath $Key)) {
            throw "Production signing key is missing: $Key. Read docs/production-provisioning.md."
        }
        idf.py -B build-product -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.production.defaults' set-target esp32s3
        if ($LASTEXITCODE -ne 0) { throw 'Production configure failed.' }
        idf.py -B build-product build
    } else {
        idf.py set-target esp32s3
        if ($LASTEXITCODE -ne 0) { throw 'Development configure failed.' }
        idf.py build
    }
    if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed.' }
} finally {
    Pop-Location
}
