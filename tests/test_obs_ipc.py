"""Real loopback IPC and C++ client checks; no OBS, models or webcam required."""
from contextlib import contextmanager
import os
from pathlib import Path
import secrets
import shutil
import socket
import subprocess
from tempfile import TemporaryDirectory
import threading
import time
import unittest

from teleport_cam.obs_ipc import (HEADER, MAGIC, NativeFrame, REQUEST,
                                  RESPONSE, payload_size, receive_frame, recv_exact, send_frame)
from teleport_cam.obs_worker import serve_connection

ROOT = Path(__file__).resolve().parents[1]
TOKEN = b'a' * 64


@contextmanager
def loopback_server(handler):
    stop = threading.Event()
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(('127.0.0.1', 0))
    listener.listen(4)
    listener.settimeout(0.1)

    def serve():
        while not stop.is_set():
            try:
                connection, _ = listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            try:
                with connection:
                    handler(connection, stop)
            except (OSError, EOFError, ValueError):
                pass

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    try:
        yield listener.getsockname()[1]
    finally:
        stop.set()
        listener.close()
        worker.join(timeout=3)
        if worker.is_alive():
            raise AssertionError('IPC server thread did not stop')


class IpcProtocolTests(unittest.TestCase):
    def test_real_socket_round_trip_preserves_all_formats_and_metadata(self):
        with loopback_server(lambda connection, stop: serve_connection(connection, TOKEN, stop)) as port:
            with socket.create_connection(('127.0.0.1', port), timeout=2) as connection:
                connection.sendall(TOKEN)
                self.assertEqual(recv_exact(connection, 4, time.monotonic() + 1), b'OKAY')
                for fmt in range(1, 9):
                    with self.subTest(format=fmt):
                        size = payload_size(fmt, 64, 48)
                        frame = NativeFrame(fmt, 64, 48, fmt, 123456789, secrets.token_bytes(size))
                        send_frame(connection, frame, REQUEST, time.monotonic() + 1)
                        self.assertEqual(receive_frame(connection, RESPONSE, time.monotonic() + 1), frame)

    def test_invalid_token_never_receives_a_frame(self):
        with loopback_server(lambda connection, stop: serve_connection(connection, TOKEN, stop)) as port:
            with socket.create_connection(('127.0.0.1', port), timeout=2) as connection:
                connection.sendall(b'b' * 64)
                self.assertEqual(connection.recv(4), b'')

    def test_invalid_headers_are_rejected_before_payload_allocation(self):
        valid = [MAGIC, 1, REQUEST, 1, 64, 48, 1, 2, 64 * 48 * 4]
        for index, invalid in ((0, b'BAD!'), (1, 2), (2, RESPONSE), (3, 99),
                               (4, 100000), (5, 0), (6, 0), (8, 2**32 - 1)):
            with self.subTest(field=index):
                left, right = socket.socketpair()
                with left, right:
                    fields = valid.copy()
                    fields[index] = invalid
                    left.sendall(HEADER.pack(*fields))
                    with self.assertRaises(ValueError):
                        receive_frame(right, REQUEST, time.monotonic() + 1)

    def test_partial_reads_are_reassembled_and_eof_is_reported(self):
        frame = NativeFrame(5, 4, 2, 1, 42, bytes(range(12)))
        left, right = socket.socketpair()
        packet = frame.header(REQUEST) + frame.pixels

        def write_fragments():
            with left:
                for byte in packet:
                    left.sendall(bytes([byte]))
        worker = threading.Thread(target=write_fragments)
        worker.start()
        with right:
            self.assertEqual(receive_frame(right, REQUEST, time.monotonic() + 2), frame)
            with self.assertRaises(EOFError):
                receive_frame(right, REQUEST, time.monotonic() + 1)
        worker.join(timeout=1)

    def test_repeated_sequence_disconnects_client(self):
        with loopback_server(lambda connection, stop: serve_connection(connection, TOKEN, stop)) as port:
            with socket.create_connection(('127.0.0.1', port), timeout=2) as connection:
                connection.sendall(TOKEN)
                recv_exact(connection, 4, time.monotonic() + 1)
                frame = NativeFrame(1, 2, 2, 1, 42, b'\x00' * 16)
                send_frame(connection, frame, REQUEST, time.monotonic() + 1)
                receive_frame(connection, RESPONSE, time.monotonic() + 1)
                send_frame(connection, frame, REQUEST, time.monotonic() + 1)
                self.assertEqual(connection.recv(1), b'')

    def test_stalled_partial_message_has_total_deadline(self):
        left, right = socket.socketpair()
        with left, right:
            left.sendall(b'F')
            start = time.monotonic()
            with self.assertRaises(TimeoutError):
                receive_frame(right, REQUEST, start + 0.1)
            self.assertLess(time.monotonic() - start, 1)

    def test_invalid_dimensions_formats_and_odd_chroma_are_rejected(self):
        for fmt, width, height in ((0, 2, 2), (1, 3841, 2), (1, 2, 2161), (1, 1, 2),
                                    (4, 3, 2), (5, 2, 3), (6, 3, 2)):
            with self.subTest(format=fmt, width=width, height=height):
                with self.assertRaises(ValueError):
                    payload_size(fmt, width, height)

    def test_worker_cli_starts_without_models_or_camera_dependencies(self):
        code = """
import builtins
original = builtins.__import__
def guarded(name, *args, **kwargs):
    if name.split('.')[0] in {'cv2', 'mediapipe', 'tkinter', 'pyvirtualcam', 'numpy'}:
        raise AssertionError('unexpected dependency: ' + name)
    return original(name, *args, **kwargs)
builtins.__import__ = guarded
import teleport_cam.obs_worker
"""
        result = subprocess.run([os.sys.executable, '-c', code], cwd=ROOT, capture_output=True,
                                text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)


