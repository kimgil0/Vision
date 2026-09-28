# ImageProcessor 과제 제출

24-bit BMP 이미지 필터 CLI — C++17, 외부 라이브러리 없음

## 구현 항목

- **필터 6종**: `grayscale`, `invert`, `threshold` (고정값 / Otsu 자동), `blur` (가우시안), `sharpen` (언샤프 마스크), `sobel` (엣지 검출)
- **[가산점] 추상 클래스(`FilterBase`) 기반 다형성**: NVI 패턴으로 공통 사전조건을 기반 클래스가 보장
- **[가산점] 필터 파이프라인 체인**: `--pipeline "grayscale, blur, threshold:128"` (Composite 패턴)
- **[가산점] 멀티쓰레드 처리**: `--threads N`. 결과는 스레드 수와 무관하게 비트 단위로 동일
- **[가산점] 로그 파일 출력**: `--log <path>`. 타임스탬프, 단계별 소요 시간, 오류 기록
- 추가
  - 단위 테스트 46개(`ImageProcessor.Tests`) + 시나리오 검증 스크립트 23건(`run_samples.ps1`)
  - 제공 코드의 결함 6건 수정 ([제공 코드 리뷰](#제공-코드-리뷰) 참고)
  - 안전한 저장: 출력 폴더 자동 생성, 임시 파일에 쓴 뒤 교체 (실패해도 기존 파일 보존)
  - exe 더블클릭 시 샘플 이미지 데모 실행

## 실행 명령어

과제 README 의 예시 명령어 3개는 **수정 없이 그대로** 동작합니다.

```powershell
# Grayscale 변환
.\x64\Release\ImageProcessor.exe --input .\Resource\1_astronaut.bmp --output .\Resource\1_astronaut_grayscale.bmp --filter grayscale
```

```powershell
# Blur 처리 + 임계값 지정 (blur 뒤에 threshold:128 단계가 추가됨)
.\x64\Release\ImageProcessor.exe --input .\Resource\2_coffee.bmp --output .\Resource\2_coffee_blur_threshold.bmp --filter blur --threshold 128
```

```powershell
# 필터 파이프라인 체인(고급)
.\x64\Release\ImageProcessor.exe --input .\Resource\3_chelsea_cat.bmp --output .\Resource\3_chelsea_cat_pipeline.bmp --pipeline "grayscale, blur, threshold:128"
```

그 밖의 필터와 옵션입니다. 출력 폴더가 없으면 자동으로 만듭니다.

```powershell
# 이진화: 고정 임계값 / Otsu 자동 임계값
.\x64\Release\ImageProcessor.exe -i .\Resource\4_text_page.bmp -o .\Output\4_text_page_128.bmp  -f threshold:128
.\x64\Release\ImageProcessor.exe -i .\Resource\4_text_page.bmp -o .\Output\4_text_page_otsu.bmp -f threshold:otsu
```

```powershell
# 색 반전 / 가우시안 블러(σ=3) / 샤프닝 / 엣지 검출
.\x64\Release\ImageProcessor.exe -i .\Resource\2_coffee.bmp       -o .\Output\2_coffee_invert.bmp       -f invert
.\x64\Release\ImageProcessor.exe -i .\Resource\2_coffee.bmp       -o .\Output\2_coffee_blur.bmp         -f blur:3
.\x64\Release\ImageProcessor.exe -i .\Resource\1_astronaut.bmp    -o .\Output\1_astronaut_sharpen.bmp   -f sharpen:1.5
.\x64\Release\ImageProcessor.exe -i .\Resource\5_checkerboard.bmp -o .\Output\5_checkerboard_sobel.bmp  -f sobel
```

```powershell
# 파이프라인 + 스레드 수 + 로그 파일
.\x64\Release\ImageProcessor.exe -i .\Resource\3_chelsea_cat.bmp -o .\Output\3_chelsea_cat_edges.bmp `
    --pipeline "grayscale, blur:1.5, sobel" --threads 8 --log .\Output\run.log
```

```powershell
# 지원 필터 목록 / 도움말
.\x64\Release\ImageProcessor.exe --list-filters
.\x64\Release\ImageProcessor.exe --help
```

### 결과를 빠르게 확인하려면

- **exe 더블클릭**: 탐색기에서 `x64\Release\ImageProcessor.exe`를 더블클릭하면 샘플 이미지에 12개 시나리오를 적용해 `Output\demo`에 저장하고, 결과 폴더를 연 뒤 키 입력을 기다립니다. 명령 프롬프트에서 인자 없이 실행하면 기존대로 사용법을 출력합니다. 명령 프롬프트에서 같은 데모를 돌리려면 `--demo`를 붙입니다.
- **자동 검증**: `powershell -ExecutionPolicy Bypass -File .\run_samples.ps1`은 정상 11건과 오류 11건의 종료 코드, 결과 파일 유무, 임시 파일 잔존 여부를 검사합니다. (`-ExecutionPolicy Bypass`는 이 스크립트 실행에만 적용되며, 서명되지 않은 스크립트를 막는 Windows 기본 정책 때문에 필요합니다.)

### 필터 스펙

| 스펙 | 동작 | 파라미터 |
|---|---|---|
| `grayscale` | BT.601 휘도 흑백 변환 | 없음 |
| `invert` | 채널 반전 `255 − v` | 없음 |
| `threshold[:N \| :otsu]` | 휘도가 N 보다 크면 255, 아니면 0 | 0–255. 생략하면 Otsu 자동 |
| `blur[:sigma]` | 분리형 가우시안 블러 | (0, 50], 기본 1 |
| `sharpen[:amount]` | 언샤프 마스크 | (0, 10], 기본 1 |
| `sobel` | Sobel 그래디언트 크기 | 없음 |

- 이름은 대소문자를 구분하지 않고, 토큰 앞뒤 공백은 무시합니다.
- 파라미터는 **문자열 전체가 숫자**여야 합니다 (`128abc`는 거부).
- 파이프라인은 `"a, b:1, c"`처럼 쉼표로 구분합니다. 빈 항목(`a,,b`)은 오류입니다.
- `--threshold <N|otsu>`는 `--filter` 또는 `--pipeline` 뒤에 이진화 단계를 하나 더 붙입니다.

### 옵션

| 옵션 | 설명 |
|---|---|
| `-i, --input <path>` | 입력 BMP (24비트, 무압축) |
| `-o, --output <path>` | 출력 BMP. 폴더가 없으면 생성 |
| `-f, --filter <spec>` | 단일 필터 |
| `-p, --pipeline <list>` | 필터 체인 (`--filter`와 함께 쓸 수 없음) |
| `--threshold <N\|otsu>` | 필터 적용 후 이진화 단계 추가 |
| `-t, --threads <n>` | 작업 스레드 수. 0 = 자동(기본), 최대 256 |
| `-l, --log <path>` | 로그를 파일에 추가 기록. 폴더가 없으면 생성 |
| `--list-filters`, `-h, --help`, `--demo` | 필터 목록, 도움말, 데모 실행 |

### 종료 코드

| 코드 | 의미 | 예 |
|---|---|---|
| 0 | 성공 | |
| 1 | 예기치 못한 오류 | 메모리 부족 |
| 2 | BMP 입출력 오류 | 파일 없음, 손상, 지원하지 않는 포맷, 출력 경로가 폴더 |
| 3 | 필터 오류 | 알 수 없는 이름, 잘못된 파라미터 |
| 4 | 인자 오류 | 필수 옵션 누락, 중복 옵션, `--filter`와 `--pipeline` 동시 지정 |

어느 단계에서 실패하든 출력 파일을 만들지 않고, 기존 출력 파일도 손상시키지 않습니다.

## 빌드 · 테스트

- Visual Studio 2022 또는 2026 에서 `ImageProcessor.sln`을 열고 `Release | x64`로 빌드합니다 (`/W4` 경고 0개).
  - 프로젝트는 설치된 Visual Studio 의 기본 C++ 도구 집합을 사용합니다 (`$(DefaultPlatformToolset)`). 그래서 버전을 바꾸는 재대상 지정 없이 빌드됩니다.
  - 압축을 아주 깊은 폴더(경로 약 150자 이상)에 풀면 Windows 경로 길이 제한(260자) 때문에 테스트 프로젝트 빌드가 실패할 수 있습니다. 바탕화면처럼 짧은 경로를 권장합니다.
- 단위 테스트: `.\x64\Release\ImageProcessor.Tests.exe`
- 시나리오 검증: `powershell -ExecutionPolicy Bypass -File .\run_samples.ps1`

## 설계

```
main ─┬─ CommandLineParser ─→ ProgramOptions
      ├─ FilterFactory ─────→ FilterPipeline (단일 필터도 1단계 파이프라인)
      ├─ BmpParser ─────────→ ImageBuffer
      ├─ pipeline->apply(image, FilterContext{ threads, logger })
      └─ saveBmpAtomically ─→ 폴더 생성 → <path>.tmp 에 쓰기 → 교체

FilterBase (NVI: 공개 apply() 가 공통 검사 → private virtual process())
 ├─ GrayscaleFilter   ├─ ThresholdFilter      ├─ SharpenFilter ── has-a ── GaussianBlurFilter
 ├─ InvertFilter      ├─ GaussianBlurFilter   ├─ SobelFilter
 └─ FilterPipeline ── has-many ── FilterBase   (Composite)
```

| 결정 | 이유 |
|---|---|
| **NVI 패턴** | 빈 이미지, 스레드 수 같은 공통 사전조건을 `apply()` 한 곳에서 검사합니다. 새 필터가 검사를 빠뜨릴 수 없습니다. |
| **Composite 파이프라인** | 파이프라인도 `FilterBase`입니다. main 은 단일 필터와 체인을 구분하지 않습니다. |
| **카탈로그 기반 Factory** | 필터 추가 = `FilterFactory.cpp`의 표에 한 줄 추가. `--list-filters`와 오류 메시지의 사용 가능 목록이 자동으로 따라옵니다. |
| **Fail-fast** | 필터 스펙을 이미지 로딩 **전에** 검증합니다. 오타 하나 때문에 큰 파일을 다 읽은 뒤 실패하지 않습니다. |
| **불변 필터 + 실행 환경 분리** | 파라미터는 생성자에서 검증한 뒤 바꿀 수 없습니다. 스레드 수와 로거는 `FilterContext`로 넘깁니다. `apply()`가 `const`라 스레드 간 공유가 안전합니다. |
| **예외 타입 = 종료 코드** | 파라미터를 `std::from_chars`로 직접 파싱합니다. `std::stoi`가 던지는 `std::invalid_argument`(→ 종료 코드 1)가 새어나가지 않습니다. |
| **채널 상수화** | `pixel::BLUE/GREEN/RED` 상수로 BGR 순서를 표현해 RGB/BGR 혼동을 막습니다. |
| **원자적 저장** | 임시 파일에 다 쓴 뒤에만 대상 파일을 교체합니다. 쓰기 도중 실패해도 불완전한 파일이 남거나 기존 파일이 손상되지 않습니다. |

### 멀티스레드 (`Parallel.h`)

- 이미지를 행 블록으로 나누고, 각 블록은 **자기 출력 행에만** 씁니다. 락 없이도 데이터 경쟁이 없습니다.
- 이웃 픽셀을 읽는 필터(blur, sobel)는 입력과 출력 버퍼를 분리합니다. 그래서 **1 스레드와 N 스레드의 결과가 비트 단위로 같습니다** (6개 필터 모두 테스트).
- 워커 스레드의 예외는 `std::exception_ptr`로 모았다가, 모든 스레드를 join 한 뒤 호출 스레드에서 다시 던집니다. 그대로 두면 `std::terminate`입니다.
- `ThreadJoinGuard`(RAII): 스레드 생성 도중 `std::system_error`가 나도 이미 시작한 스레드는 join 됩니다.
- **작업량 기반 분할**: 스레드 하나를 추가하는 비용이 약 0.5 ms 로 측정되었습니다. 작은 작업은 나누지 않습니다.

실측 (8 논리 코어, Release x64, 5회 중 최솟값):

| 이미지 | 필터 | 1 스레드 | 8 스레드 | 배율 |
|---|---|---:|---:|---:|
| 512×512 | grayscale | 0.60 ms | 0.54 ms | 작업량이 작아 분할하지 않음 |
| 512×512 | blur:3 | 23.9 ms | 11.3 ms | 2.1× |
| 4000×3000 | grayscale | 23.7 ms | 10.3 ms | 2.3× |
| 4000×3000 | sobel | 109 ms | 34 ms | 3.2× |
| 4000×3000 | blur:3 | 1,228 ms | 398 ms | 3.1× |

### 알고리즘

| 필터 | 구현 | 선택 이유 |
|---|---|---|
| grayscale | BT.601 정수 고정소수점 `(77R + 150G + 29B + 128) >> 8` | 가중치 합이 256 이라 흰색이 정확히 255 로 보존됩니다. 부동소수점 절삭 오차가 없고 플랫폼 간 결과가 같습니다. |
| threshold | `luma > level` (OpenCV `THRESH_BINARY` 규약) + Otsu | `4_text_page`처럼 밝기 분포가 제각각인 문서를 자동으로 이진화합니다. |
| blur | 분리형 가우시안, `r = ⌈3σ⌉`, 가장자리 복제 | 연산량이 O(r²)에서 O(r)로 줄어듭니다. 커널 합이 1 이라 테두리가 어두워지지 않습니다. |
| sharpen | 언샤프 마스크 `src + a·(src − blur)` | `GaussianBlurFilter`를 합성(composition)으로 재사용합니다. |
| sobel | 휘도 평면 → 3×3 Sobel → `√(Gx² + Gy²)` 포화 | 경계를 복제하므로 이미지 테두리에 가짜 엣지가 생기지 않습니다. |

## 제공 코드 리뷰

제공 코드에서 발견한 결함은 **최소 범위로 수정하고 `[수정]` 주석으로 표시**했습니다.

| # | 위치 | 문제 | 영향 | 조치 |
|---|---|---|---|---|
| 1 | `BmpParser::loadFromFile` | 헤더의 크기를 실제 파일 크기와 대조하지 않고 먼저 할당함 | 54 바이트 파일이 10000×10000 을 주장하면 **287 MB 를 할당한 뒤에야** EOF 로 실패 (실측, 메모리 고갈 공격 가능). 수정 후 0.8 MB | 할당 전에 `bfOffBits + stride × height ≤ 파일 크기` 검증 |
| 2 | 〃 | `-biHeight` 계산 시 `biHeight == INT32_MIN` | 부호 있는 정수 오버플로 = 정의되지 않은 동작 | 사전 거부 |
| 3 | 〃 | 너무 큰 이미지에서 `ImageBuffer`가 `std::invalid_argument`를 던짐 | BMP 오류인데 종료 코드 1("Unexpected error") | `BmpParseError`로 변환 → 코드 2 |
| 4 | 〃 | `biSize`, `bfOffBits` 미검증 | OS/2 헤더를 잘못 해석하고, 헤더 바이트를 픽셀로 읽음 | 범위 검증 추가 |
| 5 | `CommandLineParser::parse` | `--help`에서 `std::exit(0)` 호출 | 스택 소멸자가 실행되지 않음. 파서가 프로세스 수명을 결정해 테스트할 수 없음 | 플래그만 세우고 main 이 종료 |
| 6 | `ImageBuffer.h` | `std::size_t`를 쓰면서 `<cstddef>`를 포함하지 않음 | 전이 include 에 의존 | include 추가 |

## 검증

### 단위 테스트 (46개)

| 영역 | 검증 내용 |
|---|---|
| BMP | 홀수 폭 패딩 왕복, bottom-up/top-down 방향. 잘린 파일, `INT32_MIN`, 잘못된 오프셋, 8비트, 압축, OS/2 헤더 거부 |
| 병렬 | 모든 행을 정확히 한 번 처리, 워커 예외 전파(terminate 없음), 작은 작업의 단일 스레드 실행 |
| 필터 | BT.601 가중치(BGR 순서), invert 두 번 = 원본, threshold 경계(`>`), Otsu 분리, blur 상수 보존·대칭·초소형 이미지, sharpen 오버슈트, sobel 테두리 |
| 계약 | **모든 필터가 1 스레드와 7 스레드에서 같은 결과**, 빈 이미지 거부 |
| 파싱 | 잘못된 스펙 17종이 모두 `FilterError`(코드 3)로 보고됨, 파이프라인 = 순차 적용, CLI 중복·누락·형식 오류, `--threshold` |
| 저장·로그 | 폴더 자동 생성, 덮어쓰기, 쓰기 실패 시 기존 파일 보존, 임시 파일 미잔존, 로그 형식 |

- 제공된 원본 `BmpParser.cpp`로 되돌려 실행하면 BMP 테스트 3개가 실패합니다. 테스트가 위 결함들을 실제로 잡는다는 뜻입니다.
- **AddressSanitizer**(`/fsanitize=address`) 빌드에서 단위 테스트 46개, 샘플 이미지 처리, 데모 실행 모두 메모리 오류가 없었습니다.

## 한계 및 개선 여지

- **blur 중간 결과를 8비트로 저장**: 최대 ±1 오차가 생깁니다. float 중간 버퍼를 쓰면 없앨 수 있습니다 (메모리 4배).
- **감마 보정 없음**: sRGB 값에 직접 가중치를 적용합니다. 업계 관행과 같지만 물리적으로 정확한 휘도는 아닙니다.
- **스레드를 호출마다 생성**: 파이프라인이 길어지면 스레드 풀로 생성 비용(약 0.5 ms/스레드)을 없앨 수 있습니다.
- **24비트 무압축 BMP 만 지원**: 제공된 파서의 범위를 따랐습니다.
