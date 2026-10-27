"""Backup and flash sequencing checks; no USB device is accessed."""
import contextlib
import hashlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/orbit_clock.py"
spec = importlib.util.spec_from_file_location("orbit_clock_tasks", SCRIPT)
orbit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(orbit)


class BackupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.backup = Path(self.temp.name) / "original.bin"
        self.output = contextlib.redirect_stdout(io.StringIO())
        self.output.__enter__()
        self.addCleanup(self.output.__exit__, None, None, None)

    def test_retries_only_failed_segment_and_preserves_byte_order(self):
        calls = []

        def read(command, **kwargs):
            pos = command.index("read-flash")
            address = int(command[pos + 1], 16)
            calls.append(address)
            if calls == [0, 4]:
                raise subprocess.CalledProcessError(2, command)
            Path(command[-1]).write_bytes(b"ABCD" if address == 0 else b"EFGH")
            self.assertEqual(command[pos - 2:pos], ["--after", "no-reset"])

        with patch.object(orbit, "FLASH_BYTES", 8), patch.object(orbit, "BACKUP_CHUNK_BYTES", 4), \
             patch.object(orbit, "run", side_effect=read), patch.object(orbit.time, "sleep"):
            orbit.backup_flash(["esptool"], self.backup, {})
        self.assertEqual(calls, [0, 4, 4])
        self.assertEqual(self.backup.read_bytes(), b"ABCDEFGH")
        self.assertIn(hashlib.sha256(b"ABCDEFGH").hexdigest(), self.backup.with_suffix(".sha256").read_text())

    def test_short_reads_never_produce_a_complete_backup(self):
        def short(command, **kwargs):
            Path(command[-1]).write_bytes(b"bad")

        with patch.object(orbit, "FLASH_BYTES", 4), patch.object(orbit, "BACKUP_CHUNK_BYTES", 4), \
             patch.object(orbit, "run", side_effect=short) as run, patch.object(orbit.time, "sleep"):
            with self.assertRaisesRegex(RuntimeError, "未开始烧录"):
                orbit.backup_flash(["esptool"], self.backup, {})
        self.assertEqual(run.call_count, 3)
        self.assertFalse(self.backup.exists())

    def test_failed_backup_prevents_any_flash_write(self):
        with patch.object(orbit, "TASK", Path(self.temp.name)), \
             patch.object(orbit.Path, "is_file", return_value=True), \
             patch.object(orbit, "backup_flash", side_effect=RuntimeError("backup failed")), \
             patch.object(orbit, "run") as run, patch.object(orbit, "sync_time") as sync:
            with self.assertRaisesRegex(RuntimeError, "backup failed"):
                orbit.flash("test-port", {})
        run.assert_not_called()
        sync.assert_not_called()

    def test_success_uses_conservative_baud_and_syncs_after_write(self):
        with patch.object(orbit, "TASK", Path(self.temp.name)), \
             patch.object(orbit.Path, "is_file", return_value=True), \
             patch.object(orbit, "backup_flash") as backup, \
             patch.object(orbit, "run") as run, patch.object(orbit, "sync_time") as sync, \
             patch.object(orbit.time, "sleep"):
            orbit.flash("test-port", {})
        self.assertEqual(backup.call_args.args[0][-2:], ["--baud", "115200"])
        self.assertIn("write-flash", run.call_args.args[0])
        sync.assert_called_once_with("test-port")

    def test_explicit_direct_rom_mode_skips_backup_and_writes_then_syncs(self):
        with patch.object(orbit.Path, "is_file", return_value=True), \
             patch.object(orbit, "backup_flash") as backup, \
             patch.object(orbit, "run") as run, patch.object(orbit, "sync_time") as sync, \
             patch.object(orbit.time, "sleep"):
            orbit.flash("test-port", {}, skip_backup=True, no_stub=True)
        backup.assert_not_called()
        self.assertEqual(run.call_count, 1)
        command = run.call_args.args[0]
        self.assertIn("--no-stub", command[:command.index("write-flash")])
        self.assertNotIn("read-flash", command)
        sync.assert_called_once_with("test-port")


if __name__ == "__main__":
    unittest.main()
