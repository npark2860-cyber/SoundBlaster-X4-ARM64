[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Preflight', 'Install', 'Verify', 'Rollback')]
    [string]$Action
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$PackageRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ApoInf = Join-Path $PackageRoot 'X4ApoArm64.inf'
$ExtensionInf = Join-Path $PackageRoot 'X4ApoSpeakerExtension.inf'
$ApoCat = Join-Path $PackageRoot 'X4ApoArm64.cat'
$ExtensionCat = Join-Path $PackageRoot 'X4ApoSpeakerExtension.cat'
$ApoDll = Join-Path $PackageRoot 'X4ApoArm64.dll'
$CertFile = Join-Path $PackageRoot 'X4ApoStageA2Test.cer'
$StateFile = Join-Path $PackageRoot 'X4_STAGE_A2_INSTALL_STATE.json'
$VerifyReport = Join-Path $PackageRoot 'X4_STAGE_A2_VERIFY_REPORT.json'

$TargetHardwarePrefix = 'USB\VID_041E&PID_3278&MI_03'
$ExpectedService = 'usbaudio2'
$ExpectedApoFriendlyName = 'Sound Blaster X4 ARM64 Stage A2 Test APO'
$ExpectedCertSubject = 'CN=SoundBlaster-X4-ARM64 Stage A2 Test'

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Run PowerShell as Administrator.'
    }
}

function Assert-Arm64Host {
    $arch = [Environment]::GetEnvironmentVariable('PROCESSOR_ARCHITECTURE', 'Machine')
    if ($arch -ne 'ARM64') {
        throw "Stage A2 is ARM64-only. Native machine architecture reported: $arch"
    }
}

function Get-X4AudioDevice {
    $devices = @(Get-PnpDevice -PresentOnly -ErrorAction Stop | Where-Object {
        $_.InstanceId -like "$TargetHardwarePrefix*"
    })
    if ($devices.Count -ne 1) {
        throw "Expected exactly one present X4 MI_03 audio device; found $($devices.Count)."
    }
    return $devices[0]
}

function Get-DeviceService([string]$InstanceId) {
    $property = Get-PnpDeviceProperty -InstanceId $InstanceId -KeyName 'DEVPKEY_Device_Service' -ErrorAction Stop
    return [string]$property.Data
}

function Get-TestSigningEnabled {
    $text = (& bcdedit.exe /enum '{current}' 2>&1 | Out-String)
    $line = ($text -split "`r?`n" | Where-Object { $_ -match '(?i)testsigning' } | Select-Object -First 1)
    if (-not $line) {
        return $false
    }
    if ($line -match '(?i)\b(no|off|false|0)\b|아니오|끔|해제') {
        return $false
    }
    return $true
}

function Get-SecureBootEnabled {
    try {
        return [bool](Confirm-SecureBootUEFI -ErrorAction Stop)
    }
    catch {
        throw "Secure Boot state could not be read: $($_.Exception.Message)"
    }
}

function Invoke-PnpUtilChecked([string[]]$Arguments) {
    $output = (& pnputil.exe @Arguments 2>&1 | Out-String)
    $exitCode = $LASTEXITCODE
    Write-Host $output
    if ($exitCode -notin @(0, 3010)) {
        throw "pnputil failed with exit code $exitCode. Arguments: $($Arguments -join ' ')"
    }
    return $output
}

function Get-OemInfByOriginalName([string]$OriginalName) {
    $text = (& pnputil.exe /enum-drivers /files 2>&1 | Out-String)
    $blocks = [regex]::Split($text, '(?:\r?\n){2,}')
    foreach ($block in $blocks) {
        if ($block -match [regex]::Escape($OriginalName)) {
            $match = [regex]::Match($block, '(?im)\boem\d+\.inf\b')
            if ($match.Success) {
                return $match.Value.ToLowerInvariant()
            }
        }
    }
    return $null
}

function Read-State {
    if (-not (Test-Path $StateFile)) {
        return $null
    }
    return (Get-Content -Raw -Path $StateFile | ConvertFrom-Json)
}

function Save-State($State) {
    $State | ConvertTo-Json -Depth 5 | Set-Content -Path $StateFile -Encoding UTF8
}

