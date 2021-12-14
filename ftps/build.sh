clear
git clean -xfd
cmake -Bbuild -DCMAKE_TOOLCHAIN_FILE=./cmake/linux.cmake -GNinja .
cmake --build ./build 