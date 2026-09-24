#pragma once

/**
 * @file OutputWriter.h
 * @brief 결과 이미지를 안전하게 저장한다.
 */

#include "ImageBuffer.h"

#include <string>

namespace ip {

/**
 * @brief ImageBuffer 를 BMP 로 저장하되, 실패해도 기존 파일이 손상되거나 불완전한 파일이 남지 않게 한다.
 *
 *  1. 출력 폴더가 없으면 만든다.
 *  2. 같은 폴더의 임시 파일(<path>.tmp)에 먼저 쓴다 (BmpParser::saveToFile).
 *  3. 쓰기가 끝난 뒤에만 임시 파일로 대상 파일을 교체한다 (같은 볼륨 내 rename).
 *  실패하면 임시 파일을 지우고 예외를 던진다. 입력과 출력 경로가 같아도 안전하다.
 *
 * @throws BmpParseError 폴더 생성, 쓰기, 교체 중 하나라도 실패한 경우.
 */
void saveBmpAtomically(const std::string& path, const ImageBuffer& image);

} // namespace ip
