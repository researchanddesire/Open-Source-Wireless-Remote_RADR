#!/usr/bin/env python3
"""Comment on a PR with the Test Rig's hardware result and device video.

The rig records each device it flashes (see scripts/hil/video.py) into the
hardware evidence artifact. This turns every recording into an inline GIF
preview and a web MP4, stores both in the private Supabase Storage bucket
`hil-evidence`, and posts one comment per run attempt, on pass and fail.
Missing video or storage never hides the result: the comment says what is absent.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

BUCKET = 'hil-evidence'
SIGNED_SECONDS = 365 * 24 * 3600
PREVIEW_SECONDS = 30
MAX_GIF_BYTES = 9_000_000
PRODUCTS = {'lockbox': 'Lockbox', 'dtt': 'DTT', 'ossm': 'OSSM', 'radr': 'RADR'}
ICONS = {'passed': '✅', 'failed': '❌'}


def request(url, *, method='GET', headers=None, body=None, timeout=120):
    data = body if isinstance(body, (bytes, type(None))) else json.dumps(body).encode()
    req = urllib.request.Request(url, data=data, method=method, headers=headers or {})
    if data is not None and not isinstance(body, bytes):
        req.add_header('Content-Type', 'application/json')
    with urllib.request.urlopen(req, timeout=timeout) as response:
        payload = response.read()
    return json.loads(payload) if payload else {}


class Storage:
    """Private bucket access through the project's service key."""

    def __init__(self, project, access_token):
        keys = request(f'https://api.supabase.com/v1/projects/{project}/api-keys?reveal=true',
                       headers={'Authorization': f'Bearer {access_token}'})
        key = next((k['api_key'] for k in keys if k.get('name') == 'service_role'), None) or next(
            (k['api_key'] for k in keys if k.get('type') == 'secret' and k.get('api_key')), None)
        if not key:
            raise RuntimeError('no service key is available for the evidence project')
        print(f'::add-mask::{key}')
        self.headers = {'apikey': key, 'Authorization': f'Bearer {key}'}
        self.base = f'https://{project}.supabase.co/storage/v1'

    def ensure_bucket(self):
        try:
            request(f'{self.base}/bucket/{BUCKET}', headers=self.headers)
        except urllib.error.HTTPError as error:
            if error.code not in (400, 404):
                raise
            request(f'{self.base}/bucket', method='POST', headers=self.headers,
                    body={'id': BUCKET, 'name': BUCKET, 'public': False, 'file_size_limit': 52428800,
                          'allowed_mime_types': ['video/mp4', 'image/gif', 'image/jpeg']})

    def publish(self, path, file, content_type):
        quoted = urllib.parse.quote(path)
        request(f'{self.base}/object/{BUCKET}/{quoted}', method='POST', body=file.read_bytes(),
                headers={**self.headers, 'Content-Type': content_type, 'x-upsert': 'true',
                         'Cache-Control': 'max-age=31536000'}, timeout=300)
        signed = request(f'{self.base}/object/sign/{BUCKET}/{quoted}', method='POST',
                         headers=self.headers, body={'expiresIn': SIGNED_SECONDS})
        return self.base + signed['signedURL']


def ffmpeg(*args):
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', *args], check=True)


def duration(video):
    out = subprocess.run(['ffprobe', '-v', 'error', '-show_entries', 'format=duration', '-of', 'csv=p=0',
                          str(video)], capture_output=True, text=True, check=True).stdout
    return float(out.strip() or 0)


def preview(video, target):
    """Speed the whole recording up to a short looping GIF, shrinking until it fits."""
    speed = max(1.0, duration(video) / PREVIEW_SECONDS)
    for fps, size in ((6, 360), (5, 300), (4, 240)):
        scale = f"scale='if(gt(iw,ih),{size},-2)':'if(gt(iw,ih),-2,{size})':flags=lanczos"
        ffmpeg('-i', str(video), '-filter_complex',
               f'setpts=PTS/{speed:.3f},fps={fps},{scale},split[a][b];'
               '[a]palettegen=max_colors=96:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4',
               '-loop', '0', str(target))
        if target.stat().st_size <= MAX_GIF_BYTES:
            return speed
    raise RuntimeError('preview GIF is too large')


