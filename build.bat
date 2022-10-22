@echo off

if %1 == Debug (
        echo %1
        set COMPILER_FLAGS= -FC -GR- -EHa- -EHsc -EHs -nologo -Zi -Zf -MP -std:c++latest /MTd
) ELSE IF %1 == Release (
        echo %1
        set COMPILER_FLAGS= -FC -GR- -EHa- -EHsc -EHs -nologo -std:c++latest /MT /O2
)

set VENDOR=../vendor
set VULKAN_SDK=C:\VulkanSDK\1.3.224.1

set VULKAN=/I %VULKAN_SDK%/Include
set ASSIMP=/I %VENDOR%/Assimp/include
set GLEW=/I %VENDOR%/GLEW/include
set GLM=/I %VENDOR%/GLM
set SDL2=/I %VENDOR%/SDL2/include
set SINGLE_HEADER=/I %VENDOR%/SingleHeaderLibs

set INC=/I../code/
set INCLUDES=%INC% %SDL2% %GLEW% %GLM% %SINGLE_HEADER% %VULKAN%



set LIBPATH=/LIBPATH:%VENDOR%
set SDL2=%LIBPATH%/SDL2/lib/
set GLEW=%LIBPATH%/GLEW/lib/Release/x64
set ASSIMP=%LIBPATH%/Assimp/lib/x64

set VULKAN=/LIBPATH:%VULKAN_SDK%/Lib
set LIB_PATHS=%SDL2% %GLEW% %VULKAN%



set LINKED_LIBS= shell32.lib OpenGL32.lib glew32.lib SDL2main.lib SDL2.lib vulkan-1.lib
set LINKER_FLAGS=/link %LIB_PATHS% %LINKED_LIBS% /SUBSYSTEM:WINDOWS /INCREMENTAL /TIME+

if not exist build (mkdir build)
pushd build

@REM cl %COMPILER_FLAGS% %INCLUDES% /Fe:prog.exe ../code/*.cpp ../code/GL/*.cpp  %LINKER_FLAGS%
cl %COMPILER_FLAGS% %INCLUDES% /Fe:prog.exe ../code/*.cpp %LINKER_FLAGS%

popd

compileShaders