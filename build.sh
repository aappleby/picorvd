rm -rf build
mkdir -p build
cmake -B build && make -j8 -C build
