#!/usr/bin/env python3
"""
NDI 接收：独立线程持续从 NDI 拉流并丢弃中间帧，仅保留最新一帧；主线程取帧时返回副本，
避免「识别/处理慢 → 读到的是积压旧帧」的问题。

按键：q 退出。常量区可改源名称子串、超时等。
"""

from __future__ import annotations

import threading
import time

import cv2
import numpy as np
import NDIlib as ndi

__all__ = ["NdiLatestReceiver"]

# 源发现：名称包含该子串则连接；空字符串表示使用列表中的第一个源
SOURCE_NAME_SUBSTR = ""
FIND_RETRY_TIMES = 100
FIND_WAIT_MS = 100
RECV_CAPTURE_TIMEOUT_MS = 100


class NdiLatestReceiver:
    """
    后台线程内循环 recv_capture_v2：对 VIDEO 立即 copy 并 free，更新最新帧与序号；
    主线程 read() 再 copy 一份给调用方（与 dual_capture.AsyncCapture 的「取最新+副本」一致）。
    """

    def __init__(self, source_name_substr: str = SOURCE_NAME_SUBSTR) -> None:
        self._substr = source_name_substr
        self._recv = None
        self._lock = threading.Lock()
        self._frame: np.ndarray | None = None
        self._seq = 0
        self._ok = False
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._opened = False

    def open(self) -> bool:
        if not ndi.initialize():
            print("NDI initialize 失败。")
            return False

        ndi_find = ndi.find_create_v2()
        if ndi_find is None:
            print("NDI find_create_v2 失败。")
            ndi.destroy()
            return False

        sources: list = []
        target = None
        for _ in range(FIND_RETRY_TIMES):
            ndi.find_wait_for_sources(ndi_find, FIND_WAIT_MS)
            sources = ndi.find_get_current_sources(ndi_find)
            if not sources:
                continue
            if self._substr:
                for s in sources:
                    if self._substr in s.ndi_name:
                        target = s
                        break
                if target is not None:
                    break
            else:
                target = sources[0]
                break

        if target is None:
            print(
                f"未发现可用 NDI 源（子串过滤: {self._substr!r}，重试 {FIND_RETRY_TIMES} 次）。"
            )
            ndi.find_destroy(ndi_find)
            ndi.destroy()
            return False

        print(f"连接 NDI 源: {target.ndi_name}")

        ndi_recv_create = ndi.RecvCreateV3()
        ndi_recv_create.color_format = ndi.RECV_COLOR_FORMAT_BGRX_BGRA
        self._recv = ndi.recv_create_v3(ndi_recv_create)
        if self._recv is None:
            print("recv_create_v3 失败。")
            ndi.find_destroy(ndi_find)
            ndi.destroy()
            return False

        ndi.recv_connect(self._recv, target)
        ndi.find_destroy(ndi_find)

        self._stop.clear()
        self._thread = threading.Thread(target=self._worker, daemon=True)
        self._thread.start()
        self._opened = True
        return True

    def _worker(self) -> None:
        assert self._recv is not None
        while not self._stop.is_set():
            t, v, a, meta = ndi.recv_capture_v2(self._recv, RECV_CAPTURE_TIMEOUT_MS)

            if t == ndi.FRAME_TYPE_NONE:
                continue

            if t == ndi.FRAME_TYPE_VIDEO:
                frame = np.copy(v.data)
                ndi.recv_free_video_v2(self._recv, v)
                with self._lock:
                    self._frame = frame
                    self._ok = True
                    self._seq += 1
                continue

            if t == ndi.FRAME_TYPE_AUDIO:
                ndi.recv_free_audio_v2(self._recv, a)
                continue

            if t == ndi.FRAME_TYPE_METADATA:
                ndi.recv_free_metadata(self._recv, meta)
                continue

            if t == ndi.FRAME_TYPE_ERROR:
                continue

    def read(self) -> tuple[bool, np.ndarray | None, int]:
        with self._lock:
            if self._frame is None:
                return False, None, self._seq
            return self._ok, self._frame.copy(), self._seq

    def warmup(self, max_wait_s: float = 5.0, poll_s: float = 0.02) -> tuple[bool, np.ndarray | None]:
        deadline = time.perf_counter() + max_wait_s
        while time.perf_counter() < deadline:
            ok, frame, _ = self.read()
            if ok and frame is not None:
                return True, frame
            time.sleep(poll_s)
        return False, None

    def close(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=5.0)
            self._thread = None
        if self._recv is not None:
            ndi.recv_destroy(self._recv)
            self._recv = None
        if self._opened:
            ndi.destroy()
            self._opened = False

    def __enter__(self) -> NdiLatestReceiver:
        if not self.open():
            raise RuntimeError("NdiLatestReceiver.open() failed")
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()


def main() -> None:
    window = "NDI Receiver (latest frame)  q=quit"
    rx = NdiLatestReceiver(SOURCE_NAME_SUBSTR)
    if not rx.open():
        return

    ok, first = rx.warmup()
    if not ok or first is None:
        print("等待首帧超时。")
        rx.close()
        return

    prev_new_frame_t = time.perf_counter()
    last_seq: int | None = None
    ema_fps = 30.0

    print("q: 退出  |  接收线程持续拉流，主循环取到的是当前最新帧副本")

    while True:
        # 可在此插入耗时处理；接收线程仍会更新到最新帧
        # time.sleep(0.08)

        ok, frame, seq = rx.read()
        if not ok or frame is None:
            time.sleep(0.01)
            continue

        if last_seq is None or seq != last_seq:
            now = time.perf_counter()
            if last_seq is not None:
                dt = now - prev_new_frame_t
                if dt > 1e-6:
                    ema_fps = ema_fps * 0.9 + (1.0 / dt) * 0.1
            prev_new_frame_t = now
            last_seq = seq

        cv2.putText(
            frame,
            f"latest seq={seq}  emaFPS~{ema_fps:.1f}",
            (12, 36),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.9,
            (0, 255, 0),
            2,
            cv2.LINE_AA,
        )
        cv2.imshow(window, frame)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    rx.close()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
