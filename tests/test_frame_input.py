import unittest

import numpy as np

from teleport_cam.frame_input import LatestFrameInput


class LatestFrameInputTests(unittest.TestCase):
    def test_read_returns_the_newest_frame_without_backlog(self):
        source = LatestFrameInput()
        first = np.full((8, 12, 3), 10, dtype=np.uint8)
        newest = np.full((8, 12, 3), 20, dtype=np.uint8)

        source.submit(first)
        source.submit(newest)
        ok, frame = source.read(timeout=0.01)

        self.assertTrue(ok)
        np.testing.assert_array_equal(frame, newest)
        self.assertEqual(source.read(timeout=0.01), (False, None))

    def test_submit_owns_a_copy_of_the_frame(self):
        source = LatestFrameInput()
        original = np.full((4, 6, 3), 30, dtype=np.uint8)

        source.submit(original)
        original[:] = 99
        ok, frame = source.read(timeout=0.01)

        self.assertTrue(ok)
        self.assertTrue(np.all(frame == 30))

    def test_rejects_invalid_frame_formats(self):
        source = LatestFrameInput()
        with self.assertRaises(ValueError):
            source.submit(np.zeros((4, 6), dtype=np.uint8))
        with self.assertRaises(ValueError):
            source.submit(np.zeros((4, 6, 3), dtype=np.float32))
        with self.assertRaises(ValueError):
            source.submit(np.zeros((4, 6, 4), dtype=np.uint8))

    def test_close_rejects_future_frames_and_unblocks_reads(self):
        source = LatestFrameInput()
        source.close()

        self.assertEqual(source.read(timeout=0.01), (False, None))
        with self.assertRaises(RuntimeError):
            source.submit(np.zeros((4, 6, 3), dtype=np.uint8))


if __name__ == "__main__":
    unittest.main()
