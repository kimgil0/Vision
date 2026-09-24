"""
과제 요구사항 자동 검증 — CircleDrawer.exe 를 실제로 조작하고 화면을 캡처해 판정한다.

  python tools\\requirements_check.py            # Release|x64 빌드 후 실행
  python tools\\requirements_check.py --docs     # README 용 스크린샷을 docs\\ 에 저장

- 실제 마우스·키보드는 움직이지 않는다. 창에 클릭·드래그·버튼 메시지를 직접 보낸다.
- 검사 중 프로그램 창이 다른 창들 뒤에 잠시 떴다가 닫힌다.
- "정원이 세 점을 지나는가"는 정답 외접원 둘레 360개 지점이 실제로 검게 그려졌는지로 판정한다.
- 필요: Python 3 + Pillow (pip install pillow)
"""

from __future__ import annotations

import ctypes
import ctypes.wintypes as wt
import math
import re
import shutil
import sys
import time
from pathlib import Path

from PIL import Image

from gui_driver import App, user32

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / "x64" / "Release" / "CircleDrawer.exe"
OUT = Path(__file__).resolve().parent / "output"
DOCS = ROOT / "docs"

# resource.h
IDC_CANVAS, IDC_EDIT_RADIUS, IDC_EDIT_THICKNESS = 1000, 1001, 1003
IDC_BTN_RESET, IDC_BTN_RANDOM = 1005, 1006
IDC_POINTS, IDC_CIRCLE_INFO, IDC_STATUS = (1007, 1008, 1009), 1010, 1011

POINTS = [(90, 150), (250, 70), (330, 300)]
FOURTH = (200, 420)
DRAG_FROM, DRAG_TO = (330, 300), (430, 340)
FORBIDDEN = [r"\bEllipse\s*\(", r"\bPolygon\s*\(", r"FillPolygon", r"DrawPolygon", r"Gdiplus", r"gdiplus",
             r"\bArc\s*\(", r"\bPie\s*\(", r"\bChord\s*\(", r"\bRoundRect\s*\(", r"\bSetPixel\s*\("]
WM_KEYDOWN, WM_KEYUP, VK_RETURN = 0x0100, 0x0101, 0x0D


# ── 판정 도구 ──────────────────────────────────────────────
def circumcircle(a, b, c):
    (ax, ay), (bx, by), (cx, cy) = a, b, c
    d = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by))
    if abs(d) < 1e-9:
        return None
    ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d
    uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d
    return ux, uy, math.hypot(ax - ux, ay - uy)


class Canvas:
    def __init__(self, app: App):
        self.app = app
        l, t, r, b = app.control_client_rect(IDC_CANVAS)
        self.origin, self.size = (l, t), (r - l, b - t)

    def at(self, p):
        return self.origin[0] + int(p[0]), self.origin[1] + int(p[1])

    def shot(self, name: str | None = None, settle: float = 0.25) -> Image.Image:
        time.sleep(settle)
        img = self.app.capture_client()
        if name:
            img.save(OUT / f"{name}.png")
        return img

    def gray(self, img: Image.Image) -> Image.Image:
        ox, oy = self.origin
        return img.crop((ox, oy, ox + self.size[0], oy + self.size[1])).convert("L")

    def darkest_near(self, g: Image.Image, x: float, y: float, reach: int = 2) -> int:
        w, h = g.size
        xi, yi = int(round(x)), int(round(y))
        vals = [g.getpixel((xi + dx, yi + dy)) for dx in range(-reach, reach + 1) for dy in range(-reach, reach + 1)
                if 0 <= xi + dx < w and 0 <= yi + dy < h]
        return min(vals) if vals else 255

    def ring_ratio(self, img: Image.Image, pts) -> float:
        c = circumcircle(*pts)
        if not c:
            return 0.0
        g, (cx, cy, r) = self.gray(img), c
        hits = total = 0
        for k in range(360):
            t = 2 * math.pi * k / 360
            x, y = cx + r * math.cos(t), cy + r * math.sin(t)
            if not (4 <= x < self.size[0] - 4 and 4 <= y < self.size[1] - 4):
                continue
            if min(math.dist((x, y), p) for p in pts) < 25:
                continue
            total += 1
            hits += self.darkest_near(g, x, y) < 100
        return hits / total if total else 0.0

    def dark_count(self, img: Image.Image, box=None) -> int:
        g = self.gray(img)
        if box:
            g = g.crop(box)
        return sum(1 for v in g.get_flattened_data() if v < 100)

    def dark_inside(self, img: Image.Image) -> int:
        """캔버스 테두리 1px 을 뺀 영역의 어두운 픽셀 수."""
        return self.dark_count(img, (2, 2, self.size[0] - 2, self.size[1] - 2))

    def ring_thickness(self, img: Image.Image, pts) -> int | None:
        """정원이 수평선과 만나는 지점에서 어두운 픽셀 수 = 선 두께."""
        cx, cy, r = circumcircle(*pts)
        g = self.gray(img)
        for sign in (1, -1):
            x0 = cx + sign * r
            if 20 <= x0 < self.size[0] - 20 and min(math.dist((x0, cy), p) for p in pts) > 30:
                row = int(round(cy))
                return sum(1 for x in range(int(x0) - 15, int(x0) + 16) if g.getpixel((x, row)) < 128)
        return None


