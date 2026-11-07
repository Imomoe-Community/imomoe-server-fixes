import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "build_paired.py"
SPEC = importlib.util.spec_from_file_location("serverfixes_build_paired", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)

class PairedBuildSourceTests(unittest.TestCase):
    def test_generated_include_contains_checked_in_sdk_header(self):
        header = MODULE.GENERATED_INCLUDE / "s2sdk/s2sdk.hpp"
        self.assertTrue(header.is_file(), f"Missing checked-in S2SDK header: {header}")

    def test_source_matches_resume_checkout_used_by_sdk_builder(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sdk_work = root / "new-sdk-work"
            resumed = root / "pinned-source"
            resumed.mkdir()
            self.assertEqual(MODULE.choose_sdk_source(sdk_work, resumed), resumed.resolve())

    def test_new_clone_source_is_under_sdk_work_directory(self):
        sdk_work = Path("/tmp/sdk-run")
        self.assertEqual(MODULE.choose_sdk_source(sdk_work, None), sdk_work / "source")

if __name__ == "__main__":
    unittest.main()
