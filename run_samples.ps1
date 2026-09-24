<#
.SYNOPSIS
    Runs every sample image through the filters and verifies the exit code of each error scenario.
.DESCRIPTION
    - Outputs are written to .\Output (created automatically), log to .\Output\run.log
    - Error scenarios must return the documented exit code AND must not leave an output or .tmp file.
    - Returns exit code 1 if any case fails (usable in CI).
.EXAMPLE
    .\run_samples.ps1
#>
param(
    [string]$Exe = (Join-Path $PSScriptRoot 'x64\Release\ImageProcessor.exe')
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Exe)) { throw "Executable not found: $Exe  (build Release|x64 first)" }

$res = Join-Path $PSScriptRoot 'Resource'
$out = Join-Path $PSScriptRoot 'Output'
$log = Join-Path $out 'run.log'
$errOut = Join-Path $out 'must_not_exist.bmp'
Remove-Item $errOut, "$errOut.tmp" -ErrorAction SilentlyContinue

$cases = @(
    # ---- README template examples ----
    @{ Name = 'README: grayscale';                 Expect = 0; Args = @('--input', "$res\1_astronaut.bmp", '--output', "$out\1_astronaut_grayscale.bmp", '--filter', 'grayscale') }
    @{ Name = 'README: blur + --threshold 128';    Expect = 0; Args = @('--input', "$res\2_coffee.bmp", '--output', "$out\2_coffee_blur_threshold.bmp", '--filter', 'blur', '--threshold', '128') }
    @{ Name = 'README: pipeline (width 451)';      Expect = 0; Args = @('--input', "$res\3_chelsea_cat.bmp", '--output', "$out\3_chelsea_cat_pipeline.bmp", '--pipeline', 'grayscale, blur, threshold:128') }

    # ---- every filter ----
    @{ Name = 'invert';                            Expect = 0; Args = @('-i', "$res\2_coffee.bmp",       '-o', "$out\2_coffee_invert.bmp",        '-f', 'invert') }
    @{ Name = 'blur:3';                            Expect = 0; Args = @('-i', "$res\2_coffee.bmp",       '-o', "$out\2_coffee_blur.bmp",          '-f', 'blur:3') }
    @{ Name = 'sharpen:1.5';                       Expect = 0; Args = @('-i', "$res\1_astronaut.bmp",    '-o', "$out\1_astronaut_sharpen.bmp",    '-f', 'sharpen:1.5') }
    @{ Name = 'sobel (width 451)';                 Expect = 0; Args = @('-i', "$res\3_chelsea_cat.bmp",  '-o', "$out\3_chelsea_cat_sobel.bmp",    '-f', 'sobel') }
    @{ Name = 'threshold:otsu';                    Expect = 0; Args = @('-i', "$res\4_text_page.bmp",    '-o', "$out\4_text_page_otsu.bmp",       '-f', 'threshold:otsu') }
    @{ Name = 'threshold:128';                     Expect = 0; Args = @('-i', "$res\4_text_page.bmp",    '-o', "$out\4_text_page_128.bmp",        '-f', 'threshold:128') }
    @{ Name = 'sobel checkerboard, 8 threads';     Expect = 0; Args = @('-i', "$res\5_checkerboard.bmp", '-o', "$out\5_checkerboard_sobel.bmp",   '-f', 'sobel', '-t', '8') }
    @{ Name = 'output into new nested folder';     Expect = 0; Args = @('-i', "$res\5_checkerboard.bmp", '-o', "$out\nested\deep\5_checkerboard_invert.bmp", '-f', 'invert') }

    # ---- error scenarios ----
    @{ Name = 'ERR unknown filter';                Expect = 3; Args = @('-i', "$res\1_astronaut.bmp", '-o', $errOut, '-f', 'emboss') }
    @{ Name = 'ERR non-numeric parameter';         Expect = 3; Args = @('-i', "$res\1_astronaut.bmp", '-o', $errOut, '-f', 'threshold:128abc') }
    @{ Name = 'ERR out-of-range parameter';        Expect = 3; Args = @('-i', "$res\1_astronaut.bmp", '-o', $errOut, '-f', 'blur:0') }
    @{ Name = 'ERR bad --threshold value';         Expect = 3; Args = @('-i', "$res\1_astronaut.bmp", '-o', $errOut, '-f', 'blur', '--threshold', 'abc') }
    @{ Name = 'ERR empty pipeline stage';          Expect = 3; Args = @('-i', "$res\1_astronaut.bmp", '-o', $errOut, '-p', 'grayscale,,blur') }
    @{ Name = 'ERR chain passed to --filter';      Expect = 3; Args = @('-i', "$res\1_astronaut.bmp", '-o', $errOut, '-f', 'grayscale,blur') }
    @{ Name = 'ERR missing input file';            Expect = 2; Args = @('-i', "$res\missing.bmp",     '-o', $errOut, '-f', 'grayscale') }
    @{ Name = 'ERR output path is a folder';       Expect = 2; Args = @('-i', "$res\1_astronaut.bmp", '-o', $out, '-f', 'grayscale') }
    @{ Name = 'ERR missing --output';              Expect = 4; Args = @('-i', "$res\1_astronaut.bmp", '-f', 'grayscale') }
    @{ Name = 'ERR --filter and --pipeline';       Expect = 4; Args = @('-i', "$res\1_astronaut.bmp", '-o', $errOut, '-f', 'invert', '-p', 'invert') }
    @{ Name = 'ERR bad --threads';                 Expect = 4; Args = @('-i', "$res\1_astronaut.bmp", '-o', $errOut, '-f', 'grayscale', '-t', '-1') }
)

# Windows PowerShell 5.1 turns native stderr into terminating errors under 'Stop'.
# Error scenarios write to stderr on purpose, so judge them by exit code only.
$ErrorActionPreference = 'Continue'

$failed = 0
foreach ($case in $cases) {
    & $Exe @($case.Args + @('-l', $log)) *> $null
    $code = $LASTEXITCODE
    $ok = ($code -eq $case.Expect)
    if ($case.Expect -ne 0 -and (Test-Path $errOut)) { $ok = $false }  # failures must not leave output
    if (-not $ok) { $failed++ }
    '{0}  {1,-34} expected {2}  got {3}' -f ($(if ($ok) { 'PASS' } else { 'FAIL' })), $case.Name, $case.Expect, $code
}

$leftovers = @(Get-ChildItem $out -Recurse -Filter '*.tmp' -ErrorAction SilentlyContinue)
if ($leftovers.Count -gt 0) {
    $failed++
    "FAIL  temporary files left behind: $($leftovers.FullName -join ', ')"
}
else {
    'PASS  no temporary (.tmp) files left behind'
}

''
'{0} / {1} checks passed.  Outputs: {2}' -f ($cases.Count + 1 - $failed), ($cases.Count + 1), $out
exit ([int]($failed -gt 0))
