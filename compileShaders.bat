@echo off

set VULKAN_SDK_Bin=C:\VulkanSDK\1.3.224.1\Bin

pushd shaders

%VULKAN_SDK_Bin%/glslc.exe standard.vert -o standard.vert.spv
%VULKAN_SDK_Bin%/glslc.exe standard.frag -o standard.frag.spv

popd

echo ----------------
echo compiled shaders