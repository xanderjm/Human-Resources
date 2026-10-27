"""The enamel atlas is explicit, non-overlapping and source-bound."""
import ast
import copy
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
RECIPE = 'assets/graphics/atomic-ui/v1/recipe.json'


class UiComponentRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Exercise the production metadata validator using stdlib only. Pixel
        # cooking is covered separately through the pinned art interpreter.
        source = ast.parse((ROOT / 'tools/cook_ui_components.py').read_text())
        nodes = [n for n in source.body if isinstance(n, ast.FunctionDef) and
                 n.name in ('checked_recipe', 'source_paths')]
        namespace = {'ROOT': ROOT, 'RECIPE': RECIPE, 'json': json, 'Path': Path}
        exec(compile(ast.Module(body=nodes, type_ignores=[]), 'ui-recipe-validator', 'exec'), namespace)
        cls.check = staticmethod(namespace['checked_recipe'])
        cls.sources = staticmethod(namespace['source_paths'])

    def candidate(self, mutate):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            recipe = copy.deepcopy(self.check())
            mutate(recipe)
            path = root / RECIPE
            path.parent.mkdir(parents=True)
            path.write_text(json.dumps(recipe))
            self.check(root)

    def test_explicit_component_and_icon_set(self):
        recipe = self.check()
        self.assertEqual(len(recipe['regions']), 43)
        self.assertEqual(len({r['id'] for r in recipe['regions']}), 43)
        for source in self.sources():
            self.assertTrue((ROOT / source).is_file(), source)
        self.assertIn('assets/graphics/atomic-ui/v1/icons.png', self.sources())
        self.assertIn('assets/fonts/human-resources-v3/ttf/HR-Abduction-Logo-Black-Italic.ttf', self.sources())

    def test_overlap_and_font_collision_are_rejected(self):
        with self.assertRaises(ValueError):
            self.candidate(lambda d: d['regions'][1].update(slot=d['regions'][0]['slot']))
        with self.assertRaises(ValueError):
            self.candidate(lambda d: d['regions'][0].update(slot=[0, 1088, 1024, 480]))

    def test_slice_may_not_cross_a_fixed_rivet(self):
        def cut_rivet(recipe):
            next(r for r in recipe['regions'] if r['id']=='navy_panel')['slice']=[.07,.18,.07,.18]
        with self.assertRaisesRegex(ValueError,'protected decorative detail'):
            self.candidate(cut_rivet)

    def test_outside_duplicate_and_bad_slice_are_rejected(self):
        for mutate in (
            lambda d: d['regions'][0].update(slot=[2040, 0, 1024, 512]),
            lambda d: d['regions'][1].update(id=d['regions'][0]['id']),
            lambda d: d['regions'][0].update(slice=[.8, 0, 0, 0]),
        ):
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                self.candidate(mutate)


if __name__ == '__main__':
    unittest.main()
