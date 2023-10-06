#rm -rf bin
#mkdir -p bin
(cd example; ./build.sh)
cmake -B bin && make -j8 -C bin
