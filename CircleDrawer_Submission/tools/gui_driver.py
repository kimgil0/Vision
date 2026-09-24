"""
MFC 대화상자를 사람 대신 조작하는 테스트 도구 (Win32 ctypes).

- 실제 마우스·키보드를 움직이지 않는다: 창에 WM_LBUTTONDOWN / WM_MOUSEMOVE / WM_COMMAND 메시지를 직접 보낸다.
- 창을 화면 밖으로 옮겨 사용자의 작업을 방해하지 않고, PrintWindow(PW_RENDERFULLCONTENT)로 창 내용을 캡처한다.
- SendMessageTimeout 으로 보내므로 UI 스레드가 멈추면(프리징) 시간 초과로 감지된다.
"""

from __future__ import annotations

import ctypes
import ctypes.wintypes as wt
import subprocess
import time

from PIL import Image

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32")
user32.SetProcessDPIAware()

WM_NULL, WM_SETTEXT, WM_GETTEXT, WM_CLOSE = 0x0000, 0x000C, 0x000D, 0x0010
WM_COMMAND, WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP = 0x0111, 0x0200, 0x0201, 0x0202
MK_LBUTTON, BN_CLICKED, SMTO_ABORTIFHUNG = 0x0001, 0, 0x0002
SWP_NOSIZE, SWP_NOZORDER, SWP_NOACTIVATE = 0x0001, 0x0004, 0x0010
HWND_BOTTOM = 1
PW_RENDERFULLCONTENT = 0x00000002
SRCCOPY = 0x00CC0020

user32.SendMessageTimeoutW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM, wt.UINT, wt.UINT,
                                       ctypes.POINTER(ctypes.c_size_t)]
user32.SendMessageTimeoutW.restype = wt.LPARAM
user32.PostMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user32.GetDlgItem.argtypes = [wt.HWND, ctypes.c_int]
user32.GetDlgItem.restype = wt.HWND
user32.PrintWindow.argtypes = [wt.HWND, wt.HDC, wt.UINT]
gdi32.SelectObject.argtypes = [wt.HDC, wt.HGDIOBJ]
gdi32.SelectObject.restype = wt.HGDIOBJ
gdi32.CreateCompatibleDC.argtypes = [wt.HDC]
gdi32.CreateCompatibleDC.restype = wt.HDC
gdi32.CreateCompatibleBitmap.argtypes = [wt.HDC, ctypes.c_int, ctypes.c_int]
gdi32.CreateCompatibleBitmap.restype = wt.HBITMAP
gdi32.DeleteObject.argtypes = [wt.HGDIOBJ]
gdi32.DeleteDC.argtypes = [wt.HDC]
gdi32.GetDIBits.argtypes = [wt.HDC, wt.HBITMAP, wt.UINT, wt.UINT, ctypes.c_void_p, ctypes.c_void_p, wt.UINT]
gdi32.BitBlt.argtypes = [wt.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wt.HDC, ctypes.c_int,
                         ctypes.c_int, wt.DWORD]
user32.GetDC.argtypes = [wt.HWND]
user32.GetDC.restype = wt.HDC
user32.ReleaseDC.argtypes = [wt.HWND, wt.HDC]
user32.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.ClientToScreen.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]
user32.MapWindowPoints.argtypes = [wt.HWND, wt.HWND, ctypes.c_void_p, wt.UINT]
user32.SetWindowPos.argtypes = [wt.HWND, wt.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wt.UINT]
user32.IsWindowEnabled.argtypes = [wt.HWND]
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user32.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user32.IsWindowVisible.argtypes = [wt.HWND]

WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG), ("biPlanes", wt.WORD),
                ("biBitCount", wt.WORD), ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD),
                ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD),
                ("biClrImportant", wt.DWORD)]


def makelparam(x: int, y: int) -> int:
    return ((y & 0xFFFF) << 16) | (x & 0xFFFF)


