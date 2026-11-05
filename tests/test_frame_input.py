import unittest
from threading import Event, Thread
from unittest.mock import patch

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

    def test_close_wakes_every_waiting_reader(self):
        source = LatestFrameInput()
        results = []
        readers = [Thread(target=lambda: results.append(source.read(timeout=10)), daemon=True)
                   for _ in range(3)]
        for reader in readers:
            reader.start()
        source.close()
        for reader in readers:
            reader.join(timeout=1)
            self.assertFalse(reader.is_alive())
        self.assertEqual(results, [(False, None)] * 3)

    def test_close_during_frame_copy_rejects_submission(self):
        source = LatestFrameInput()
        copying, resume = Event(), Event()
        errors = []
        frame = np.zeros((4, 6, 3), dtype=np.uint8)

        class DelayedCopy:
            def copy(self):
                copying.set()
                if not resume.wait(timeout=2):
                    raise TimeoutError('copy was not resumed')
                return frame.copy()

        def submit():
            try:
                source.submit(frame)
            except Exception as error:
                errors.append(error)

        with patch('teleport_cam.frame_input.np.ascontiguousarray', return_value=DelayedCopy()):
            worker = Thread(target=submit, daemon=True)
            worker.start()
            try:
                self.assertTrue(copying.wait(timeout=1))
                source.close()
            finally:
                resume.set()
                worker.join(timeout=1)
        self.assertFalse(worker.is_alive())
        self.assertEqual(len(errors), 1)
        self.assertIsInstance(errors[0], RuntimeError)
        self.assertEqual(source.read(timeout=0), (False, None))


if __name__ == "__main__":
    unittest.main()
