SET(CMAKE_SYSTEM_NAME       Linux)
SET(CMAKE_SYSTEM_PROCESSOR  x86_64)

SET(CMAKE_C_COMPILER        gcc)
SET(CMAKE_CXX_COMPILER      g++)
SET(AS                      as)
SET(AR                      ar)
SET(OBJCOPY                 objcopy)
SET(OBJDUMP                 objdump)
SET(SIZE                    size)

SET(LINUX_FLAGS             "-g -pthread -m32")

SET(CMAKE_C_FLAGS           "${LINUX_FLAGS} "                               CACHE INTERNAL "c compiler flags")
SET(CMAKE_CXX_FLAGS         "${LINUX_FLAGS} -fno-rtti -fno-exceptions"      CACHE INTERNAL "cxx compiler flags")
SET(CMAKE_ASM_FLAGS         "${LINUX_FLAGS} -x assembler-with-cpp"          CACHE INTERNAL "asm compiler flags")
SET(CMAKE_EXE_LINKER_FLAGS  "${LINUX_FLAGS} ${LD_FLAGS} -Wl,--gc-sections"  CACHE INTERNAL "exe link flags")

SET(CMAKE_C_FLAGS_DEBUG     "-Og -g -ggdb3"                                 CACHE INTERNAL "c debug compiler flags")
SET(CMAKE_CXX_FLAGS_DEBUG   "-Og -g -ggdb3"                                 CACHE INTERNAL "cxx debug compiler flags")
SET(CMAKE_ASM_FLAGS_DEBUG   "-g -ggdb3"                                     CACHE INTERNAL "asm debug compiler flags")

SET(CMAKE_C_FLAGS_RELEASE   "-O3"                                           CACHE INTERNAL "c release compiler flags")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3"                                           CACHE INTERNAL "cxx release compiler flags")
SET(CMAKE_ASM_FLAGS_RELEASE ""                                              CACHE INTERNAL "asm release compiler flags")

# this makes the test compiles use static library option so that we don't need to pre-set linker flags and scripts
# SET(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
SET(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
