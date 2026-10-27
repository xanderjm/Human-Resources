"""Reject accidental expansion or omission at the product shipping boundary."""
import ast
import copy
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ProductContentRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # The validator has no art dependencies. Load that exact production
        # contract without importing the optional offline NumPy/Pillow cooker.
        source = ast.parse((ROOT / 'tools/cook_product_content.py').read_text())
        definitions = [node for node in source.body if
                       isinstance(node, ast.FunctionDef) and node.name in {'validate_recipe', 'validate_graphic_regions', 'model_options'} or
                       isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id in
                       {'MODELS', 'MATERIALS', 'ANIMATED_MODELS', 'LOCATIONS', 'FONTS', 'ATLAS_GRAPHICS'} for t in node.targets)]
        namespace = {}
        exec(compile(ast.Module(body=definitions, type_ignores=[]), 'product-recipe-validator', 'exec'), namespace)
        cls.options = staticmethod(namespace['model_options'])
        cls.validate = staticmethod(namespace['validate_recipe'])
        cls.validate_regions = staticmethod(namespace['validate_graphic_regions'])
        cls.regions = namespace['ATLAS_GRAPHICS']
        cls.recipe = json.loads((ROOT / 'assets/runtime/game-content.json').read_text())

    def test_current_consumed_recipe_is_accepted(self):
        self.validate(self.recipe)
        self.assertEqual(len(self.recipe['models']), len(set(self.recipe['models'])))
        self.assertEqual(self.recipe['locations'], ['farmland', 'suburbia', 'desert'])

    def test_product_starter_is_not_the_upgraded_preview_assembly(self):
        self.assertEqual(self.options('ufo-modular'), {
            'source_path':'assets/models/ufo-modular/ufo-modular.glb',
            'exclude_nodes':('Antenna_Module','Dome','Dome_Collar','Outer_Ring_Pivot')})
        for part in ('antenna','beam','laser','rim-lens'):
            self.assertEqual(self.options('ufo-'+part), {
                'source_path':'assets/models/ufo-modular/part-'+part+'.glb'})
            self.assertIn('ufo-'+part, self.recipe['models'])
        self.assertEqual(self.options('shield-shell'), {
            'source_path':'assets/graphics/fx-shield-shell/unit-sphere.glb',
            'opaque_material':True})
        self.assertEqual(self.options('cow'), {})
        self.assertEqual(self.options('bush-low'), {})
        for asset in ('crop-seedling', 'grass-tuft'):
            self.assertEqual(self.options(asset), {
                'source_path':f'assets/models/{asset}/{asset}-runtime.glb'})

    def test_graphic_regions_cannot_pollute_transparent_card_margins(self):
        self.validate_regions(self.regions)
        former = copy.deepcopy(self.regions)
        former[1] = ('ui-xp-track', (0, 1344), (768, 128))
        with self.assertRaises(ValueError):
            self.validate_regions(former)
        with self.assertRaises(ValueError):
            self.validate_regions([('outside', (1020, 2000), (16, 80))])

    def test_missing_consumed_source_is_rejected(self):
        for key in ('models', 'materials', 'animated_models', 'fonts', 'locations'):
            candidate = copy.deepcopy(self.recipe)
            candidate[key].pop()
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.validate(candidate)

    def test_unreviewed_or_duplicate_sources_are_rejected(self):
        for addition in ('unreviewed-new-model', self.recipe['models'][0]):
            candidate = copy.deepcopy(self.recipe)
            candidate['models'].append(addition)
            with self.subTest(addition=addition), self.assertRaises(ValueError):
                self.validate(candidate)
        candidate = copy.deepcopy(self.recipe)
        candidate['authoring_tree'] = 'assets/'
        with self.assertRaises(ValueError):
            self.validate(candidate)


if __name__ == '__main__':
    unittest.main()