function Assert-PackageFiles {
    foreach ($file in @($ApoInf, $ExtensionInf, $ApoCat, $ExtensionCat, $ApoDll, $CertFile)) {
        if (-not (Test-Path $file)) {
            throw "Required Stage A2 file is missing: $file"
        }
    }

    $bytes = [System.IO.File]::ReadAllBytes($ApoDll)
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
    if ($machine -ne 0xAA64) {
        throw ('X4ApoArm64.dll is not ARM64 PE machine 0xAA64; got 0x{0:X4}.' -f $machine)
    }

    $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($CertFile)
    if ($cert.Subject -ne $ExpectedCertSubject) {
        throw "Unexpected test certificate subject: $($cert.Subject)"
    }

    foreach ($signedFile in @($ApoDll, $ApoCat, $ExtensionCat)) {
        $signature = Get-AuthenticodeSignature -FilePath $signedFile
        if (-not $signature.SignerCertificate) {
            throw "No Authenticode signer found: $signedFile"
        }
        if ($signature.SignerCertificate.Thumbprint -ne $cert.Thumbprint) {
            throw "Signer thumbprint mismatch: $signedFile"
        }
    }

    return $cert
}

function Import-TestCertificate($Cert) {
    Import-Certificate -FilePath $CertFile -CertStoreLocation 'Cert:\LocalMachine\Root' | Out-Null
    Import-Certificate -FilePath $CertFile -CertStoreLocation 'Cert:\LocalMachine\TrustedPublisher' | Out-Null

    foreach ($signedFile in @($ApoDll, $ApoCat, $ExtensionCat)) {
        $signature = Get-AuthenticodeSignature -FilePath $signedFile
        if ($signature.Status -ne 'Valid') {
            throw "Signature is not trusted after certificate import: $signedFile ($($signature.Status))"
        }
    }
}

function Remove-TestCertificate([string]$Thumbprint) {
    if (-not $Thumbprint) {
        return
    }
    foreach ($store in @('Cert:\LocalMachine\TrustedPublisher', 'Cert:\LocalMachine\Root')) {
        $path = Join-Path $store $Thumbprint
        if (Test-Path $path) {
            $cert = Get-Item $path
            if ($cert.Subject -eq $ExpectedCertSubject) {
                Remove-Item $path -Force
            }
        }
    }
}

function Get-ApoComponentPresent {
    $devices = @(Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object {
        $_.FriendlyName -eq $ExpectedApoFriendlyName
    })
    return ($devices.Count -gt 0)
}

function Get-SpeakerFxBindingPresent {
    $root = 'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceClasses\{6994AD04-93EF-11D0-A3CC-00A0C9223196}'
    if (-not (Test-Path $root)) {
        return $false
    }

    $associationName = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},0'
    $streamName = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},5'
    $modeName = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},6'
    $endpointName = '{D04E05A6-594B-4FB6-A80D-01AF5EED7D1D},7'

    $speaker = '{DFF21CE1-F70F-11D0-B917-00A0C9223196}'
    $sfx = '{71DAB6A1-39F3-423E-90A8-032729851157}'
    $mfx = '{C624D7B2-8333-448E-85C8-51EEFC2025ED}'
    $efx = '{EC2F4B76-6AE1-4DB9-8FF6-344B74CF9650}'

    $interfaceRoots = @(Get-ChildItem -Path $root -Recurse -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -match '(?i)VID_041E&PID_3278&MI_03' -and $_.Name -match '(?i)msft_topo'
    })

    foreach ($interfaceRoot in $interfaceRoots) {
        $keys = @($interfaceRoot) + @(Get-ChildItem -Path $interfaceRoot.PSPath -Recurse -ErrorAction SilentlyContinue)
        foreach ($key in $keys) {
            try {
                $props = Get-ItemProperty -Path $key.PSPath -ErrorAction Stop
                $names = $props.PSObject.Properties.Name
                if ($names -contains $associationName -and
                    $names -contains $streamName -and
                    $names -contains $modeName -and
                    $names -contains $endpointName) {
                    if ([string]$props.$associationName -eq $speaker -and
                        [string]$props.$streamName -eq $sfx -and
                        [string]$props.$modeName -eq $mfx -and
                        [string]$props.$endpointName -eq $efx) {
                        return $true
                    }
                }
            }
            catch {
            }
        }
    }
    return $false
}