def labels(app: App):
    return [app.get_text(i, timeout_ms=300) for i in IDC_POINTS]


def parse_points(texts):
    pts = []
    for t in texts:
        m = re.search(r"\((-?\d+), (-?\d+)\)", t or "")
        if m:
            pts.append((int(m.group(1)), int(m.group(2))))
    return pts


class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ThreadID", wt.DWORD),
                ("th32OwnerProcessID", wt.DWORD), ("tpBasePri", wt.LONG), ("tpDeltaPri", wt.LONG),
                ("dwFlags", wt.DWORD)]


def thread_count(pid: int) -> int:
    """프로세스의 현재 스레드 수 (Toolhelp32 스냅샷)."""
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
    snapshot = kernel32.CreateToolhelp32Snapshot(0x00000004, 0)   # TH32CS_SNAPTHREAD
    entry = THREADENTRY32()
    entry.dwSize = ctypes.sizeof(THREADENTRY32)
    count = 0
    ok = kernel32.Thread32First(snapshot, ctypes.byref(entry))
    while ok:
        count += entry.th32OwnerProcessID == pid
        ok = kernel32.Thread32Next(snapshot, ctypes.byref(entry))
    kernel32.CloseHandle(snapshot)
    return count


def watch_moves(app: App, seconds: float, baseline=None, on_tick=None) -> tuple[list[float], float]:
    """
    좌표 표시가 바뀐 시각들과, UI 가 응답하지 않은 총 시간.
    좌표 라벨 3개를 차례로 읽는 사이에 화면이 갱신되면 한 번의 이동이 두 번의 변화로 보이므로,
    0.15초 안에 연달아 관측된 변화는 하나의 이동으로 합친다.
    """
    last = baseline if baseline is not None else labels(app)
    changes, frozen = [], 0.0
    start = time.time()
    while time.time() - start < seconds:
        t0 = time.time()
        if not app.responsive(250):
            frozen += time.time() - t0
            continue
        now = labels(app)
        if now != last and all(now):
            t = time.time() - start
            if changes and t - changes[-1] < 0.15:
                changes[-1] = t
            else:
                changes.append(t)
            last = now
        if on_tick:
            on_tick(time.time() - start)
        time.sleep(0.02)
    return changes, frozen


def gap_text(changes: list[float]) -> str:
    gaps = [b - a for a, b in zip(changes, changes[1:])]
    return f"{min(gaps):.2f}~{max(gaps):.2f}s" if gaps else "-"


def regular(changes: list[float]) -> bool:
    return all(0.4 <= b - a <= 0.6 for a, b in zip(changes, changes[1:]))


def scan_forbidden() -> list[str]:
    hits = []
    for src in sorted((ROOT / "CircleDrawer").glob("*.[ch]*")):
        for n, line in enumerate(src.read_text(encoding="utf-8-sig", errors="replace").splitlines(), 1):
            code = line.split("//")[0]
            if any(re.search(p, code) for p in FORBIDDEN):
                hits.append(f"{src.name}:{n}")
    return hits