class App:
    """실행 파일 하나를 띄워 조작한다. with 문으로 쓰면 끝날 때 창을 닫는다."""

    def __init__(self, exe: str, behind_other_windows: bool = True):
        self.proc = subprocess.Popen([exe])
        self.hwnd = self._find_dialog()
        if behind_other_windows:
            # 화면 밖으로 옮기면 창이 자기 표면을 갱신하지 않아 "실제로 보이는 모습"을 캡처할 수 없다.
            # 화면 안에 두되 Z 순서 맨 아래로 보내 사용자의 다른 창 뒤에 숨기고, 포커스도 빼앗지 않는다.
            user32.SetWindowPos(self.hwnd, HWND_BOTTOM, 40, 40, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE)
        time.sleep(0.3)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # ── 창 찾기 ─────────────────────────────────────────────
    def _find_dialog(self, timeout: float = 10.0) -> int:
        deadline = time.time() + timeout
        found = []

        def callback(hwnd, _):
            pid = wt.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            cls = ctypes.create_unicode_buffer(64)
            user32.GetClassNameW(hwnd, cls, 64)
            if pid.value == self.proc.pid and cls.value == "#32770" and user32.IsWindowVisible(hwnd):
                found.append(hwnd)
            return True

        cb = WNDENUMPROC(callback)
        while time.time() < deadline:
            found.clear()
            user32.EnumWindows(cb, 0)
            if found:
                return found[0]
            time.sleep(0.1)
        raise RuntimeError("dialog window not found")

    # ── 메시지 ──────────────────────────────────────────────
    def send(self, msg: int, wparam: int = 0, lparam: int = 0, timeout_ms: int = 3000, hwnd: int | None = None) -> bool:
        """동기 전송. UI 스레드가 timeout_ms 안에 처리하지 못하면 False (프리징)."""
        result = ctypes.c_size_t()
        ok = user32.SendMessageTimeoutW(hwnd or self.hwnd, msg, wparam, lparam, SMTO_ABORTIFHUNG, timeout_ms,
                                        ctypes.byref(result))
        return bool(ok)

    def responsive(self, timeout_ms: int = 250) -> bool:
        return self.send(WM_NULL, timeout_ms=timeout_ms)

    def click(self, x: int, y: int) -> bool:
        ok = self.send(WM_LBUTTONDOWN, MK_LBUTTON, makelparam(x, y))
        return self.send(WM_LBUTTONUP, 0, makelparam(x, y)) and ok

    def mouse_down(self, x: int, y: int) -> bool:
        return self.send(WM_LBUTTONDOWN, MK_LBUTTON, makelparam(x, y))

    def mouse_move(self, x: int, y: int, pressed: bool = True) -> bool:
        return self.send(WM_MOUSEMOVE, MK_LBUTTON if pressed else 0, makelparam(x, y))

    def mouse_up(self, x: int, y: int) -> bool:
        return self.send(WM_LBUTTONUP, 0, makelparam(x, y))

    def button(self, ctrl_id: int, timeout_ms: int = 3000) -> bool:
        h = user32.GetDlgItem(self.hwnd, ctrl_id)
        return self.send(WM_COMMAND, (BN_CLICKED << 16) | ctrl_id, h or 0, timeout_ms=timeout_ms)

    def post_button(self, ctrl_id: int) -> None:
        h = user32.GetDlgItem(self.hwnd, ctrl_id)
        user32.PostMessageW(self.hwnd, WM_COMMAND, (BN_CLICKED << 16) | ctrl_id, h or 0)

    def set_text(self, ctrl_id: int, text: str) -> bool:
        h = user32.GetDlgItem(self.hwnd, ctrl_id)
        buffer = ctypes.create_unicode_buffer(text)
        return self.send(WM_SETTEXT, 0, ctypes.addressof(buffer), hwnd=h)

    def get_text(self, ctrl_id: int, timeout_ms: int = 1000) -> str | None:
        h = user32.GetDlgItem(self.hwnd, ctrl_id)
        if not h:
            return None
        buffer = ctypes.create_unicode_buffer(1024)
        result = ctypes.c_size_t()
        ok = user32.SendMessageTimeoutW(h, WM_GETTEXT, 1024, ctypes.addressof(buffer), SMTO_ABORTIFHUNG,
                                        timeout_ms, ctypes.byref(result))
        return buffer.value if ok else None

    def is_enabled(self, ctrl_id: int) -> bool:
        return bool(user32.IsWindowEnabled(user32.GetDlgItem(self.hwnd, ctrl_id)))

    def control_client_rect(self, ctrl_id: int) -> tuple[int, int, int, int]:
        """자식 컨트롤의 위치를 대화상자 클라이언트 좌표로 (left, top, right, bottom)."""
        h = user32.GetDlgItem(self.hwnd, ctrl_id)
        r = wt.RECT()
        user32.GetWindowRect(h, ctypes.byref(r))
        pts = (wt.POINT * 2)(wt.POINT(r.left, r.top), wt.POINT(r.right, r.bottom))
        user32.MapWindowPoints(None, self.hwnd, pts, 2)
        return pts[0].x, pts[0].y, pts[1].x, pts[1].y

    # ── 캡처 ────────────────────────────────────────────────
    def capture_client(self) -> Image.Image:
        """
        지금 창에 실제로 표시되어 있는 클라이언트 영역을 그대로 복사한다 (BitBlt, 다시 그리기 요청 없음).

        PrintWindow 는 창에게 새로 그리게 하므로 "화면 갱신을 빠뜨린" 버그(초기화 후 Invalidate 누락,
        드래그 중 다시 그리기 누락)를 가려 버린다. 그래서 창의 현재 표면(DWM 리디렉션 표면)을 복사한다.
        """
        cr = wt.RECT()
        user32.GetClientRect(self.hwnd, ctypes.byref(cr))
        w, h = cr.right, cr.bottom
        window_dc = user32.GetDC(self.hwnd)
        mem = gdi32.CreateCompatibleDC(window_dc)
        bmp = gdi32.CreateCompatibleBitmap(window_dc, w, h)
        old = gdi32.SelectObject(mem, bmp)
        gdi32.BitBlt(mem, 0, 0, w, h, window_dc, 0, 0, SRCCOPY)

        info = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
        buffer = ctypes.create_string_buffer(w * h * 4)
        gdi32.GetDIBits(mem, bmp, 0, h, buffer, ctypes.byref(info), 0)
        gdi32.SelectObject(mem, old)
        gdi32.DeleteObject(bmp)
        gdi32.DeleteDC(mem)
        user32.ReleaseDC(self.hwnd, window_dc)
        return Image.frombuffer("RGB", (w, h), buffer, "raw", "BGRX", 0, 1)

    def capture_client_repainted(self) -> Image.Image:
        """PrintWindow(PW_RENDERFULLCONTENT): 창에게 다시 그리게 한 뒤 캡처한다 (참고용)."""
        wr = wt.RECT()
        user32.GetWindowRect(self.hwnd, ctypes.byref(wr))
        w, h = wr.right - wr.left, wr.bottom - wr.top
        screen = user32.GetDC(None)
        mem = gdi32.CreateCompatibleDC(screen)
        bmp = gdi32.CreateCompatibleBitmap(screen, w, h)
        old = gdi32.SelectObject(mem, bmp)
        user32.PrintWindow(self.hwnd, mem, PW_RENDERFULLCONTENT)

        info = BITMAPINFOHEADER(ctypes.sizeof(BITMAPINFOHEADER), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
        buffer = ctypes.create_string_buffer(w * h * 4)
        gdi32.GetDIBits(mem, bmp, 0, h, buffer, ctypes.byref(info), 0)
        gdi32.SelectObject(mem, old)
        gdi32.DeleteObject(bmp)
        gdi32.DeleteDC(mem)
        user32.ReleaseDC(None, screen)
        image = Image.frombuffer("RGB", (w, h), buffer, "raw", "BGRX", 0, 1)

        origin = wt.POINT(0, 0)
        user32.ClientToScreen(self.hwnd, ctypes.byref(origin))
        cr = wt.RECT()
        user32.GetClientRect(self.hwnd, ctypes.byref(cr))
        left, top = origin.x - wr.left, origin.y - wr.top
        return image.crop((left, top, left + cr.right, top + cr.bottom))

    # ── 종료 ────────────────────────────────────────────────
    def close(self, timeout: float = 5.0) -> int | None:
        if self.proc.poll() is None:
            user32.PostMessageW(self.hwnd, WM_CLOSE, 0, 0)
            try:
                self.proc.wait(timeout)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        return self.proc.returncode
