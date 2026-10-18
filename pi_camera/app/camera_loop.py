"""Take one photo per configured profile, then wait before the next series."""
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import time


def capture_command(profile, photo):
    command = [
        'rpicam-still', '--raw', '--nopreview', '--timeout', '3000',
        '--output', str(photo), '--metadata', str(photo.with_suffix('.json')),
        '--metadata-format', 'json',
    ]
    if profile['exposure_us'] is None:
        command += ['--exposure', 'sport']
    for key, flag in [('exposure_us', '--shutter'), ('gain', '--gain')]:
        value = profile[key]
        if value is not None:
            if value <= 0:
                raise ValueError(f'{key} must be positive or null')
            command += [flag, str(value)]
    if profile.get('ev') is not None:
        command += ['--ev', str(profile['ev'])]
    return command


def profile_folder(index, profile):
    exposure = profile['exposure_us']
    gain = profile['gain']
    exposure_label = 'auto' if exposure is None else f'{exposure}us'
    gain_label = 'auto' if gain is None else str(gain)
    return f'{index:02d}_exp-{exposure_label}_gain-{gain_label}_ev-{profile.get("ev", 0)}'


def main():
    config = json.loads(Path(__file__).with_name('config.json').read_text())
    output_dir = Path(config['output_dir']).expanduser()
    if not output_dir.is_absolute():
        raise ValueError('output_dir must be an absolute path or start with ~/')
    if config['interval_seconds'] <= 0 or not config['profiles']:
        raise ValueError('Use a positive interval and at least one profile')
    # Validate all profiles before starting a series.
    for profile in config['profiles']:
        capture_command(profile, output_dir / 'check.jpg')
    while True:
        series = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
        for index, profile in enumerate(config['profiles'], start=1):
            folder = output_dir / profile_folder(index, profile)
            folder.mkdir(parents=True, exist_ok=True)
            photo = folder / f'{series}.jpg'
            subprocess.run(capture_command(profile, photo), check=True, timeout=60)
            print(f'Saved {photo}', flush=True)
        time.sleep(config['interval_seconds'])


if __name__ == '__main__':
    main()
