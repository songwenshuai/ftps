
# Name of the target
SET(CMAKE_CROSSCOMPILING        TRUE)
SET(CMAKE_SYSTEM_NAME           Linux)
SET(CMAKE_SYSTEM_PROCESSOR      cortex-a53)

# Name of the target
SET(CROSS_ROOT                  /opt/cross-arm/gcc-linaro-5.3.1-2016.05-x86_64_arm-linux-gnueabi)
SET(CROSS_COMPILE               ${CROSS_ROOT}/bin/arm-linux-gnueabi-)

# Name of the target
SET(CMAKE_SYSROOT               ${CROSS_ROOT}/arm-linux-gnueabi/libc)
SET(CMAKE_FIND_ROOT_PATH        ${CROSS_ROOT})

# Name of the target
SET(CMAKE_ASM_COMPILER          ${CROSS_COMPILE}gcc)
SET(CMAKE_C_COMPILER            ${CROSS_COMPILE}gcc)
SET(CMAKE_CXX_COMPILER          ${CROSS_COMPILE}g++)
SET(CMAKE_C_COMPILER_AR         ${CROSS_COMPILE}ar)
SET(CMAKE_C_COMPILER_RANLIB     ${CROSS_COMPILE}ranlib)
SET(CMAKE_CXX_COMPILER_AR       ${CROSS_COMPILE}ar)
SET(CMAKE_CXX_COMPILER_RANLIB   ${CROSS_COMPILE}ranlib)
SET(LD                          ${CROSS_COMPILE}gcc)
SET(AS                          ${CROSS_COMPILE}as)
SET(OBJCOPY                     ${CROSS_COMPILE}objcopy)
SET(OBJDUMP                     ${CROSS_COMPILE}objdump)
SET(SIZE                        ${CROSS_COMPILE}size)
SET(NM                          ${CROSS_COMPILE}nm)
SET(STRIP                       ${CROSS_COMPILE}strip)
SET(CPP                         ${CMAKE_C_COMPILER} -E)
SET(CXXCPP                      ${CMAKE_CXX_COMPILER} -E)

# Name of the target
SET(VFP_FLAGS                   "")
SET(SPEC_FLAGS                  "-Wstrict-prototypes -fgnu89-inline")
SET(LD_FLAGS                    "")
SET(MCPU_FLAGS                  "-mcpu=cortex-a53 -marm -mlittle-endian -mfpu=neon -mfloat-abi=softfp")
SET(DEFS                        "-fPIC -D_REENTRANT -DPURE_LITTLE_ENDIAN")
SET(OPT                         "-O2 -fno-strict-aliasing")
SET(BASE_FLAGS                  "-Wall -fdata-sections -ffunction-sections -mlong-calls -Wno-psabi -fexceptions")
SET(LINUX_FLAGS                 "-g -pthread")

# Name of the target
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# this makes the test compiles use static library option so that we don't need to pre-set linker flags and scripts
# SET(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
# SET(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

# Name of the target
SET(CMAKE_C_FLAGS           "${LINUX_FLAGS} ${OPT} ${DEFS} ${BASE_FLAGS} ${MCPU_FLAGS} ${VFP_FLAGS} ${SPEC_FLAGS}"             CACHE INTERNAL "c compiler flags")
SET(CMAKE_CXX_FLAGS         "${LINUX_FLAGS} ${OPT} ${DEFS} ${BASE_FLAGS} ${MCPU_FLAGS} ${VFP_FLAGS} -fno-rtti -fno-exceptions" CACHE INTERNAL "cxx compiler flags")
SET(CMAKE_ASM_FLAGS         "${LINUX_FLAGS} ${MCPU_FLAGS} -x assembler-with-cpp"                                               CACHE INTERNAL "asm compiler flags")
SET(CMAKE_EXE_LINKER_FLAGS  "${LINUX_FLAGS} ${MCPU_FLAGS} ${LD_FLAGS} -Wl,--gc-sections"                                       CACHE INTERNAL "exe link flags")

# Name of the target
# SET(CMAKE_C_FLAGS_DEBUG     "-Og -g -ggdb3" CACHE INTERNAL "c debug compiler flags")
# SET(CMAKE_CXX_FLAGS_DEBUG   "-Og -g -ggdb3" CACHE INTERNAL "cxx debug compiler flags")
# SET(CMAKE_ASM_FLAGS_DEBUG   "-g -ggdb3"     CACHE INTERNAL "asm debug compiler flags")

# Name of the target
# SET(CMAKE_C_FLAGS_RELEASE   "-O3"           CACHE INTERNAL "c release compiler flags")
# SET(CMAKE_CXX_FLAGS_RELEASE "-O3"           CACHE INTERNAL "cxx release compiler flags")
# SET(CMAKE_ASM_FLAGS_RELEASE ""              CACHE INTERNAL "asm release compiler flags")

# cross reference
#
# https://gitlab.kitware.com/cmake/community/-/wikis/doc/cmake/CrossCompiling
# https://blog.kitware.com/cross-compiling-for-raspberry-pi/

# flag reference
#
# https://gcc.gnu.org/onlinedocs/gcc-7.3.0/gcc/Option-Summary.html#Option-Summary
# https://blog.csdn.net/a568478312/article/details/79195218/

# comment
#
# -Wall                                 启用大部分警告信息
# -fno-rtti                             禁用运行时类型信息.
# -fno-exceptions                       禁用异常机制.
# -Wstrict-prototypes                   使用了非原型的函数声明时给出警告.
# -fgnu89-inline                        为内联函数使用传统的 GNU 语义.
# -mfpu = name (neon or vfpvx)          ARM平台指定浮点数运算优化, 软浮点和硬浮点, 以及浮点运算向量单元.
# -mfloat-abi = name (soft hard softfp) ARM平台指定浮点数运算优化, 软浮点和硬浮点, 以及浮点运算向量单元.
# -fPIC                                 尽可能生成与位置无关的代码(大模式).
# -O<N>                                 将优化等级设为 N.
# -fno-strict-aliasing                  需要让不同指针指向同一个内存位置, 其实在开启优化选项 -O2 和 -O3 的情况下, gcc 会自动采用 strict.
# -mcpu=cortex-a53
# -marm
# -mlittle-endian
# -D_REENTRANT
# -DPURE_LITTLE_ENDIAN
# -Wno-error=unused-variable            有未使用的变量时警告
# -Wno-error=unused-but-set-variable    Warn when a variable is only set, otherwise unused
# -Wno-unused-local-typedefs            Warn when typedefs locally defined in a function are not used
# -Wno-sizeof-pointer-memaccess
# -Wno-maybe-uninitialized              Warn about maybe uninitialized automatic variables
# -fdata-sections                       将每个数据项分别放在它们各自的节中
# -ffunction-sections                   将每个函数分别放在它们各自的节中
# -mlong-calls
# -Wno-psabi                            suppress these va_list mangling warnings with gcc -Wno-psabi
# -fexceptions                          启用异常处理
# -x assembler-with-cpp
# -Wl,<选项>                            将逗号分隔的 <选项> 传递给链接器
# -Wl,--gc-sections                     删除未使用的节(在某些目标上)