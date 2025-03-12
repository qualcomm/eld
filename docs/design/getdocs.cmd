::
:: Script to copy Linux docs to Windows
::
::      getdocs.cmd [PREFIX]
::
:: Docs will be copied to %PREFIX%\docs
::
setlocal EnableDelayedExpansion
set RVAL=

if not defined PREFIX (
    set PREFIX=.
)
IF NOT "%~1" == "" (
    set PREFIX=%1
)

echo PREFIX=%PREFIX%
set PREFIX=!PREFIX:/=\!
echo PREFIX=%PREFIX%

set SRC=\\western\qdsp6_sw_release\internal\HALIDE\branch-2.3\linux64\latest\Halide\docs
xcopy %SRC% %PREFIX%\docs\ /s /q /y
@if %errorlevel% neq 0 ( set RVAL=1 )

@if defined RVAL (
    echo Error: non-zero exit status from step %RVAL%
    exit /B 1
) else (
    echo done
)
