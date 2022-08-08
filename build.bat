@echo off

@REM set CF= -FC -GR- -EHa- -EHsc -EHs -nologo  -MP -std:c++latest /MT /O2
set CF= -FC -GR- -EHa- -EHsc -EHs -nologo -Zi -Zf -MP -std:c++latest /MTd

set CODE=../code/*.cpp
set INC=/I../code/

set VENDOR=../vendor
set LIBPATH=/LIBPATH:../vendor

set SDL2_INC=/I %VENDOR%/SDL2/include
set GLEW_INC=/I %VENDOR%/GLEW/include
set GLM_INC=/I %VENDOR%/GLM
set ASSIMP_INC=/I %VENDOR%/Assimp/include

set INCLUDES=%INC% %SDL2_INC% %GLEW_INC% %GLM_INC% %ASSIMP_INC%

set SDL2_LIBRARY_PATH=%LIBPATH%/SDL2/lib/
set GLEW_LIBRARY_PATH=%LIBPATH%/GLEW/lib/Release/x64
set ASSIMP_LIBRARY_PATH=%LIBPATH%/Assimp/lib/x64

set LIB_PATHS=%SDL2_LIBRARY_PATH% %GLEW_LIBRARY_PATH% %ASSIMP_LIBRARY_PATH%


set LIBS=user32.lib shell32.lib OpenGL32.lib GLu32.lib glew32.lib SDL2main.lib SDL2.lib


set LF=/link %LIB_PATHS% %LIBS% /SUBSYSTEM:WINDOWS /time+

if not exist build (mkdir build)
pushd build

cl %CF% /Fe:prog.exe %CODE% %INCLUDES% %LF%

popd