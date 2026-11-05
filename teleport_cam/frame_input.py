"""Thread-safe latest-frame input for external video hosts such as OBS.

The desktop application keeps using OpenCV capture. External hosts can push
frames here without asking the Python engine to open a second camera.
"""
from __future__ import annotations

from threading import Condition

import numpy as np

from .scene import Frame


class LatestFrameInput:
    """A bounded, drop-oldest mailbox: processing always favors the newest frame."""

    def __init__(self) -> None:
        self._condition = Condition()
        self._frame: Frame | None = None
        self._closed = False

    def submit(self, frame: Frame) -> None:
        with self._condition:
            if self._closed:
                raise RuntimeError("Entrada de vídeo encerrada.")
        if not isinstance(frame, np.ndarray) or frame.dtype != np.uint8:
            raise ValueError("O quadro deve ser um array NumPy uint8.")
        if frame.ndim != 3 or frame.shape[2] != 3 or min(frame.shape[:2]) < 2:
            raise ValueError("O quadro deve usar o formato BGR de três canais.")
        owned = np.ascontiguousarray(frame).copy()
        with self._condition:
            # close() may have run while the caller's image was being copied.
            if self._closed:
                raise RuntimeError("Entrada de vídeo encerrada.")
            self._frame = owned
            self._condition.notify()

    def read(self, timeout: float = 0.25) -> tuple[bool, Frame | None]:
        with self._condition:
            self._condition.wait_for(
                lambda: self._closed or self._frame is not None,
                timeout=max(0.0, timeout),
            )
            if self._closed or self._frame is None:
                return False, None
            frame, self._frame = self._frame, None
            return True, frame

    def close(self) -> None:
        with self._condition:
            self._closed = True
            self._frame = None
            self._condition.notify_all()