function Get-AudioDgModuleState {
    $processes = @(Get-Process -Name audiodg -ErrorAction SilentlyContinue)
    if ($processes.Count -eq 0) {
        return $false
    }

    $couldInspectAny = $false
    foreach ($process in $processes) {
        try {
            $modules = @($process.Modules)
            $couldInspectAny = $true
            if ($modules | Where-Object { $_.ModuleName -ieq 'X4ApoArm64.dll' }) {
                return $true
            }
        }
        catch {
        }
    }

    if (-not $couldInspectAny) {
        return $null
    }
    return $false
}

function Write-VerifyReport {
    $device = Get-X4AudioDevice
    $service = Get-DeviceService $device.InstanceId
    $apoPresent = Get-ApoComponentPresent
    $fxPresent = Get-SpeakerFxBindingPresent
    $audioDgLoaded = Get-AudioDgModuleState

    $report = [ordered]@{
        Timestamp = (Get-Date).ToString('o')
        TargetInstanceId = $device.InstanceId
        BaseService = $service
        BaseServiceExpected = ($service -eq $ExpectedService)
        ApoComponentPresent = $apoPresent
        SpeakerFxBindingPresent = $fxPresent
        AudioDgModuleLoaded = $audioDgLoaded
        ApoProcessInvocation = 'Not directly observable in Stage A2 without changing the validated pass-through DLL.'
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -Path $VerifyReport -Encoding UTF8
    $report | Format-List | Out-Host
    Write-Host "Report: $VerifyReport"
    return $report
}

Assert-Arm64Host

switch ($Action) {
    'Preflight' {
        Assert-Administrator
        $cert = Assert-PackageFiles
        $device = Get-X4AudioDevice
        $service = Get-DeviceService $device.InstanceId
        if ($service -ne $ExpectedService) {
            throw "X4 MI_03 base service is not usbaudio2: $service"
        }

        $secureBoot = Get-SecureBootEnabled
        $testSigning = Get-TestSigningEnabled
        $existingApo = Get-OemInfByOriginalName 'X4ApoArm64.inf'
        $existingExtension = Get-OemInfByOriginalName 'X4ApoSpeakerExtension.inf'

        [ordered]@{
            Result = if (-not $secureBoot -and $testSigning) { 'PASS' } else { 'BLOCKED' }
            X4InstanceId = $device.InstanceId
            BaseService = $service
            SecureBootEnabled = $secureBoot
            TestSigningEnabled = $testSigning
            CertificateThumbprint = $cert.Thumbprint
            ExistingApoPackage = $existingApo
            ExistingExtensionPackage = $existingExtension
        } | Format-List | Out-Host

        if ($secureBoot) {
            throw 'Secure Boot is enabled. This self-signed Stage A2 package must not be installed in the current state.'
        }
        if (-not $testSigning) {
            throw 'Windows TESTSIGNING is not enabled. Enable test signing and reboot before Stage A2 installation.'
        }
        if ($existingApo -or $existingExtension) {
            throw 'A package with the Stage A2 INF original name is already in Driver Store. Roll it back before a fresh Stage A2 install.'
        }
        Write-Host 'PRELIGHT RESULT: PASS'
    }

    'Install' {
        Assert-Administrator
        $cert = Assert-PackageFiles
        $device = Get-X4AudioDevice
        $service = Get-DeviceService $device.InstanceId
        if ($service -ne $ExpectedService) {
            throw "X4 MI_03 base service is not usbaudio2: $service"
        }
        if (Get-SecureBootEnabled) {
            throw 'Secure Boot is enabled. Installation aborted before any certificate or driver-store change.'
        }
        if (-not (Get-TestSigningEnabled)) {
            throw 'Windows TESTSIGNING is not enabled. Installation aborted before any certificate or driver-store change.'
        }
        if (Test-Path $StateFile) {
            throw "Existing Stage A2 state file found: $StateFile. Roll back first."
        }
        if ((Get-OemInfByOriginalName 'X4ApoArm64.inf') -or (Get-OemInfByOriginalName 'X4ApoSpeakerExtension.inf')) {
            throw 'Stage A2 package INF is already present in Driver Store. Roll back before a fresh install.'
        }

        $state = [ordered]@{
            Schema = 1
            InstalledAt = (Get-Date).ToString('o')
            TargetInstanceId = $device.InstanceId
            PreInstallService = $service
            CertificateThumbprint = $cert.Thumbprint
            ApoPublishedInf = $null
            ExtensionPublishedInf = $null
        }
        Save-State $state

        Import-TestCertificate $cert

        Invoke-PnpUtilChecked @('/add-driver', $ApoInf) | Out-Null
        $state.ApoPublishedInf = Get-OemInfByOriginalName 'X4ApoArm64.inf'
        Save-State $state
        if (-not $state.ApoPublishedInf) {
            throw 'APO INF was added but its published oem*.inf name could not be resolved. State file retained for rollback.'
        }

        Invoke-PnpUtilChecked @('/add-driver', $ExtensionInf, '/install') | Out-Null
        $state.ExtensionPublishedInf = Get-OemInfByOriginalName 'X4ApoSpeakerExtension.inf'
        Save-State $state
        if (-not $state.ExtensionPublishedInf) {
            throw 'Extension INF install returned success but its published oem*.inf name could not be resolved. State file retained for rollback.'
        }

        Invoke-PnpUtilChecked @('/scan-devices') | Out-Null

        $postService = Get-DeviceService $device.InstanceId
        if ($postService -ne $ExpectedService) {
            throw "Stage A2 invariant failed: X4 base service changed from usbaudio2 to $postService. Run Rollback immediately."
        }

        Write-Host "Stage A2 install completed. APO INF: $($state.ApoPublishedInf), Extension INF: $($state.ExtensionPublishedInf)"
        Write-VerifyReport | Out-Null
    }

    'Verify' {
        Assert-Administrator
        Assert-PackageFiles | Out-Null
        Write-VerifyReport | Out-Null
    }

    'Rollback' {
        Assert-Administrator
        $state = Read-State

        $extensionPublishedInf = if ($state -and $state.ExtensionPublishedInf) {
            [string]$state.ExtensionPublishedInf
        } else {
            Get-OemInfByOriginalName 'X4ApoSpeakerExtension.inf'
        }
        $apoPublishedInf = if ($state -and $state.ApoPublishedInf) {
            [string]$state.ApoPublishedInf
        } else {
            Get-OemInfByOriginalName 'X4ApoArm64.inf'
        }
        $thumbprint = if ($state -and $state.CertificateThumbprint) {
            [string]$state.CertificateThumbprint
        } elseif (Test-Path $CertFile) {
            (New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($CertFile)).Thumbprint
        } else {
            $null
        }

        if ($extensionPublishedInf) {
            Invoke-PnpUtilChecked @('/delete-driver', $extensionPublishedInf, '/uninstall', '/force') | Out-Null
        }
        Invoke-PnpUtilChecked @('/scan-devices') | Out-Null

        if ($apoPublishedInf) {
            Invoke-PnpUtilChecked @('/delete-driver', $apoPublishedInf, '/uninstall', '/force') | Out-Null
        }
        Invoke-PnpUtilChecked @('/scan-devices') | Out-Null

        $device = Get-X4AudioDevice
        try {
            Invoke-PnpUtilChecked @('/restart-device', $device.InstanceId) | Out-Null
        }
        catch {
            Write-Warning "Targeted X4 restart did not complete: $($_.Exception.Message)"
        }

        Remove-TestCertificate $thumbprint

        $service = Get-DeviceService $device.InstanceId
        if ($service -ne $ExpectedService) {
            throw "Rollback completed package removal but X4 base service is not usbaudio2: $service"
        }

        if ($state) {
            $rollbackRecord = [ordered]@{
                Schema = 1
                RolledBackAt = (Get-Date).ToString('o')
                TargetInstanceId = $device.InstanceId
                BaseService = $service
                RemovedExtensionInf = $extensionPublishedInf
                RemovedApoInf = $apoPublishedInf
                RemovedCertificateThumbprint = $thumbprint
            }
            $rollbackPath = Join-Path $PackageRoot ('X4_STAGE_A2_ROLLBACK_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '.json')
            $rollbackRecord | ConvertTo-Json -Depth 5 | Set-Content -Path $rollbackPath -Encoding UTF8
            Remove-Item $StateFile -Force
            Write-Host "Rollback record: $rollbackPath"
        }

        Write-Host 'ROLLBACK RESULT: PASS - X4 MI_03 remains on Microsoft usbaudio2.'
    }
}
