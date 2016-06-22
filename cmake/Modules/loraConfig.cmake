INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_LORA lora)

FIND_PATH(
    LORA_INCLUDE_DIRS
    NAMES lora/api.h
    HINTS $ENV{LORA_DIR}/include
        ${PC_LORA_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    LORA_LIBRARIES
    NAMES gnuradio-lora
    HINTS $ENV{LORA_DIR}/lib
        ${PC_LORA_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LORA DEFAULT_MSG LORA_LIBRARIES LORA_INCLUDE_DIRS)
MARK_AS_ADVANCED(LORA_LIBRARIES LORA_INCLUDE_DIRS)

