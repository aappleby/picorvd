rm -rf bin
rm -rf obj
mkdir -p bin
mkdir -p obj
riscv64-unknown-elf-gcc -g -Os -flto -ffunction-sections -static-libgcc -lgcc -march=rv32ec -mabi=ilp32e -I/usr/include/newlib -nostdlib -I. -Wall -c ch32v003fun.c -o obj/ch32v003fun.o 
riscv64-unknown-elf-g++ -g -Os -flto -ffunction-sections -static-libgcc -lgcc -march=rv32ec -mabi=ilp32e -I/usr/include/newlib -nostdlib -I. -Wall -c blink.cpp -o obj/blink.o
riscv64-unknown-elf-g++ -g -Os -flto -ffunction-sections -static-libgcc -lgcc -march=rv32ec -mabi=ilp32e -I/usr/include/newlib -nostdlib -I. -Wall -T ch32v003fun.ld  -Wl,--gc-sections -o bin/blink.elf obj/ch32v003fun.o obj/blink.o -lgcc 
riscv64-unknown-elf-size bin/blink.elf
riscv64-unknown-elf-objdump -S bin/blink.elf > bin/blink.lst
riscv64-unknown-elf-objdump -t bin/blink.elf > bin/blink.map
riscv64-unknown-elf-objcopy -O binary bin/blink.elf bin/blink.bin
riscv64-unknown-elf-objcopy -O ihex bin/blink.elf bin/blink.hex
cd bin && xxd -i blink.bin > blink.h
