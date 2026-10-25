"""Reject unreviewed or silently missing static mesh source exports."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('static_mesh_optimizations', ROOT / 'tools/static_mesh_optimizations.py')
mesh = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mesh)


class StaticMeshOptimizationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.recipe = json.loads((ROOT / mesh.RECIPE).read_text())
        paths = set(self.recipe['tools']) | {mesh.RECIPE}
        paths.update(row[key] for row in self.recipe['models'] for key in ('source', 'runtime_source'))
        for name in paths:
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)

    def write_recipe(self, value):
        (self.root / mesh.RECIPE).write_text(json.dumps(value))

    def test_exact_reviewed_exports_and_canonical_sources_are_accepted(self):
        recipe = mesh.checked_recipe(self.root)
        self.assertEqual({row['id'] for row in recipe['models']}, {'crop-seedling', 'grass-tuft'})
        self.assertEqual({row['triangles_after'] for row in recipe['models']}, {425, 453})
        for row in recipe['models']:
            provenance = mesh.provenance(recipe, row['id'], self.root)
            self.assertEqual(provenance['canonical_source_sha256'], row['source_sha256'])
            self.assertEqual(provenance['reviewed_runtime_source_sha256'], row['runtime_source_sha256'])
            self.assertEqual(provenance['recipe_sha256'], mesh.digest(self.root / mesh.RECIPE))

    @unittest.skipUnless(shutil.which('git'), 'requires Git checkout conversion')
    def test_windows_checkout_retains_exact_pinned_bytes(self):
        fixture_root = ROOT / 'build/test-fixtures'
        fixture_root.mkdir(parents=True, exist_ok=True)
        checkout_root = tempfile.TemporaryDirectory(prefix='static-mesh-checkout-', dir=fixture_root)
        self.addCleanup(checkout_root.cleanup)
        shutil.copytree(self.root, checkout_root.name, dirs_exist_ok=True)
        self.root = Path(checkout_root.name)
        environment = dict(os.environ, GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL=os.devnull)

        def git(*args):
            return subprocess.run(['git', *args], cwd=self.root, env=environment,
                                  check=True, capture_output=True, text=True)

        git('init', '--quiet')
        git('-c', 'core.autocrlf=false', 'add', '--all')
        git('config', 'core.autocrlf', 'true')
        git('config', 'core.eol', 'crlf')
        pinned = set(self.recipe['tools']) | {mesh.RECIPE}

        def checkout():
            for name in pinned:
                (self.root / name).unlink()
            git('checkout-index', '--all', '--force')

        # The control uses actual Git conversion, rather than a synthetic edit.
        checkout()
        for name in pinned:
            self.assertIn(b'\r\n', (self.root / name).read_bytes(), name)
        with self.assertRaisesRegex(ValueError, 'authoring tool hash mismatch'):
            mesh.checked_recipe(self.root)

        shutil.copyfile(ROOT / '.gitattributes', self.root / '.gitattributes')
        git('-c', 'core.autocrlf=false', 'add', '.gitattributes')
        checkout()
        for name in pinned:
            self.assertEqual((self.root / name).read_bytes(), (ROOT / name).read_bytes(), name)
        mesh.checked_recipe(self.root)

    def test_missing_export_fails_instead_of_falling_back_to_original(self):
        (self.root / self.recipe['models'][0]['runtime_source']).unlink()
        with self.assertRaisesRegex(ValueError, 'missing'):
            mesh.checked_recipe(self.root)

    def test_changed_original_export_or_tool_requires_new_review(self):
        names = [self.recipe['models'][0]['source'], self.recipe['models'][0]['runtime_source'],
                 'tools/build_static_mesh_optimizations.py']
        for name in names:
            path = self.root / name
            before = path.read_bytes()
            with self.subTest(name=name):
                path.write_bytes(before + b'changed')
                with self.assertRaisesRegex(ValueError, 'hash mismatch'):
                    mesh.checked_recipe(self.root)
                path.write_bytes(before)

    def test_recipe_rejects_added_duplicate_or_escaped_source(self):
        for mode in ('added', 'duplicate', 'escape', 'absolute', 'ratio', 'count'):
            recipe = copy.deepcopy(self.recipe)
            if mode == 'added':
                recipe['models'].append(copy.deepcopy(recipe['models'][0]))
            elif mode == 'duplicate':
                recipe['models'][1] = copy.deepcopy(recipe['models'][0])
            elif mode == 'escape':
                recipe['models'][0]['runtime_source'] = '../unreviewed.glb'
            elif mode == 'absolute':
                recipe['models'][0]['runtime_source'] = str(self.root / 'unreviewed.glb')
            elif mode == 'ratio':
                recipe['method']['ratio'] = .5
            else:
                recipe['models'][0]['triangles_after'] += 1
            with self.subTest(mode=mode):
                self.write_recipe(recipe)
                with self.assertRaises(ValueError):
                    mesh.checked_recipe(self.root)

    def test_symlinked_export_is_not_an_authored_source(self):
        row = self.recipe['models'][0]
        path = self.root / row['runtime_source']
        path.unlink()
        try:
            path.symlink_to(self.root / row['source'])
        except OSError:
            self.skipTest('Host cannot create symlink fixture')
        with self.assertRaisesRegex(ValueError, 'missing or outside'):
            mesh.checked_recipe(self.root)

    def test_authoring_replay_cannot_write_to_source_or_existing_directory(self):
        for destination in (self.root / 'assets', self.root / 'build'):
            with self.subTest(destination=destination):
                with self.assertRaisesRegex(ValueError, 'ignored build'):
                    mesh.replay(destination, self.root)
        destination = self.root / 'build/existing'
        destination.mkdir(parents=True)
        with self.assertRaises(FileExistsError):
            mesh.replay(destination, self.root)


if __name__ == '__main__':
    unittest.main()
