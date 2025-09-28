@echo off
@setlocal enableextensions
rem @cd /d "%~dp0\..\"

rem VS 2026

if exist "%ProgramFiles%\Microsoft Visual Studio\2026\Community\VC\Auxiliary\Build\vcvarsall.bat" (
	call "%ProgramFiles%\Microsoft Visual Studio\2026\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64_arm64
	goto start
)

if exist "%ProgramFiles%\Microsoft Visual Studio\2026\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
	call "%ProgramFiles%\Microsoft Visual Studio\2026\Professional\VC\Auxiliary\Build\vcvarsall.bat" amd64_arm64
	goto start
)

rem VS 2022

if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
	call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64_arm64
	goto start
)

if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
	call "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" amd64_arm64
	goto start
)

echo VS was not found...

goto end

:start

msbuild memreduct.sln -property:Configuration=Release -property:Platform=x86 -verbosity:normal
if %ERRORLEVEL% neq 0 goto end

msbuild memreduct.sln -property:Configuration=Release -property:Platform=x64 -verbosity:normal
if %ERRORLEVEL% neq 0 goto end

msbuild memreduct.sln -property:Configuration=Release -property:Platform=ARM64 -verbosity:normal
if %ERRORLEVEL% neq 0 goto end

:end

echo done...

pause
