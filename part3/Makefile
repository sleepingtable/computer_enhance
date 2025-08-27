all: haversine haversine_profiling haversine_generator

haversine: haversine.cpp
	g++ -std=c++20 -Wall -fopenmp haversine.cpp -o haversine
	chmod +x haversine

haversine_profiling: haversine.cpp
	g++ -std=c++20 -Wall -DPROFILER=1 -fopenmp haversine.cpp -o haversine_profiling
	chmod +x haversine_profiling

haversine_generator: haversine_generator.cpp
	g++ -std=c++20 -Wall haversine_generator.cpp -o haversine_generator
	chmod +x haversine_generator

clean:
	rm haversine haversine_profiling haversine_generator
