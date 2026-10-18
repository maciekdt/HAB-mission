#!/bin/bash
set -eu
command -v rpicam-still >/dev/null
python3 -m py_compile "$HOME/pi_camera/app/camera_loop.py"
mkdir -p "$HOME/.config/systemd/user"
cat > "$HOME/.config/systemd/user/hab-camera.service" <<'UNIT'
[Unit]
Description=HAB camera test

[Service]
ExecStart=/usr/bin/python3 %h/pi_camera/app/camera_loop.py
Restart=on-failure
RestartSec=10

[Install]
WantedBy=default.target
UNIT
sudo loginctl enable-linger "$(id -un)"
systemctl --user daemon-reload
systemctl --user enable hab-camera.service
systemctl --user restart hab-camera.service
systemctl --user --no-pager status hab-camera.service
