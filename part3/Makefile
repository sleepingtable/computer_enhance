all: haversine haversine_profiling haversine_generator fread_test page_faults write_loop write_speed_main read_speed_main

haversine: haversine.cpp
	g++ -std=c++20 -Wall -fopenmp haversine.cpp -o haversine
	chmod +x haversine

haversine_profiling: haversine.cpp
	g++ -std=c++20 -Wall -DPROFILER=1 -fopenmp haversine.cpp -o haversine_profiling
	chmod +x haversine_profiling

haversine_generator: haversine_generator.cpp
	g++ -std=c++20 -Wall haversine_generator.cpp -o haversine_generator
	chmod +x haversine_generator

fread_test: fread_test.cpp
	g++ -std=c++20 -Wall fread_test.cpp -o fread_test
	chmod +x fread_test

page_faults: page_faults.cpp
	clang++ -g -std=c++20 -Wall -O2 page_faults.cpp -o page_faults
	chmod +x page_faults

write_loop: write_loop.cpp nop_loop.obj
	clang++ -g -std=c++20 -Wall -O1 write_loop.cpp nop_loop.obj -o write_loop
	chmod +x write_loop

nop_loop.obj: nop_loop.asm
	nasm -f elf64 -o nop_loop.obj nop_loop.asm

write_speed.obj: write_speed.asm
	nasm -f elf64 -o write_speed.obj write_speed.asm

write_speed_main: write_speed_main.cpp write_speed.obj
	clang++ -g -std=c++20 -Wall -O1 write_speed_main.cpp write_speed.obj -o write_speed_main

read_speed.obj: read_speed.asm
	nasm -f elf64 -o read_speed.obj read_speed.asm

read_speed_main: read_speed_main.cpp read_speed.obj
	clang++ -g -std=c++20 -Wall -O1 read_speed_main.cpp read_speed.obj -o read_speed_main

clean:
	rm haversine haversine_profiling haversine_generator fread_test page_faults write_loop write_speed_main read_speed_main
