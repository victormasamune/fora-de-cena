"""Thread-safe latest-frame input for external video hosts such as OBS.

The desktop application keeps using OpenCV capture. External hosts can push
frames here without asking the Python engine to open a second camera.
"""
from __future__ import annotations

from queue import Empty, Full, Queue
from threading import Event

import numpy as np

from .scene import Frame


class LatestFrameInput:
    """A bounded, drop-oldest mailbox: processing always favors the newest frame."""

    def __init__(self) -> None:
        self._frames: Queue[Frame | None] = Queue(maxsize=1)
        self._closed = Event()

    def submit(self, frame: Frame) -> None:
        if self._closed.is_set():
            raise RuntimeError("Entrada de vídeo encerrada.")
        if not isinstance(frame, np.ndarray) or frame.dtype != np.uint8:
            raise ValueError("O quadro deve ser um array NumPy uint8.")
        if frame.ndim != 3 or frame.shape[2] != 3 or min(frame.shape[:2]) < 2:
            raise ValueError("O quadro deve usar o formato BGR de três canais.")
        owned = np.ascontiguousarray(frame).copy()
        try:
            self._frames.put_nowait(owned)
        except Full:
            try:
                self._frames.get_nowait()
            except Empty:
                pass
            try:
                self._frames.put_nowait(owned)
            except Full:
                # A concurrent submit won the slot; its frame is newer.
                pass

    def read(self, timeout: float = 0.25) -> tuple[bool, Frame | None]:
        if self._closed.is_set():
            return False, None
        try:
            frame = self._frames.get(timeout=max(0.0, timeout))
        except Empty:
            return False, None
        if frame is None:
            return False, None
        return True, frame

    def close(self) -> None:
        if self._closed.is_set():
            return
        self._closed.set()
        try:
            self._frames.put_nowait(None)
        except Full:
            try:
                self._frames.get_nowait()
            except Empty:
                pass
            try:
                self._frames.put_nowait(None)
            except Full:
                pass
