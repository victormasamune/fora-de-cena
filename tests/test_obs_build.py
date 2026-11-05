"""Configure the plugin against an SDK contract, without OBS binaries or FFmpeg.

The Windows cross-configuration checks CMake package discovery only. It does not
compile the DLL or certify MSVC compatibility.
"""
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory
import unittest

ROOT = Path(__file__).resolve().parents[1]
CMAKE = shutil.which('cmake')


@unittest.skipUnless(CMAKE and shutil.which('g++') and shutil.which('make'),
                     'CMake discovery regression checks require CMake, g++ and make.')
class ObsBuildDiscoveryTests(unittest.TestCase):
    def configure(self, root, *, transitive_failure=False, optional_hints=False):
        sdk = root / 'sdk'
        sdk.mkdir()
        config = 'add_library(OBS::libobs INTERFACE IMPORTED)\n'
        if transitive_failure:
            config = ('include(CMakeFindDependencyMacro)\n'
                      'find_dependency(FdcMissingSdkRuntime CONFIG REQUIRED)\n') + config
        (sdk / 'libobsConfig.cmake').write_text(config)
        poison = root / 'poison'
        poison.mkdir()
        # Reproduce the old failure even on a host that happens to have FFmpeg.
        (poison / 'FindFFmpeg.cmake').write_text(
            'message(FATAL_ERROR "unexpected direct FFmpeg dependency")\n')
        command = [CMAKE, '-S', str(ROOT / 'obs-plugin'), '-B', str(root / 'build'),
                   '-G', 'Unix Makefiles', '-DCMAKE_SYSTEM_NAME=Windows',
                   '-DCMAKE_CXX_COMPILER=' + shutil.which('g++'),
                   '-DCMAKE_CXX_COMPILER_WORKS=TRUE', '-DCMAKE_CXX_ABI_COMPILED=TRUE',
                   '-Dlibobs_DIR=' + str(sdk), '-DCMAKE_MODULE_PATH=' + str(poison)]
        if optional_hints:
            command += ['-DOBS_DEPS_ROOT=' + str(root / 'old-deps'),
                        '-DOBS_SOURCE_ROOT=' + str(root / 'old-source')]
        return subprocess.run(command, capture_output=True, text=True, timeout=30)

    def test_windows_sdk_does_not_need_direct_ffmpeg_or_custom_roots(self):
        with TemporaryDirectory() as directory:
            result = self.configure(Path(directory))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_cached_source_and_deps_hints_do_not_reintroduce_ffmpeg(self):
        with TemporaryDirectory() as directory:
            result = self.configure(Path(directory), optional_hints=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_sdk_dependency_is_not_silently_bypassed(self):
        with TemporaryDirectory() as directory:
            result = self.configure(Path(directory), transitive_failure=True, optional_hints=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('FdcMissingSdkRuntime', result.stdout + result.stderr)
