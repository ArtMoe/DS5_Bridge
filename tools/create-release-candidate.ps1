param(
  [string] $OutputRoot = [Environment]::GetFolderPath('MyDocuments'),
  [string] $Configuration = 'Release',
  [switch] $SkipBuild,
  [switch] $NoZip,
  [string] $Label = ''
)

$ErrorActionPreference = 'Stop'

function Resolve-RepoRoot {
  $scriptPath = $PSScriptRoot
  if (-not $scriptPath) {
    throw 'Unable to resolve script root.'
  }
  return (Resolve-Path (Join-Path $scriptPath '..')).Path
}

function Read-JsonFile([string] $Path) {
  return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Read-FirmwareVersion([string] $CompanionSourcePath) {
  $source = Get-Content -LiteralPath $CompanionSourcePath -Raw
  $major = [regex]::Match($source, 'kFirmwareMajor\s*=\s*(\d+)')
  $minor = [regex]::Match($source, 'kFirmwareMinor\s*=\s*(\d+)')
  $patch = [regex]::Match($source, 'kFirmwarePatch\s*=\s*(\d+)')
  if (-not ($major.Success -and $minor.Success -and $patch.Success)) {
    throw "Unable to read firmware version from $CompanionSourcePath"
  }
  return "$($major.Groups[1].Value).$($minor.Groups[1].Value).$($patch.Groups[1].Value)"
}

function Invoke-Step([string] $Name, [scriptblock] $Action) {
  Write-Host ""
  Write-Host "==> $Name"
  & $Action
}

function Copy-Directory([string] $Source, [string] $Destination) {
  if (Test-Path -LiteralPath $Destination) {
    Remove-Item -LiteralPath $Destination -Recurse -Force
  }
  Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

$repoRoot = Resolve-RepoRoot
$companionRoot = Join-Path $repoRoot 'companion'
$firmwareBuildDir = Join-Path $repoRoot 'build\companion'
$firmwareOutput = Join-Path $firmwareBuildDir 'ds5-bridge.uf2'
$installerDir = Join-Path $companionRoot 'artifacts\installer'
$portableArtifactsDir = Join-Path $companionRoot 'artifacts'
$companionPackage = Read-JsonFile (Join-Path $companionRoot 'package.json')
$companionVersion = [string] $companionPackage.version
$firmwareVersion = Read-FirmwareVersion (Join-Path $repoRoot 'src\companion.cpp')
$stamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
$labelSuffix = if ([string]::IsNullOrWhiteSpace($Label)) { '' } else { " $($Label.Trim())" }
$releaseDir = Join-Path $OutputRoot "DS5 Bridge Release Candidate$labelSuffix $stamp"

if (-not (Test-Path -LiteralPath $OutputRoot)) {
  New-Item -ItemType Directory -Path $OutputRoot | Out-Null
}

$buildStartedAt = Get-Date

if (-not $SkipBuild) {
  Invoke-Step 'Build firmware UF2' {
    Push-Location $repoRoot
    try {
      cmake --build $firmwareBuildDir --target ds5-bridge --config $Configuration
    } finally {
      Pop-Location
    }
  }

  Invoke-Step 'Build portable companion package' {
    Push-Location $companionRoot
    try {
      npm run package:win
    } finally {
      Pop-Location
    }
  }

  Invoke-Step 'Build companion installer' {
    Push-Location $companionRoot
    try {
      npm run installer:win
    } finally {
      Pop-Location
    }
  }
}

Invoke-Step 'Collect artifacts' {
  if (-not (Test-Path -LiteralPath $firmwareOutput)) {
    throw "Missing firmware output: $firmwareOutput"
  }

  $installer = Get-ChildItem -LiteralPath $installerDir -File -Filter '*.exe' |
    Where-Object { $_.Name -like 'DS5-Bridge-Companion-Setup-*' } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
  if (-not $installer) {
    throw "Missing companion installer in $installerDir"
  }

  $portable = Get-ChildItem -LiteralPath $portableArtifactsDir -Directory |
    Where-Object {
      $_.Name -like 'DS5 Bridge-win32-x64-*' -and ($SkipBuild -or $_.LastWriteTime -ge $buildStartedAt.AddMinutes(-1))
    } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
  if (-not $portable) {
    throw "Missing portable companion package in $portableArtifactsDir"
  }

  New-Item -ItemType Directory -Path $releaseDir | Out-Null

  $firmwareName = "DS5-Bridge-Firmware-v$firmwareVersion.uf2"
  $installerName = "DS5-Bridge-Companion-Setup-v$companionVersion.exe"
  $portableName = "DS5-Bridge-Companion-Portable-v$companionVersion-win32-x64"
  $portableDestination = Join-Path $releaseDir $portableName
  $portableZipDestination = Join-Path $releaseDir "$portableName.zip"

  Copy-Item -LiteralPath $firmwareOutput -Destination (Join-Path $releaseDir $firmwareName) -Force
  Copy-Item -LiteralPath $installer.FullName -Destination (Join-Path $releaseDir $installerName) -Force
  Copy-Directory $portable.FullName $portableDestination

  if (-not $NoZip) {
    Compress-Archive -LiteralPath $portableDestination -DestinationPath $portableZipDestination -Force
  }

  $manifest = [ordered]@{
    createdAt = (Get-Date).ToString('o')
    firmwareVersion = $firmwareVersion
    companionVersion = $companionVersion
    source = $repoRoot
    artifacts = @(
      $firmwareName,
      $installerName,
      $portableName
    )
  }
  if (-not $NoZip) {
    $manifest.artifacts += "$portableName.zip"
  }
  $manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $releaseDir 'manifest.json') -Encoding UTF8
}

Write-Host ""
Write-Host "Release candidate created:"
Write-Host $releaseDir
Get-ChildItem -LiteralPath $releaseDir | Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
