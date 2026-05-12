<#
.SYNOPSIS
Lists or removes stale Windows PnP device instances created while testing DS5_Bridge firmware variants.

.DESCRIPTION
Windows treats changes to USB PID, serial number, interface layout, product string, and audio topology as new
device identities. This script targets the Sony DualSense/DualSense Edge identities used by this firmware and
helps remove old non-present instances after descriptor testing.

Dry-run is the default. Pass -Apply from an elevated PowerShell session to remove matched devices.
#>

[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
    [switch]$Apply,
    [switch]$IncludePresent,
    [switch]$IncludeBluetooth,
    [switch]$SkipAudioEndpoints
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$sonyDualSenseVidPidPattern = '(?i)VID_054C&(PID_0CE6|PID_0DF2)'
$dualsenseNamePattern = '(?i)(DualSense|DualSense Edge|Wireless Controller)'

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-DeviceCategory {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Device
    )

    $instanceId = [string]$Device.InstanceId
    $friendlyName = [string]$Device.FriendlyName

    if ($instanceId -match '(?i)^BTHENUM\\') {
        return 'Bluetooth pairing'
    }
    if ($Device.Class -eq 'AudioEndpoint') {
        return 'Audio endpoint'
    }
    if ($instanceId -match $sonyDualSenseVidPidPattern) {
        return 'USB/HID bridge stack'
    }
    if ($friendlyName -match $dualsenseNamePattern) {
        return 'Named DualSense device'
    }
    return 'Other'
}

function Test-TargetDevice {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Device
    )

    $instanceId = [string]$Device.InstanceId
    $friendlyName = [string]$Device.FriendlyName
    $isBluetooth = $instanceId -match '(?i)^BTHENUM\\'
    $isAudioEndpoint = $Device.Class -eq 'AudioEndpoint'

    if ($isBluetooth -and -not $IncludeBluetooth) {
        return $false
    }
    if ($isAudioEndpoint -and $SkipAudioEndpoints) {
        return $false
    }
    if (-not $IncludePresent -and $Device.Status -eq 'OK') {
        return $false
    }

    if ($instanceId -match $sonyDualSenseVidPidPattern) {
        return $true
    }
    if ($isAudioEndpoint -and $friendlyName -match $dualsenseNamePattern) {
        return $true
    }
    if ($isBluetooth -and $friendlyName -match $dualsenseNamePattern) {
        return $true
    }

    return $false
}

function Write-DeviceTable {
    param(
        [Parameter(Mandatory = $true)]
        [array]$Devices
    )

    $Devices |
        Sort-Object Category, FriendlyName, InstanceId |
        Format-Table -AutoSize Category, Class, Status, FriendlyName, InstanceId
}

if ($Apply -and -not (Test-IsAdministrator)) {
    throw 'Run PowerShell as Administrator before using -Apply.'
}

$pnpDevices = Get-PnpDevice
$targets = foreach ($device in $pnpDevices) {
    if (Test-TargetDevice -Device $device) {
        [pscustomobject]@{
            Category = Get-DeviceCategory -Device $device
            Class = $device.Class
            Status = $device.Status
            FriendlyName = $device.FriendlyName
            InstanceId = $device.InstanceId
        }
    }
}

$targets = @($targets | Sort-Object InstanceId -Unique)

if ($targets.Count -eq 0) {
    Write-Host 'No matching DS5_Bridge/DualSense device instances were found.'
    exit 0
}

Write-Host "Matched $($targets.Count) device instance(s)."
Write-DeviceTable -Devices $targets

Write-Host ''
Write-Host 'Full instance IDs:'
foreach ($target in ($targets | Sort-Object Category, FriendlyName, InstanceId)) {
    Write-Host "[$($target.Category)] $($target.InstanceId)"
}

if (-not $Apply) {
    Write-Host ''
    Write-Host 'Dry run only. Re-run from an elevated PowerShell session with -Apply to remove these instances.'
    Write-Host 'Use -IncludePresent only when the bridge/controller is unplugged and you intentionally want to remove live-looking entries.'
    Write-Host 'Use -IncludeBluetooth to include direct DualSense Bluetooth pairing records.'
    exit 0
}

Write-Host ''
Write-Host 'Removing matched device instances with pnputil...'
foreach ($target in $targets) {
    $instanceId = [string]$target.InstanceId
    if ($PSCmdlet.ShouldProcess($instanceId, 'Remove PnP device instance')) {
        & pnputil.exe /remove-device "$instanceId"
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "pnputil failed for: $instanceId"
        }
    }
}
