$ErrorActionPreference = 'Stop'
$config = Get-Content (Join-Path $PSScriptRoot 'deploy.local.json') -Raw | ConvertFrom-Json
$destination = "$($config.username)@$($config.host)"

& ssh $destination 'mkdir -p ~/pi_camera'
if ($LASTEXITCODE -ne 0) { throw 'SSH connection failed' }

& scp -r (Join-Path $PSScriptRoot 'app') "${destination}:pi_camera/"
if ($LASTEXITCODE -ne 0) { throw 'File transfer failed' }

& ssh -t $destination 'bash ~/pi_camera/app/install.sh'
if ($LASTEXITCODE -ne 0) { throw 'Installation failed' }