@unittest.skipUnless(shutil.which('g++'), 'C++ IPC integration requires g++ (Windows build uses MSVC).')
class NativeBridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = TemporaryDirectory()
        cls.addClassCleanup(cls.folder.cleanup)
        cls.client = Path(cls.folder.name) / ('ipc-client.exe' if os.name == 'nt' else 'ipc-client')
        command = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-pthread',
                   '-I', str(ROOT / 'obs-plugin/src'), str(ROOT / 'obs-plugin/src/ipc-bridge.cpp'),
                   str(ROOT / 'obs-plugin/tests/ipc-client.cpp'), '-o', str(cls.client)]
        if os.name == 'nt':
            command.append('-lws2_32')
        subprocess.run(command, capture_output=True, text=True, check=True, timeout=60)

    def run_client(self, port, token=TOKEN, mode='echo', fmt=1):
        result = subprocess.run([str(self.client), str(port), token.decode(), mode, str(fmt)],
                                capture_output=True, text=True, timeout=6)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('IPC_OK' if mode == 'echo' else 'FALLBACK_OK', result.stdout)

    def test_cpp_python_round_trips_and_padding_fidelity(self):
        with loopback_server(lambda connection, stop: serve_connection(connection, TOKEN, stop)) as port:
            for fmt in range(1, 9):
                with self.subTest(format=fmt):
                    self.run_client(port, fmt=fmt)

    def test_expired_response_cannot_replace_current_video(self):
        with loopback_server(lambda connection, stop: serve_connection(connection, TOKEN, stop)) as port:
            self.run_client(port, mode='stale')

    def test_disabling_or_resizing_discards_the_previous_reply(self):
        with loopback_server(lambda connection, stop: serve_connection(connection, TOKEN, stop)) as port:
            for mode in ('reset', 'resize'):
                with self.subTest(mode=mode):
                    self.run_client(port, mode=mode)

    def test_reconnect_after_worker_drops_the_first_frame(self):
        calls = []

        def reconnect(connection, stop):
            calls.append(1)
            if len(calls) == 1:
                recv_exact(connection, 64, time.monotonic() + 1)
                connection.sendall(b'OKAY')
                receive_frame(connection, REQUEST, time.monotonic() + 1)
                return
            serve_connection(connection, TOKEN, stop)

        with loopback_server(reconnect) as port:
            self.run_client(port)
        self.assertGreaterEqual(len(calls), 2)

    def test_wrong_authentication_keeps_client_responsive(self):
        with loopback_server(lambda connection, stop: serve_connection(connection, TOKEN, stop)) as port:
            self.run_client(port, token=b'b' * 64, mode='fallback')

    def test_unresponsive_worker_keeps_client_responsive_and_shutdown_bounded(self):
        def unresponsive(connection, stop):
            stop.wait(0.8)
        with loopback_server(unresponsive) as port:
            self.run_client(port, mode='fallback')

    def test_mismatched_response_cannot_replace_video(self):
        def mismatch(connection, stop):
            recv_exact(connection, 64, time.monotonic() + 1)
            connection.sendall(b'OKAY')
            frame = receive_frame(connection, REQUEST, time.monotonic() + 1)
            bad = NativeFrame(frame.pixel_format, frame.width, frame.height,
                              frame.sequence + 1, frame.timestamp, frame.pixels)
            send_frame(connection, bad, RESPONSE, time.monotonic() + 1)
        with loopback_server(mismatch) as port:
            self.run_client(port, mode='fallback')
