"""Explicitly started, loopback-only authenticated OBS echo worker (no effects)."""
from __future__ import annotations

import argparse
import hmac
import secrets
import socket
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from .obs_ipc import (IO_TIMEOUT, REQUEST, RESPONSE, TOKEN_BYTES,
                      receive_frame, recv_exact, send_frame)

MAX_CLIENTS = 4


def serve_connection(connection: socket.socket, token: bytes,
                     stop: threading.Event | None = None) -> int:
    """Authenticate before accepting any image; one request/response at a time."""
    supplied = recv_exact(connection, TOKEN_BYTES, time.monotonic() + IO_TIMEOUT)
    if not hmac.compare_digest(supplied, token):
        raise PermissionError('Chave IPC inválida.')
    connection.settimeout(IO_TIMEOUT)
    connection.sendall(b'OKAY')
    last_sequence = count = 0
    while stop is None or not stop.is_set():
        frame = receive_frame(connection, REQUEST, time.monotonic() + IO_TIMEOUT)
        if frame.sequence <= last_sequence:
            raise ValueError('Sequência OBS repetida ou fora de ordem.')
        send_frame(connection, frame, RESPONSE, time.monotonic() + IO_TIMEOUT)
        last_sequence = frame.sequence
        count += 1
        if count == 1:
            print(f'IPC_OK: quadro {frame.width}x{frame.height} devolvido sem efeitos.', flush=True)
    return count


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=0,
                        help='Porta local; 0 escolhe uma porta disponível.')
    args = parser.parse_args(argv)
    if not 0 <= args.port <= 65535:
        parser.error('A porta deve estar entre 0 e 65535.')
    token = secrets.token_hex(TOKEN_BYTES // 2).encode('ascii')
    stop = threading.Event()
    slots = threading.BoundedSemaphore(MAX_CLIENTS)

    def client(connection: socket.socket) -> None:
        try:
            with connection:
                connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                serve_connection(connection, token, stop)
        except (OSError, EOFError, ValueError):
            # Invalid/disconnected peers cannot kill the service or hold a slot.
            pass
        finally:
            slots.release()

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            if sys.platform == 'win32':
                server.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            server.bind(('127.0.0.1', args.port))
            server.listen(MAX_CLIENTS)
            server.settimeout(0.25)
            print('Fora de Cena — IPC experimental, retorno sem efeitos', flush=True)
            print(f'Porta: {server.getsockname()[1]}', flush=True)
            print(f'Chave: {token.decode()}', flush=True)
            print('Use esses valores no filtro Fora de Cena IPC (experimental).', flush=True)
            print('Mantenha esta janela aberta. Ctrl+C encerra o worker.', flush=True)
            with ThreadPoolExecutor(max_workers=MAX_CLIENTS, thread_name_prefix='obs-ipc') as executor:
                try:
                    while not stop.is_set():
                        try:
                            connection, _ = server.accept()
                        except socket.timeout:
                            continue
                        if not slots.acquire(blocking=False):
                            connection.close()
                            continue
                        try:
                            executor.submit(client, connection)
                        except Exception:
                            connection.close()
                            slots.release()
                            raise
                except KeyboardInterrupt:
                    pass
                finally:
                    stop.set()
    except OSError as error:
        print(f'Não foi possível iniciar o worker IPC: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
