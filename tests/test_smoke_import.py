"""Collection must not require models, GUI, camera enumeration or MediaPipe."""
import subprocess
import sys
from pathlib import Path
import unittest


class SmokeImportTests(unittest.TestCase):
    def test_import_has_no_runtime_side_effects(self):
        root = Path(__file__).resolve().parents[1]
        code = """
import builtins
original = builtins.__import__
def guarded(name, *args, **kwargs):
    if name.split('.')[0] in {'teleport_cam', 'numpy', 'tkinter', 'setup_models'}:
        raise AssertionError('runtime import during collection: ' + name)
    return original(name, *args, **kwargs)
builtins.__import__ = guarded
import smoke_test
assert callable(smoke_test.main)
"""
        result = subprocess.run([sys.executable, '-c', code], cwd=root,
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, '')
