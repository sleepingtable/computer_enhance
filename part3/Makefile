all: haversine haversine_profiling haversine_generator fread_test

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

clean:
	rm haversine haversine_profiling haversine_generator fread_test
