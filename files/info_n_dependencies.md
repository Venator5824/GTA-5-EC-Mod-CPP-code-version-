
Config type: DLL

C Code generation: MD


C: 

Includes:

ScriptHookV_SDK_1.0.617.1a\inc;

llama.cpp-master-vulkan\include;

llama.cpp-master-vulkan;

llama.cpp-master-vulkan\ggml\include;

llama.cpp-master-vulkan\common;

llama.cpp-master-vulkan\src;

whisper.cpp-master\include;

miniaudio-master;

whisper.cpp-master\src


Linker:

llama.lib

whisper.lib

ggml.lib

ggml-base.lib

ggml-cpu.lib

ScriptHookV.lib

dxgi.lib

ggml-vulkan.lib

vulkan-1.lib

LLAMA compiled flags:

VULKAN: ON

AVX 512: NO

VSC 17 2022

DLLAMA_SHARED=OFF 

DBUILD_SHARED_LIBS=OFF 

DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded // INFO: despite this flag, llama still only works on MD currently. 

Release build

Static buld


WHISPER compiled flags:

VULKAN: ON

AVX 512: NO

VSC 17 2022

DBUILD_SHARED_LIBS=OFF

Release build

Static build

