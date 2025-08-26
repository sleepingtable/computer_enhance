#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <immintrin.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <sys/time.h>
#include <vector>

using f64 = double;
using u32 = uint32_t;
using u64 = uint64_t;


static f64 EARTH_RADIUS {6372.8};


u64 read_os_timer() {
    timeval value;
    gettimeofday(&value, 0);
    return 1000000 * static_cast<u64>(value.tv_sec) + static_cast<u64>(value.tv_usec);
}


void print(const std::string &name, const u64 &rdtsc_duration, const u64 &rdtsc_tot, const f64 &mult_rdtsc, const u64 &rdtsc_duration_children = 0) {
    std::cout
        << name << ": "
        << (rdtsc_duration - rdtsc_duration_children) * mult_rdtsc << "s"
        << " (" << static_cast<float>(rdtsc_duration - rdtsc_duration_children) / static_cast<float>(rdtsc_tot) * 100 << "%)";

    if (rdtsc_duration_children != 0)
        std::cout
            << " (" << static_cast<float>(rdtsc_duration) / static_cast<float>(rdtsc_tot) * 100 << "% w/ children)";

    std::cout << std::endl;
}


struct InstrumentItem {
    u64 rdtsc_elapsed {0};
    u64 rdtsc_elapsed_children {0};
    u64 hit_count {0};
    char const *name {nullptr};

    void print(const u64 &rdtsc_tot, const f64 &mult_rdtsc) const {
        if (name != nullptr)
            std::cout << rdtsc_elapsed << " -- " << rdtsc_elapsed_children << std::endl;
        if (name != nullptr)
            ::print(name, rdtsc_elapsed, rdtsc_tot, mult_rdtsc, rdtsc_elapsed_children);
    }
};


struct Instrumentation {
    std::string name;
    u64 rdtsc_begin {0};
    u64 rdtsc_end {0};
    f64 os_begin;
    f64 os_end {0};
    InstrumentItem items[4098];
    explicit Instrumentation() {
        name = "Total time";
        start();
    }
    void start() {
        rdtsc_begin = __rdtsc();
        os_begin = static_cast<f64>(read_os_timer()) / 1000000;
    }
    void end() {
        os_end = static_cast<f64>(read_os_timer()) / 1000000;
        rdtsc_end = __rdtsc();
    }
    void print(const u64 &rdtsc_tot, const f64 &mult_rdtsc) {
        auto duration = rdtsc_end - rdtsc_begin;
        ::print(name, duration, rdtsc_tot, mult_rdtsc);
    }
    ~Instrumentation() {
        end();
        u64 rdtsc_tot = rdtsc_end - rdtsc_begin;
        f64 mult_rdtsc = static_cast<f64>(os_end - os_begin) / static_cast<f64>(rdtsc_tot);
        std::cout << rdtsc_tot << std::endl;
        print(rdtsc_tot, mult_rdtsc);
        for (const auto &el : items)
            el.print(rdtsc_tot, mult_rdtsc);
    }
};


static Instrumentation INSTRUMENTATION;


static u32 CURRENT_INSTRUMENT_INDEX {0};



struct InstrumentBlock {
    char const *name;
    u32 index;
    u32 parent_index;
    u64 rdtsc_begin;
    InstrumentBlock(char const *name, u32 index) : name{name}, index{index}, rdtsc_begin{__rdtsc()} {
        parent_index = CURRENT_INSTRUMENT_INDEX;
        CURRENT_INSTRUMENT_INDEX = index;
    }
    ~InstrumentBlock() {
        u64 elapsed = __rdtsc() - rdtsc_begin;
        InstrumentItem *item = INSTRUMENTATION.items + index;
        item->rdtsc_elapsed += elapsed;
        ++item->hit_count;
        item->name = name;
        if (parent_index != index) {
            InstrumentItem *parent = INSTRUMENTATION.items + parent_index;
            parent->rdtsc_elapsed_children += elapsed;
            CURRENT_INSTRUMENT_INDEX = parent_index;
        }
    }
};

#define NAME_CONCAT2(A, B) A##B
#define NAME_CONCAT(A, B) NAME_CONCAT2(A, B)
#define INSTRUMENT_BLOCK(name) InstrumentBlock NAME_CONCAT(block, __LINE__){name, __COUNTER__ + 1};
#define INSTRUMENT_FUNC INSTRUMENT_BLOCK(__FUNCTION__);


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
    INSTRUMENT_FUNC;

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
    INSTRUMENT_FUNC;
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
    INSTRUMENT_FUNC;
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
    INSTRUMENT_FUNC;
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
    INSTRUMENT_FUNC;
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
    INSTRUMENT_FUNC;
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


void print_time(std::string name, auto t0, auto t1, auto tot) {
    std::cout << name << ": " << t1 - t0 << "(" << static_cast<float>(t1 - t0) / static_cast<float>(tot) * 100 << "%)" << std::endl;
}


int main(int argc, char** argv) {
    if (argc < 2)
        throw std::runtime_error{"Not enough arguments"};

    std::string json_file_path {argv[1]};
    auto ps = get_points(json_file_path);

    f64 mean {0};
//#pragma omp parallel for reduction(+:mean)
    for(u64 i = 0; i < ps.x0.size(); ++i)
        mean += reference_haversine(ps.x0[i], ps.x1[i], ps.y0[i], ps.y1[i]);

    mean /= ps.x0.size();
    std::cout << "mean: " << mean << std::endl;
}
