clear
git clean -xfd
cmake -Bbuild -DCMAKE_BUILD_TYPE=MINSIZEREL -DCMAKE_TOOLCHAIN_FILE=./cmake/arm-linux-gnueabi.cmake -GNinja .
cmake --build ./build