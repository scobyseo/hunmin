param([string]$BuildDir = "build-msvc")
$vs = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
& cmd /c "`"$vs`" x64 && cmake -S . -B $BuildDir -G Ninja && cmake --build $BuildDir"