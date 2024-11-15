set(VCPKG_TARGET_ARCHITECTURE x86)

if(PORT MATCHES "discord-game-sdk")
    set(VCPKG_CRT_LINKAGE dynamic)
    set(VCPKG_LIBRARY_LINKAGE dynamic)
else()
    set(VCPKG_CRT_LINKAGE static)
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
