import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import do_flash


class FlashImageSelectionTest(unittest.TestCase):
    def test_uploader_uses_selected_image_even_if_another_build_replaces_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tools = root / "vendor"
            tools.mkdir()
            executable = tools / "uploader"
            executable.touch()
            (tools / "km0_km4_image2.bin").write_bytes(b"stale build")
            firmware = root / "selected.bin"
            firmware.write_bytes(b"selected build")
            port = root / "fake-port"
            port.touch()

            def uploader(command, **kwargs):
                staged = Path(command[1]) / "km0_km4_image2.bin"
                self.assertNotEqual(staged.parent, tools)
                self.assertEqual(staged.read_bytes(), b"selected build")
                firmware.write_bytes(b"another build")
                (tools / "km0_km4_image2.bin").write_bytes(b"another vendor build")
                self.assertEqual(staged.read_bytes(), b"selected build")

                class Process:
                    stdout = io.StringIO("All images are sent successfully!\n")

                    def wait(self):
                        return 0

                return Process()

            args = ["do_flash.py", "--port", str(port), "--firmware", str(firmware)]
            with patch.object(sys, "argv", args), \
                    patch.object(do_flash, "find_tool", return_value=(str(executable), str(tools))), \
                    patch.object(do_flash.subprocess, "Popen", side_effect=uploader), \
                    patch.object(do_flash.time, "sleep"), \
                    patch.object(do_flash, "reset_and_monitor") as monitor, \
                    contextlib.redirect_stdout(io.StringIO()), \
                    self.assertRaises(SystemExit) as exit_result:
                do_flash.main()
            self.assertEqual(exit_result.exception.code, 0)
            monitor.assert_called_once_with(str(port), duration=5.0)


if __name__ == "__main__":
    unittest.main()