def web_video(video, target):
    ffmpeg('-i', str(video), '-an', '-vf', 'scale=-2:min(ih\\,540)', '-c:v', 'libx264', '-preset', 'veryfast',
           '-crf', '30', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(target))


def load(path):
    try:
        return json.loads(path.read_text(encoding='utf-8-sig'))
    except (OSError, ValueError):
        return None


def seconds(value):
    return f'{value:.0f} s' if isinstance(value, (int, float)) else '—'


def device_detail(device):
    parts = []
    if device.get('error'):
        parts.append(device['error'])
    if device.get('heartbeats') is not None:
        parts.append(f"{device['heartbeats']} heartbeats")
    if device.get('soak_seconds'):
        parts.append(f"{seconds(device['soak_seconds'])} stable")
    return '; '.join(parts) or '—'


def guard_section(guard, media):
    if not guard:
        return []
    lines = ['', '### OSSM RS-485 safety guard', '']
    before, after = guard.get('before') or {}, guard.get('after') or {}
    lines.append(f"- Before flashing: {guard.get('preflight', 'not checked')}")
    lines.append(f"- Motor control during the run: {guard.get('motion', 'latched off')}")
    faults = guard.get('faults') or []
    lines.append(f"- Guard faults during the run: {len(faults)}" + (
        ' — ' + '; '.join(f.get('reason', '?') for f in faults[:5]) if faults else ''))
    contacts = guard.get('contacts') or []
    if contacts:
        lines.append('- Hard-stop contacts: ' + ', '.join(str(c) for c in contacts))
    if after:
        lines.append(f"- After the run: rail {'latched off' if (after.get('rail') or {}).get('critical') else 'enabled'}, "
                     f"guard {'safe' if after.get('safe') else 'NOT safe'}")
    images = [f'<img src="{url}" width="360" alt="{label}">' for label, url in media]
    if images:
        lines += ['', ' '.join(images)]
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--pr', required=True)
    parser.add_argument('--hardware-result', required=True)
    args = parser.parse_args()
    repository, run_id = os.environ['GITHUB_REPOSITORY'], os.environ['GITHUB_RUN_ID']
    attempt, sha = os.environ['GITHUB_RUN_ATTEMPT'], os.environ['GITHUB_SHA']
    server = os.environ.get('GITHUB_SERVER_URL', 'https://github.com')
    run_url = f'{server}/{repository}/actions/runs/{run_id}/attempts/{attempt}'

    results = load(args.evidence / 'results' / 'results.json') or {}
    manifest = load(args.evidence / 'results' / 'video' / 'manifest.json') or {}
    guard = load(args.evidence / 'results' / 'rs485-guard.json')
    devices = results.get('devices') or []
    product = manifest.get('product') or next((d.get('product') for d in devices if d.get('product')), '')
    passed = args.hardware_result == 'success' and devices and all(d.get('status') == 'passed' for d in devices)
    headline = 'passed' if passed else 'cancelled' if args.hardware_result == 'cancelled' else 'failed'
    icon = ICONS.get(headline, '⚠️')

    notes, media = [], {}
    storage = None
    if manifest.get('views') or manifest.get('context'):
        try:
            if not os.environ.get('SUPABASE_ACCESS_TOKEN'):
                raise RuntimeError('SUPABASE_ACCESS_TOKEN is not available to this repository')
            storage = Storage(os.environ['SUPABASE_PROJECT_ID'], os.environ['SUPABASE_ACCESS_TOKEN'])
            storage.ensure_bucket()
        except Exception as error:  # noqa: BLE001 - the result comment must still post
            notes.append(f'Video storage unavailable: {error}')
            storage = None
    prefix = f"{repository.split('/')[1].lower()}/pr-{args.pr}/{run_id}-{attempt}"
    with tempfile.TemporaryDirectory() as temp:
        temp = Path(temp)
        for view in manifest.get('views') or []:
            source = args.evidence / 'results' / 'video' / view.get('file', '')
            if not storage or not source.is_file():
                continue
            name = Path(view['file']).stem
            try:
                gif, mp4 = temp / f'{name}.gif', temp / f'{name}.mp4'
                speed = preview(source, gif)
                web_video(source, mp4)
                media[view.get('variant')] = dict(
                    gif=storage.publish(f'{prefix}/{name}.gif', gif, 'image/gif'),
                    mp4=storage.publish(f'{prefix}/{name}.mp4', mp4, 'video/mp4'),
                    speed=speed, length=duration(source), label=view.get('label') or name)
            except Exception as error:  # noqa: BLE001
                notes.append(f"Could not publish the {view.get('label', name)} video: {error}")
        context = []
        for item in manifest.get('context') or []:
            source = args.evidence / 'results' / 'video' / item.get('file', '')
            if storage and source.is_file():
                try:
                    context.append((item.get('label', source.stem),
                                    storage.publish(f"{prefix}/{source.name}", source, 'image/jpeg')))
                except Exception as error:  # noqa: BLE001
                    notes.append(f"Could not publish {source.name}: {error}")

    lines = [f'<!-- rad-hil-evidence run={run_id} attempt={attempt} -->',
             f'## {icon} Hardware validation {headline}' + (f' — {PRODUCTS.get(product, product)}' if product else ''),
             '',
             f'Merge commit `{sha[:12]}` on the Test Rig · [workflow run]({run_url})', '']
    if devices:
        lines += ['| Device | Result | Detail | Video |', '|---|---|---|---|']
        for device in devices:
            variant = device.get('variant', '?')
            item = media.get(variant)
            label = item['label'] if item else variant
            video = f"[MP4, {seconds(item['length'])}]({item['mp4']})" if item else '—'
            status = device.get('status', 'failed')
            lines.append(f"| {label} `{device.get('device_id', '')}` | {ICONS.get(status, '⚠️')} {status} "
                         f"| {device_detail(device)} | {video} |")
    else:
        lines.append(f'The rig produced no device results (hardware job: {args.hardware_result}).')
    for variant, item in media.items():
        lines += ['', f"**{item['label']}** — whole run at {item['speed']:.0f}× speed; the clock in the corner is "
                      'seconds since recording started.', '', f'<img src="{item["gif"]}" alt="{item["label"]} on the rig">']
    events = manifest.get('events') or []
    if events:
        lines += ['', '<details><summary>Timeline</summary>', '', '| Video time | Device | Event |', '|---|---|---|']
        lines += [f"| {e.get('t', 0):.0f} s | {e.get('variant', '')} | {e.get('event', '')} |" for e in events]
        lines += ['', '</details>']
    lines += guard_section(guard, context)
    if not manifest:
        notes.append('The rig did not record video for this run.')
    elif manifest.get('error'):
        notes.append(f"Recording problem on the rig: {manifest['error']}")
    if notes:
        lines += ['', *[f'> {note}' for note in notes]]
    lines += ['', 'Full logs and the original recordings are in the `hardware-evidence` artifact of the run.']
    body = '\n'.join(lines) + '\n'

    marker = lines[0]
    comments = subprocess.run(['gh', 'api', '--paginate', f'repos/{repository}/issues/{args.pr}/comments',
                               '--jq', f'.[] | select(.body | startswith("{marker}")) | .id'],
                              capture_output=True, text=True, check=True).stdout.split()
    payload = json.dumps({'body': body})
    target = (['--method', 'PATCH', f'repos/{repository}/issues/comments/{comments[0]}'] if comments
              else ['--method', 'POST', f'repos/{repository}/issues/{args.pr}/comments'])
    subprocess.run(['gh', 'api', *target, '--input', '-'], input=payload, text=True, check=True,
                   stdout=subprocess.DEVNULL)
    print(body)
    return 0


if __name__ == '__main__':
    sys.exit(main())
