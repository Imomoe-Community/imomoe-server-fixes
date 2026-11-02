from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "migrate_config.py"
SPEC = importlib.util.spec_from_file_location("migrate_config", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
migrate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(migrate)


class MigrationTests(unittest.TestCase):
    def test_true_and_false_are_preserved(self) -> None:
        for source_value, expected in (("true", "1"), ("false", "0")):
            with self.subTest(source_value=source_value), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / "funrounds.toml"
                output = root / "imomoe_server_fixes.cfg"
                source.write_text(f"hostage_rescue_compat = {source_value}\n", encoding="utf-8")

                value = migrate.migrated_value(source)
                migrate.write_target(output, value, overwrite=False)

                self.assertEqual(output.read_text(encoding="utf-8").splitlines()[-1], f"sf_hostage_compat {expected}")
                self.assertEqual(source.read_text(encoding="utf-8"), f"hostage_rescue_compat = {source_value}\n")

    def test_omitted_legacy_key_uses_its_false_default(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "funrounds.toml"
            source.write_text("", encoding="utf-8")
            self.assertFalse(migrate.migrated_value(source))

    def test_non_boolean_legacy_value_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "funrounds.toml"
            source.write_text('hostage_rescue_compat = "false"\n', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "must be a TOML boolean"):
                migrate.migrated_value(source)

    def test_output_is_required_by_cli(self) -> None:
        with self.assertRaises(SystemExit) as failure:
            migrate.main(["missing.toml"])
        self.assertEqual(failure.exception.code, 2)

    def test_existing_output_requires_explicit_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "existing.cfg"
            output.write_text("user setting\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "pass --overwrite"):
                migrate.write_target(output, False, overwrite=False)
            self.assertEqual(output.read_text(encoding="utf-8"), "user setting\n")

    def test_explicit_overwrite_replaces_only_the_target(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source.toml"
            output = root / "target.cfg"
            source.write_text("hostage_rescue_compat = false\n", encoding="utf-8")
            output.write_text("previous output\n", encoding="utf-8")
            migrate.write_target(output, migrate.migrated_value(source), overwrite=True)
            self.assertTrue(output.read_text(encoding="utf-8").endswith("sf_hostage_compat 0\n"))
            self.assertEqual(source.read_text(encoding="utf-8"), "hostage_rescue_compat = false\n")


if __name__ == "__main__":
    unittest.main()
