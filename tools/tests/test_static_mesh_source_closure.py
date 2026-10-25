"""Reviewed mesh authorities remain bound after cooking into a package."""

import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/generators'))

from audit_asset_package import audit_directory, audit_zip
import cook_runtime_assets
from static_mesh_optimizations import checked_recipe, provenance


class StaticMeshSourceClosureTests(unittest.TestCase):
    def test_cooked_manifest_rejects_changed_embedded_authorities(self):
        recipe = checked_recipe(ROOT)
        models = [{'id': row['id'], 'source': row['runtime_source'],
                   'source_sha256': row['runtime_source_sha256'],
                   'static_mesh_optimization': provenance(recipe, row['id'])}
                  for row in recipe['models']]
        omitted = {'assets/production/static-mesh-optimizations.json',
                   'tools/static_mesh_optimizations.py', 'tools/build_static_mesh_optimizations.py'}
        omitted.update(row['source'] for row in recipe['models'])
        authorities = omitted | set(recipe['tools'])
        authorities.update(row['runtime_source'] for row in recipe['models'])

        with tempfile.TemporaryDirectory(prefix='mesh-source-closure-') as directory:
            repository = Path(directory)
            source = repository / 'assets/runtime'
            source.mkdir(parents=True)
            (source / 'game-content.json').write_text(json.dumps({'materials': [], 'fonts': []}))

            # The art producer is the only substitute. Exercise the real cooker,
            # manifest writer and both release auditors using its exact schema.
            common = {'tools/art_tools.py', 'tools/art-toolchain.json', 'tools/art-requirements.txt',
                      'tools/cook_product_content.py', 'tools/cook_style_preview.py',
                      'tools/normalize_glb.py', 'assets/production/inventory.json',
                      'design/balance/endless-upgrade-system.json',
                      'assets/fonts/human-resources-v3/LICENSE.txt'}
            common.update(f'assets/graphics/{name}/{name}.png' for name in
                          ['ui-coin', 'ui-shield', 'ui-beam', 'ui-laser', 'ui-hud-panel',
                           'ui-upgrade-card', 'ui-xp-track'])
            for name in common | authorities:
                target = repository / name
                target.parent.mkdir(parents=True, exist_ok=True)
                if name in authorities:
                    shutil.copyfile(ROOT / name, target)
                else:
                    target.write_bytes(b'unrelated authored fixture')

            def produce_art(args, **_):
                output = Path(args[args.index('--output') + 1])
                output.mkdir(parents=True)
                (output / 'content-provenance.json').write_text(json.dumps({'models': models}))
                (output / 'animation-provenance.json').write_text(json.dumps({'tool_sha256': {}, 'models': []}))
                (output / 'fixture.hrm').write_bytes(b'cooked mesh fixture')

            output = repository / 'build/assets/runtime'
            with patch.object(cook_runtime_assets, 'art_python', return_value=Path(sys.executable)), \
                    patch.object(cook_runtime_assets.subprocess, 'run', side_effect=produce_art):
                cook_runtime_assets.cook(source, output, {'identity': {'slug': 'mesh-fixture'}})

            manifest = json.loads((output / 'package-manifest.json').read_text())
            recorded = [record['path'] for record in manifest['derived_sources']]
            self.assertEqual(recorded, sorted(set(recorded)))
            self.assertTrue(authorities <= set(recorded), authorities - set(recorded))
            archive = repository / 'candidate.apk'
            with zipfile.ZipFile(archive, 'w') as package:
                for path in output.rglob('*'):
                    if path.is_file():
                        package.write(path, 'assets/runtime/' + path.relative_to(output).as_posix())

            def audit():
                audit_directory(output, 'runtime', current_source_root=source)
                audit_zip(archive, 'assets/runtime', 'runtime', current_source_root=source)

            audit()
            for name in sorted(omitted):
                path = repository / name
                original = path.read_bytes()
                with self.subTest(authority=name):
                    # A same-size mutation proves digest binding beyond sizes.
                    path.write_bytes(bytes([original[0] ^ 1]) + original[1:])
                    with self.assertRaisesRegex(RuntimeError, 'derived runtime source changed: ' + name):
                        audit_directory(output, 'runtime', current_source_root=source)
                    with self.assertRaisesRegex(RuntimeError, 'derived runtime source changed: ' + name):
                        audit_zip(archive, 'assets/runtime', 'runtime', current_source_root=source)
                    path.write_bytes(original)
                    audit()


if __name__ == '__main__':
    unittest.main()
