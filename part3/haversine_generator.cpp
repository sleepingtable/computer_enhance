#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <tuple>

using f64 = double;
using u64 = uint64_t;


static f64 EARTH_RADIUS {6372.8};


static f64 square(f64 a)
{
    return a * a;
}

static f64 radians_from_degrees(f64 degrees)
{
    return 0.01745329251994329577f * degrees;
}

// NOTE(casey): EarthRadius is generally expected to be 6372.8
static f64 reference_haversine(f64 x0, f64 x1, f64 y0, f64 y1,
                               f64 earth_radius = EARTH_RADIUS)
{
    /* NOTE(casey): This is not meant to be a "good" way to calculate the Haversine distance.
       Instead, it attempts to follow, as closely as possible, the formula used in the real-world
       question on which these homework exercises are loosely based.
    */

    f64 lat1 = y0;
    f64 lat2 = y1;
    f64 lon1 = x0;
    f64 lon2 = x1;

    f64 d_lat = radians_from_degrees(lat2 - lat1);
    f64 d_lon = radians_from_degrees(lon2 - lon1);
    lat1 = radians_from_degrees(lat1);
    lat2 = radians_from_degrees(lat2);

    f64 a = square(sin(d_lat / 2.0))
            + cos(lat1) * cos(lat2) * square(sin(d_lon / 2));
    f64 c = 2.0 * asin(sqrt(a));

    return earth_radius * c;
}


struct Coordinates {
    f64 x0, x1, y0, y1;
};


auto uniform_method()
{
    return std::make_tuple(
        std::uniform_real_distribution<f64> {-180., 180.},
        std::uniform_real_distribution<f64> {-90., 90.}
    );
}


template<typename G>
auto clustered_method(G &generator)
{
    return std::make_tuple(
        std::uniform_real_distribution<f64> {
            std::uniform_real_distribution<f64>{-180., -90.}(generator),
            std::uniform_real_distribution<f64>{90., 180.}(generator)
        },
        std::uniform_real_distribution<f64> {
            std::uniform_real_distribution<f64>{-90., -45.}(generator),
            std::uniform_real_distribution<f64>{45., 90.}(generator)
        }
    );
}


int main(int argc, char** argv)
{
    if (argc < 4)
        throw std::runtime_error{"Not enough arguments"};

    std::string method = argv[1];
    u64 seed = strtoul(argv[2], nullptr, 10);
    u64 nbsamples = strtoul(argv[3], nullptr, 10);

    std::ofstream output("haversine_input.json");
    output << "{\"pairs\": \[" << std::endl;

    f64 sum{0.};
    std::mt19937_64 generator(seed);

    std::uniform_real_distribution<f64> xs, ys;
    if (method == "uniform")
        std::tie(xs, ys) = uniform_method();
    else if (method == "clustered")
        std::tie(xs, ys) = clustered_method(generator);
    else
        throw std::runtime_error{"Unknown method"};

    for(u64 i = 0; i < nbsamples; ++i)
    {
        f64 x0{xs(generator)}, x1{xs(generator)},
            y0{ys(generator)}, y1{ys(generator)};
        sum += reference_haversine(x0, x1, y0, y1);

        output << "\t{\"x0\":" << x0
               << ", \"y0\":"  << y0
               << ", \"x1\":"  << x1
               << ", \"y1\":"  << y1
               << (i == nbsamples - 1 ? "}" : "},")
               << std::endl;
    }
    output << "]}";
    sum /= nbsamples;

    std::cout << "Method: " << method << std::endl;
    std::cout << "Seed: " << seed << std::endl;
    std::cout << "Pair counts: " << nbsamples << std::endl;
    std::cout << "Average haversine: " << sum << std::endl;
}
