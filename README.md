# vkHexen
vkHexen is an SDL2 Vulkan barebones port, primarily for learning the API.  No shaders, no acceleration, just a minimal SDL_vulkan.h implementation (make a window and play).
vkHexen uses my NakedHexen port as a base due to its simplicity and portability.

Currently it is not functional.  It compiles and you can run it, but it will crash immediately upon intitialising the Vulkan Renderer.  I'm still learning it :)
If you wish to hack on it, the changes are done in i_sdl.c

# compilation
You will need vcpkg, VulkanSDK and VS2022
