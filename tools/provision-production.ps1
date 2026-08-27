param(
    [string]$Port = 'COM3',
    [switch]$Execute,
    [string]$IdfProfile = 'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildRoot = Join-Path $ProjectRoot 'build-product'
$OutputRoot = Join-Path $ProjectRoot 'provisioning-output'
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
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
Push-Location $ProjectRoot
try {
    $Key = Join-Path $ProjectRoot 'keys\production_signing_key.pem'
    if (-not (Test-Path -LiteralPath $Key)) {
        throw "Signing key is missing: $Key"
    }
    idf.py -B build-product -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.production.defaults' set-target esp32s3
    if ($LASTEXITCODE -ne 0) { throw 'Production configure failed.' }
    idf.py -B build-product build
    if ($LASTEXITCODE -ne 0) { throw 'Production build failed.' }

    $ChipText = (& esptool.py --chip esp32s3 --port $Port chip-id 2>&1 | Out-String)
    $MacMatch = [regex]::Match($ChipText, '(?i)MAC:\s*([0-9a-f:]{17})')
    if (-not $MacMatch.Success) { throw "Cannot read the device MAC on $Port." }
    $Mac = $MacMatch.Groups[1].Value.ToUpperInvariant()
    $SummaryPath = Join-Path $OutputRoot ("efuse-before-" + ($Mac -replace ':','') + '.txt')
    (& espefuse.py --chip esp32s3 --port $Port summary 2>&1 | Out-String) | Set-Content -LiteralPath $SummaryPath
    Write-Host "Device: $Mac"
    Write-Host "eFuse audit: $SummaryPath"
    Write-Warning 'The production image enables Secure Boot v2, Flash Encryption Release, secure ROM download mode, and JTAG lockdown on first boot.'
    if (-not $Execute) {
        Write-Host 'AUDIT/BUILD ONLY. No flash or eFuse was changed. Re-run with -Execute after reviewing the eFuse audit and documentation.'
        return
    }

    $First = Read-Host "Type exactly: STAGE PRODUCTION $Mac"
    if ($First -cne "STAGE PRODUCTION $Mac") { throw 'Confirmation did not match.' }
    $Flash = Get-Content -Raw -LiteralPath (Join-Path $BuildRoot 'flasher_args.json') | ConvertFrom-Json
    $Arguments = @('--chip','esp32s3','--port',$Port,'--before','default-reset',
        '--after','no-reset','--no-stub','write-flash')
    $Arguments += @($Flash.write_flash_args)
    $Arguments += @('0x0', 'bootloader/bootloader.bin')
    foreach ($Entry in $Flash.flash_files.PSObject.Properties) {
        $Arguments += @($Entry.Name, $Entry.Value)
    }
    Push-Location $BuildRoot
    try { & esptool.py @Arguments } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw 'Staging failed; the chip was not deliberately reset.' }

    Write-Warning 'Images are staged with no reset. The next reset starts irreversible eFuse provisioning.'
    $Second = Read-Host "Type exactly: ENABLE SECURITY $Mac"
    if ($Second -cne "ENABLE SECURITY $Mac") {
        Write-Warning 'Security was not enabled. Keep the board powered and in download mode until you decide how to proceed.'
        return
    }
    & esptool.py --chip esp32s3 --port $Port run
    Write-Host 'First secure boot started. Do not remove power for at least 60 seconds.'
    Write-Host 'Validate USB HID, NCM, OTA, and the eFuse state using the production checklist.'
} finally {
    Pop-Location
}
