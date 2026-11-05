"""Bounded native-frame protocol for the experimental OBS echo worker.

No camera, inference, GUI or virtual-output imports. Pixel planes are packed
without padding; v1 returns the source bytes exactly, before effects integration.
"""
from __future__ import annotations

from dataclasses import dataclass
import socket
import struct
import time

MAGIC = b'FDC1'
VERSION = 1
REQUEST, RESPONSE = 1, 2
# magic, version, kind, format, width, height, sequence, source timestamp, bytes
HEADER = struct.Struct('!4sBBHIIQQI')
MAX_WIDTH, MAX_HEIGHT = 3840, 2160
MAX_PAYLOAD = MAX_WIDTH * MAX_HEIGHT * 4
TOKEN_BYTES = 64
IO_TIMEOUT = 1.0


def payload_size(pixel_format: int, width: int, height: int) -> int:
    if not (2 <= width <= MAX_WIDTH and 2 <= height <= MAX_HEIGHT):
        raise ValueError('Dimensões fora do limite do protocolo OBS.')
    # 1=BGRA, 2=BGRX, 3=RGBA, 4=I420, 5=NV12, 6=YUY2, 7=UYVY, 8=YVYU.
    if pixel_format in (1, 2, 3):
        return width * height * 4
    if pixel_format in (4, 5) and width % 2 == 0 and height % 2 == 0:
        return width * height * 3 // 2
    if pixel_format in (6, 7, 8) and width % 2 == 0:
        return width * height * 2
    raise ValueError('Formato de vídeo não suportado pelo protocolo OBS.')


def recv_exact(connection: socket.socket, size: int, deadline: float) -> bytes:
    if not 0 <= size <= MAX_PAYLOAD:
        raise ValueError('Tamanho de mensagem fora do limite.')
    data = bytearray(size)
    view = memoryview(data)
    offset = 0
    while offset < size:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError('Prazo da mensagem OBS excedido.')
        connection.settimeout(remaining)
        count = connection.recv_into(view[offset:])
        if not count:
            raise EOFError('Conexão OBS encerrada durante a mensagem.')
        offset += count
    return bytes(data)


@dataclass(frozen=True)
class NativeFrame:
    pixel_format: int
    width: int
    height: int
    sequence: int
    timestamp: int
    pixels: bytes

    def header(self, kind: int) -> bytes:
        if kind not in (REQUEST, RESPONSE):
            raise ValueError('Tipo de mensagem inválido.')
        if len(self.pixels) != payload_size(self.pixel_format, self.width, self.height):
            raise ValueError('Carga de vídeo com tamanho incorreto.')
        if not 0 < self.sequence < 2**64 or not 0 <= self.timestamp < 2**64:
            raise ValueError('Sequência ou timestamp inválido.')
        return HEADER.pack(MAGIC, VERSION, kind, self.pixel_format, self.width,
                           self.height, self.sequence, self.timestamp, len(self.pixels))


def receive_frame(connection: socket.socket, kind: int, deadline: float) -> NativeFrame:
    raw = recv_exact(connection, HEADER.size, deadline)
    magic, version, message_kind, fmt, width, height, sequence, timestamp, size = HEADER.unpack(raw)
    if magic != MAGIC or version != VERSION or message_kind != kind or sequence == 0:
        raise ValueError('Cabeçalho OBS inválido.')
    if size != payload_size(fmt, width, height):
        raise ValueError('Carga de vídeo com tamanho incorreto.')
    return NativeFrame(fmt, width, height, sequence, timestamp,
                       recv_exact(connection, size, deadline))


def send_frame(connection: socket.socket, frame: NativeFrame, kind: int, deadline: float) -> None:
    header = frame.header(kind)
    for part in (header, frame.pixels):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError('Prazo da mensagem OBS excedido.')
        connection.settimeout(remaining)
        connection.sendall(part)
