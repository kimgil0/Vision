#pragma once

/**
 * @file DemoMode.h
 * @brief exe 를 탐색기에서 더블클릭했을 때 샘플 이미지로 필터 데모를 실행한다.
 *
 * 이 프로그램은 명령줄 도구라서 인자 없이 실행하면 할 일이 없다.
 *  - 더블클릭 (인자 없음 + 이 프로세스 전용 콘솔 창)  → 데모 실행 후 결과 폴더를 열고 키 입력 대기
 *  - `--demo`                                          → 데모만 실행 (명령 프롬프트용)
 *  - 명령 프롬프트에서 인자 없이 실행                    → 기존대로 사용법 출력 (스크립트/CI 동작 불변)
 */

namespace ip {
namespace demo {

/// 명령줄 한 번 실행에 해당하는 함수 (main 의 본체). 종료 코드를 반환한다.
using CliEntry = int (*)(int argc, char* argv[]);

/// 탐색기 더블클릭으로 실행되어, 이 프로세스만을 위한 콘솔 창이 새로 만들어졌는지.
bool isLaunchedByDoubleClick();

/**
 * @brief Resource 폴더의 샘플 이미지에 데모 시나리오를 차례로 적용해 Output\demo 에 저장한다.
 * @param runCli      각 시나리오를 실행할 명령줄 진입점. 일반 실행과 완전히 같은 경로를 탄다.
 * @param interactive true 면 끝난 뒤 결과 폴더를 열고 키 입력을 기다린다 (더블클릭 실행용).
 * @return 0 = 데모 완료, 2 = Resource 폴더를 찾지 못했거나 출력 폴더를 만들 수 없음.
 */
int run(CliEntry runCli, bool interactive);

} // namespace demo
} // namespace ip
