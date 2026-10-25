#!/usr/bin/env python3
"""Audit source-bound base-module contents from an unsigned release AAB."""
import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET
import zipfile
from audit_android_native import audit_archive
from audit_asset_package import audit_zip
from cook_app_icons import ROOT, verify_provenance


def audit(bundle: Path, config: Path, manifest: Path) -> None:
    document = json.loads(config.read_text())
    alignments = []
    def walk(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if key == 'alignment': alignments.append(child)
                walk(child)
        elif isinstance(value, list):
            for child in value: walk(child)
    walk(document)
    if alignments != ['PAGE_ALIGNMENT_16K']:
        raise ValueError(f'Bundle must request 16 KiB generated-APK alignment: {alignments}')
    project = json.loads((ROOT / 'project.json').read_text())
    xml = ET.parse(manifest).getroot()
    android = '{http://schemas.android.com/apk/res/android}'
    sdk = xml.find('uses-sdk')
    if xml.get('package') != project['identity']['android_package'] or sdk is None:
        raise ValueError('Unexpected bundle application identity/SDK metadata')
    if int(sdk.get(android + 'targetSdkVersion', '0')) != project['platform_settings']['android_target_sdk']:
        raise ValueError('Bundle target SDK differs from project authority')
    app = xml.find('application')
    if app is None or app.get(android + 'debuggable', 'false') != 'false':
        raise ValueError('Release bundle is unexpectedly debuggable')
    activities = app.findall('activity')
    if len(activities) != 1 or activities[0].get(android + 'screenOrientation') not in ('portrait', '1'):
        raise ValueError('Bundle activity does not declare product portrait orientation')
    audit_archive(bundle, 'base/')
    with zipfile.ZipFile(bundle) as archive:
        names = {entry.filename for entry in archive.infolist() if not entry.is_dir()}
        if any(name.endswith(('.glb', '.hlsl', '.hlsli', '.metal', '.air', '.dxil')) for name in names):
            raise ValueError('Bundle includes authoring models or shader sources/wrong backend')
        for name in names:
            if name.startswith('base/assets/') and not (name.startswith('base/assets/runtime/') or
                    name.startswith('base/assets/shaders/') or name == 'base/assets/app-icon-provenance.json'):
                raise ValueError(f'Unallowlisted bundle asset: {name}')
        verify_provenance(json.loads(archive.read('base/assets/app-icon-provenance.json')), ROOT / 'assets/graphics/app-icon')
    for kind, sources in (('shaders', ROOT / 'shaders'), ('runtime', ROOT / 'assets/runtime')):
        audit_zip(bundle, 'base/assets/' + kind, kind, ROOT / 'build/android-assets' / kind / 'package-manifest.json', sources)
    print('Unsigned AAB native identity, 16 KiB alignment request and exact source-bound asset audits passed.')
    print('No signing, split installation, physical-device or store acceptance is implied.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    audit(args.bundle, args.config, args.manifest)
