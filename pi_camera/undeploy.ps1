$ErrorActionPreference = 'Stop'
$config = Get-Content (Join-Path $PSScriptRoot 'deploy.local.json') -Raw | ConvertFrom-Json
$destination = "$($config.username)@$($config.host)"

$command = @'
set -eu
if systemctl --user cat hab-camera.service >/dev/null 2>&1; then
    systemctl --user disable --now hab-camera.service
fi
rm -f -- "$HOME/.config/systemd/user/hab-camera.service"
systemctl --user daemon-reload
rm -rf -- "$HOME/pi_camera"
echo 'Undeployed. Photos remain in ~/hab-photos.'
'@

& ssh $destination $command.Replace("`r", '')
if ($LASTEXITCODE -ne 0) { throw 'Undeployment failed' }
