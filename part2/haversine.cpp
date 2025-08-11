#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <vector>

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


struct points {
    std::vector<f64> x0;
    std::vector<f64> x1;
    std::vector<f64> y0;
    std::vector<f64> y1;
};


void get_char(FILE* file, char* chr) {
    if(int c = fgetc(file); c != EOF)
        *chr = static_cast<char>(c);
    else
        *chr = '\0';
}


char get_next_token(FILE* file) {
    char next;
    get_char(file, &next);
    while(next == ' ' || next == '\n' || next == '\t')
        get_char(file, &next);
    assert(
        next == '\0' ||
        next == ',' ||
        next == '[' ||
        next == ']' ||
        next == '{' ||
        next == '}' ||
        next == ',' ||
        next == ':'
    );
    return next;
}


std::string get_key(FILE* file) {
    char next = {' '};
    while(next != '"')
        get_char(file, &next);
    std::stringstream content;
    get_char(file, &next);
    while(next != '"') {
        content << next;
        get_char(file, &next);
    }
    get_char(file, &next);
    assert(next == ':');
    return content.str();
}


f64 get_value_f64(FILE* file) {
    char next = {' '};
    while(next == ' ')
        get_char(file, &next);
    std::stringstream content;
    while(next != ',' && next != '}') {
        content << next;
        get_char(file, &next);
    }
    return std::stod(content.str());
}


void add_point(FILE* file, points &ps) {
    assert(get_next_token(file) == '{');
    assert(get_key(file) == "x0");
    ps.x0.emplace_back(get_value_f64(file));
    assert(get_key(file) == "y0");
    ps.y0.emplace_back(get_value_f64(file));
    assert(get_key(file) == "x1");
    ps.x1.emplace_back(get_value_f64(file));
    assert(get_key(file) == "y1");
    ps.y1.emplace_back(get_value_f64(file));
}


points get_points(std::string json_file_path) {
    FILE* file = fopen(json_file_path.c_str(), "r");
    assert(get_next_token(file) == '{');
    assert(get_key(file) == "pairs");
    assert(get_next_token(file) == '[');
    points ps;
    add_point(file, ps);
    while(get_next_token(file) == ',')
        add_point(file, ps);
    return ps;
}


int main(int argc, char** argv) {
    if (argc < 2)
        throw std::runtime_error{"Not enough arguments"};

    std::string json_file_path {argv[1]};
    auto ps = get_points(json_file_path);

    f64 mean {0};
#pragma omp parallel for reduction(+:mean)
    for(u64 i = 0; i < ps.x0.size(); ++i)
        mean += reference_haversine(ps.x0[i], ps.x1[i], ps.y0[i], ps.y1[i]);

    mean /= ps.x0.size();
    std::cout << "mean: " << mean << std::endl;
}
