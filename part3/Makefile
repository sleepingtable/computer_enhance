all: haversine haversine_profiling haversine_generator fread_test page_faults write_loop

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

write_loop: write_loop.cpp
	clang++ -g -std=c++20 -Wall -O1 write_loop.cpp nop_loop.obj -o write_loop
	chmod +x write_loop

clean:
	rm haversine haversine_profiling haversine_generator fread_test page_faults write_loop
