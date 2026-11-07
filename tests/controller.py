"""Check the shell-settings to native-IPC gap contract without a compositor."""
import json
from pathlib import Path
import runpy
import unittest
from unittest.mock import patch

controller = runpy.run_path(str(Path(__file__).resolve().parents[1] / "native-controller"))
configure = controller["configure"]


class GapConfiguration(unittest.TestCase):
    def test_auto_and_explicit_gaps(self):
        for settings, expected in [({}, "-1"), ({"gap": None}, "-1"), ({"gap": 0}, "0"),
                                   ({"gap": 4.5}, "4.5"), ({"gap": 100}, "100")]:
            with self.subTest(settings=settings), patch.dict(configure.__globals__, run=lambda *args: self.check_command(args, expected)):
                configure(json.dumps(settings))

    def check_command(self, args, expected):
        self.assertEqual(args, ("hyprctl", "omasnap", f"configure 24 160 {expected} 1 1 59a6ff"))
        return "ok"

    def test_invalid_gaps_do_not_reach_ipc(self):
        for gap in [True, False, -1, -0.5, 101, "auto", [], {}, float("nan"), float("inf")]:
            with self.subTest(gap=gap), patch.dict(configure.__globals__, run=lambda *args: self.fail("Invalid gap reached IPC")):
                with self.assertRaises(ValueError):
                    configure(json.dumps({"gap": gap}))


if __name__ == "__main__":
    unittest.main()
