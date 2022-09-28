@echo off

if %1 == Debug (
        echo %1
        set COMPILER_FLAGS= -FC -GR- -EHa- -EHsc -EHs -nologo -Zi -Zf -MP -std:c++latest /MTd
    ) ELSE IF %1 == Release (
        echo %1
        set COMPILER_FLAGS= -FC -GR- -EHa- -EHsc -EHs -nologo  -MP -std:c++latest /MT /O2 -Zi -Zf
    )
set VENDOR=../vendor

set ASSIMP=/I %VENDOR%/Assimp/include
set GLEW=/I %VENDOR%/GLEW/include
set GLM=/I %VENDOR%/GLM
set SDL2=/I %VENDOR%/SDL2/include
set SINGLE_HEADER=/I %VENDOR%/SingleHeaderLibs

set INC=/I../code/
set INCLUDES=%INC% %SDL2% %GLEW% %GLM% %SINGLE_HEADER%



set LIBPATH=/LIBPATH:%VENDOR%

set SDL2=%LIBPATH%/SDL2/lib/
set GLEW=%LIBPATH%/GLEW/lib/Release/x64
set ASSIMP=%LIBPATH%/Assimp/lib/x64
set LIB_PATHS=%SDL2% %GLEW%



set LINKED_LIBS= shell32.lib OpenGL32.lib glew32.lib SDL2main.lib SDL2.lib
set LINKER_FLAGS=/link %LIB_PATHS% %LINKED_LIBS% /SUBSYSTEM:WINDOWS /time

if not exist build (mkdir build)
pushd build

cl %COMPILER_FLAGS% /Fe:prog.exe ../code/*.cpp %INCLUDES% %LINKER_FLAGS%

popd