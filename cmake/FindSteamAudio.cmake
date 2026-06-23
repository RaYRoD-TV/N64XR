# FindSteamAudio.cmake
# Locates the Steam Audio SDK (Valve, Apache-2.0).
#
# Honors:
#   STEAM_AUDIO_ROOT  CMake var or env var, pointing at extracted SDK root.
# Default search path: C:/SDK/steamaudio
#
# Provides:
#   SteamAudio_FOUND
#   SteamAudio_INCLUDE_DIRS
#   SteamAudio_LIBRARIES   (windows-x64 import lib + runtime dll path)
#   Imported target: SteamAudio::SteamAudio

if(NOT DEFINED STEAM_AUDIO_ROOT)
    if(DEFINED ENV{STEAM_AUDIO_ROOT})
        set(STEAM_AUDIO_ROOT "$ENV{STEAM_AUDIO_ROOT}")
    else()
        set(STEAM_AUDIO_ROOT "C:/SDK/steamaudio")
    endif()
endif()

find_path(SteamAudio_INCLUDE_DIR
    NAMES phonon.h
    PATHS "${STEAM_AUDIO_ROOT}/include"
    NO_DEFAULT_PATH
)

find_library(SteamAudio_LIBRARY
    NAMES phonon
    PATHS "${STEAM_AUDIO_ROOT}/lib/windows-x64"
    NO_DEFAULT_PATH
)

find_file(SteamAudio_RUNTIME_DLL
    NAMES phonon.dll
    PATHS "${STEAM_AUDIO_ROOT}/lib/windows-x64"
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SteamAudio
    REQUIRED_VARS SteamAudio_INCLUDE_DIR SteamAudio_LIBRARY STEAM_AUDIO_ROOT
)

if(SteamAudio_FOUND)
    set(SteamAudio_INCLUDE_DIRS "${SteamAudio_INCLUDE_DIR}")
    set(SteamAudio_LIBRARIES    "${SteamAudio_LIBRARY}")

    if(NOT TARGET SteamAudio::SteamAudio)
        add_library(SteamAudio::SteamAudio SHARED IMPORTED)
        set_target_properties(SteamAudio::SteamAudio PROPERTIES
            IMPORTED_LOCATION "${SteamAudio_RUNTIME_DLL}"
            IMPORTED_IMPLIB   "${SteamAudio_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SteamAudio_INCLUDE_DIRS}"
        )
    endif()
endif()

mark_as_advanced(SteamAudio_INCLUDE_DIR SteamAudio_LIBRARY SteamAudio_RUNTIME_DLL)
