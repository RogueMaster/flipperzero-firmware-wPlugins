"""Exercise production storage code on disposable POSIX-backed SD copies."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class StorageSafetyTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='subdup-build-')
        cls.addClassCleanup(cls.build.cleanup)
        source = (ROOT / 'storage_helper.c').read_text()
        header = (ROOT / 'storage_helper.h').read_text()
        header = re.sub(r'(?m)^#(?:include|pragma).*$', '', header)
        source = re.sub(r'(?m)^#include.*$', '', source)
        if 'storage_delete_duplicate(' in source:
            wrapper = 'static bool delete_checked(SubDupFinderApp* app,const char* name) { return storage_delete_duplicate(app,name) == StorageDeleteOk; }'
        else:
            wrapper = 'static bool delete_checked(SubDupFinderApp* app,const char* name) { char path[FULL_PATH_LEN]; assert(path_join(path,sizeof(path),app->scanned_dir,name)); return storage_delete_file(path); }'
        fixture = (ROOT / 'tests/storage_host.c').read_text()
        content = fixture.replace('/* PRODUCTION */', header + '\n' + source + '\n' + wrapper)
        path = Path(cls.build.name) / 'fixture.c'
        path.write_text(content)
        cls.binary = Path(cls.build.name) / 'fixture'
        result = subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                                 '-Wno-unused-function', '-fsanitize=address,undefined', '-I', str(ROOT),
                                 str(path), str(ROOT / 'logic.c'), '-o', str(cls.binary)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def check(self, case):
        with tempfile.TemporaryDirectory(prefix='subdup-copies-') as folder:
            result = subprocess.run([str(self.binary), folder, case], capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_crc_collision_is_not_displayed_as_duplicate(self): self.check('collision')
    def test_changed_selected_file_is_preserved(self): self.check('selected-changed')
    def test_changed_or_missing_peer_is_preserved(self):
        self.check('peer-changed')
        self.check('peer-missing')
    def test_identical_copy_delete_preserves_the_other_file(self): self.check('delete')
    def test_delete_failure_preserves_database_and_file(self): self.check('delete-failure')
    def test_short_read_is_not_a_duplicate(self): self.check('short-read')


if __name__ == '__main__':
    unittest.main()