# ── 검사 ────────────────────────────────────────────────────
def run_checks(save_docs: bool) -> list[tuple[str, bool, str]]:
    results: list[tuple[str, bool, str]] = []

    def record(name, ok, detail):
        results.append((name, bool(ok), detail))
        print(f"{'PASS' if ok else 'FAIL'}  {name:<42} {detail}", flush=True)

    hits = scan_forbidden()
    record("R0  금지 API(Ellipse·Polygon·GDI+ 등) 미사용", not hits, ", ".join(hits) or "소스 검색 결과 없음")

    with App(str(EXE)) as app:
        cv = Canvas(app)

        # R1 세 번째 클릭 이후 세 점을 지나는 정원
        app.click(*cv.at(POINTS[0]))
        app.click(*cv.at(POINTS[1]))
        img = cv.shot()
        before_third = cv.ring_ratio(img, POINTS)
        app.click(*cv.at(POINTS[2]))
        img = cv.shot("1_three_points")
        ratio = cv.ring_ratio(img, POINTS)
        record("R1  세 번째 클릭 후 세 점을 지나는 정원", ratio >= 0.95 and before_third < 0.2,
               f"두 번째 클릭 후 {before_third:.0%} → 세 번째 클릭 후 둘레 일치 {ratio:.0%}")

        # R2 클릭 지점 원 반지름 입력
        box = (POINTS[1][0] - 26, POINTS[1][1] - 26, POINTS[1][0] + 27, POINTS[1][1] + 27)
        small = cv.dark_count(img, box)
        app.set_text(IDC_EDIT_RADIUS, "20")
        large = cv.dark_count(cv.shot(), box)
        app.set_text(IDC_EDIT_RADIUS, "10")
        record("R2  클릭 지점 원 반지름을 입력받음", large > small * 2.2, f"반지름 10 → 20: 점 영역 {small} → {large} px")

        # R3 중심 좌표 표시
        shown = parse_points(labels(app))
        record("R3  각 클릭 지점 원의 중심 좌표를 UI 에 표시", shown == POINTS, f"{shown}")

        # R4 네 번째 클릭부터 그리지 않음
        app.click(*cv.at(FOURTH))
        img = cv.shot()
        g = cv.gray(img)
        fourth_dark = cv.darkest_near(g, *FOURTH, reach=1) < 100
        record("R4  네 번째 클릭부터 클릭 지점 원을 그리지 않음", not fourth_dark and parse_points(labels(app)) == POINTS,
               "네 번째 클릭 위치가 비어 있고 좌표도 그대로")

        # R5 정원 내부는 비우고, 두께를 입력받음
        cx, cy, _ = circumcircle(*POINTS)
        center_white = cv.darkest_near(g, cx, cy, reach=3) > 200
        t3 = cv.ring_thickness(img, POINTS)
        app.set_text(IDC_EDIT_THICKNESS, "8")
        t8 = cv.ring_thickness(cv.shot("5_thickness_8"), POINTS)
        app.set_text(IDC_EDIT_THICKNESS, "3")
        record("R5  정원 내부 비움 + 가장자리 두께 입력", center_white and t3 is not None and abs(t3 - 3) <= 1
               and t8 is not None and abs(t8 - 8) <= 1, f"내부 비움={center_white}, 두께 3 → {t3}px, 8 → {t8}px")

        # R6 드래그하는 동안 계속 다시 그림
        app.mouse_down(*cv.at(DRAG_FROM))
        ratios, label_ok = [], True
        for k in range(1, 11):
            p = (DRAG_FROM[0] + (DRAG_TO[0] - DRAG_FROM[0]) * k // 10, DRAG_FROM[1] + (DRAG_TO[1] - DRAG_FROM[1]) * k // 10)
            app.mouse_move(*cv.at(p))
            if k in (3, 6, 9):
                img = cv.shot("2_mid_drag" if k == 6 else None, settle=0.1)
                ratios.append(cv.ring_ratio(img, [POINTS[0], POINTS[1], p]))
                label_ok &= parse_points(labels(app))[2:] == [p]
        app.mouse_up(*cv.at(DRAG_TO))
        record("R6  드래그 중(버튼 떼기 전) 계속 정원을 다시 그림", min(ratios) >= 0.95 and label_ok,
               "드래그 도중 3회 캡처 둘레 일치 " + ", ".join(f"{r:.0%}" for r in ratios))

        # R7 초기화
        app.button(IDC_BTN_RESET)
        img = cv.shot("4_reset")
        cleared = cv.dark_inside(img) == 0 and all("-" in (t or "") for t in labels(app))
        app.click(*cv.at(POINTS[0]))
        again = parse_points(labels(app))[:1] == [POINTS[0]]
        record("R7  [초기화] 후 모두 지우고 다시 입력 가능", cleared and again, f"지움={cleared}, 다시 입력={again}")
        app.button(IDC_BTN_RESET)

        # R8 랜덤 이동: 초당 2회 × 10번, 별도 스레드, UI 프리징 없음, 정원도 다시 그림
        for p in POINTS:
            app.click(*cv.at(p))
        baseline = labels(app)
        returned = app.button(IDC_BTN_RANDOM, timeout_ms=300)
        shot = {"taken": False}

        def mid_shot(elapsed):
            if not shot["taken"] and elapsed > 2.1:
                cv.shot("3_random", settle=0.0)
                shot["taken"] = True

        changes, frozen = watch_moves(app, 6.0, baseline=baseline, on_tick=mid_shot)
        final_pts = parse_points(labels(app))
        final_ratio = cv.ring_ratio(cv.shot(), final_pts) if len(final_pts) == 3 else 0.0
        ok = returned and frozen == 0 and len(changes) == 10 and regular(changes) and final_ratio >= 0.95
        record("R8  [랜덤 이동] 초당 2회 × 10번, UI 프리징 없음", ok,
               f"이동 {len(changes)}회, 간격 {gap_text(changes)}, UI 정지 {frozen:.1f}s, "
               f"마지막 정원 일치 {final_ratio:.0%}")

        # R9 랜덤 이동 중 초기화 → 즉시 멈추고 지움
        app.button(IDC_BTN_RANDOM, timeout_ms=300)
        time.sleep(1.2)
        app.button(IDC_BTN_RESET)
        cleared_now = cv.dark_inside(cv.shot()) == 0
        time.sleep(1.2)
        still_clear = cv.dark_inside(cv.shot()) == 0 and all("-" in (t or "") for t in labels(app))
        record("R9  이동 중 [초기화] → 이동 스레드 즉시 정지", cleared_now and still_clear,
               f"초기화 직후 깨끗함={cleared_now}, 1.2초 뒤에도 깨끗함={still_clear}")

        # R10 [랜덤 이동] 연타 → 이동은 여전히 10번 (두 번째 실행 거부)
        #   스레드 수는 Windows 스레드 풀 때문에 오르내릴 수 있어 참고용으로만 보고, 판정은 동작으로 한다.
        for p in POINTS:
            app.click(*cv.at(p))
        time.sleep(0.3)
        before = thread_count(app.proc.pid)
        app.button(IDC_BTN_RANDOM, timeout_ms=300)
        time.sleep(0.25)
        second_enabled = app.is_enabled(IDC_BTN_RANDOM)
        app.button(IDC_BTN_RANDOM, timeout_ms=300)
        extra = thread_count(app.proc.pid) - before
        changes, _ = watch_moves(app, 5.5)
        record("R10 [랜덤 이동] 연타해도 한 번만 실행", not second_enabled and len(changes) <= 10 and regular(changes),
               f"실행 중 버튼 비활성={not second_enabled}, 이후 이동 {len(changes)}회 (간격 {gap_text(changes)}), "
               f"스레드 +{extra}")
        app.button(IDC_BTN_RESET)

        # R11 잘못된 입력은 거부하고 계속 동작
        for p in POINTS:
            app.click(*cv.at(p))
        ref = cv.dark_count(cv.shot(), box)
        for text in ("0", "999", "240"):
            app.set_text(IDC_EDIT_RADIUS, text)
        app.set_text(IDC_EDIT_THICKNESS, "0")
        app.button(IDC_BTN_RANDOM, timeout_ms=300)
        time.sleep(1.2)
        alive = app.proc.poll() is None and app.responsive(500)
        app.button(IDC_BTN_RESET)
        for p in POINTS:
            app.click(*cv.at(p))
        unchanged = abs(cv.dark_count(cv.shot(), box) - ref) <= ref * 0.1
        record("R11 잘못된 입력(0, 999, 240) 거부, 프로그램 유지", alive and unchanged,
               f"동작 유지={alive}, 이전 반지름 유지={unchanged}")
        app.set_text(IDC_EDIT_RADIUS, "10")
        app.set_text(IDC_EDIT_THICKNESS, "3")

        # R12 Enter 키로 창이 닫히지 않음
        edit = user32.GetDlgItem(app.hwnd, IDC_EDIT_RADIUS)
        user32.PostMessageW(edit, WM_KEYDOWN, VK_RETURN, 0x001C0001)
        user32.PostMessageW(edit, WM_KEYUP, VK_RETURN, 0xC01C0001)
        time.sleep(0.8)
        record("R12 입력칸에서 Enter 를 눌러도 창이 닫히지 않음", app.proc.poll() is None, "")

        # R13 랜덤 이동 중 창 닫기 → 스레드 정리 후 정상 종료
        app.button(IDC_BTN_RANDOM, timeout_ms=300)
        time.sleep(0.6)
        t0 = time.time()
        code = app.close()
        elapsed = time.time() - t0
        # 정상 종료 = 0. 크래시는 0xC0000005 같은 NTSTATUS 값, abort()/std::terminate 는 3 이 된다.
        shown = hex(code) if code is not None and code >= 0x80000000 else code
        record("R13 이동 중 창을 닫아도 정상 종료 (스레드 join)", code == 0 and elapsed < 2.0,
               f"종료 코드 {shown}, {elapsed:.2f}s")

    if save_docs:
        DOCS.mkdir(exist_ok=True)
        for name in ("1_three_points", "2_mid_drag", "3_random"):
            shutil.copy(OUT / f"{name}.png", DOCS / f"{name}.png")
    return results


def main() -> int:
    if not EXE.exists():
        print(f"실행 파일이 없습니다: {EXE}\nVisual Studio 에서 Release | x64 로 먼저 빌드하세요.")
        return 2
    OUT.mkdir(parents=True, exist_ok=True)
    results = run_checks(save_docs="--docs" in sys.argv)
    passed = sum(ok for _, ok, _ in results)
    print(f"\n{passed} / {len(results)} 요구사항 통과.  캡처: {OUT}")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
