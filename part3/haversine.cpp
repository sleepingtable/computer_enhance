#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <immintrin.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <vector>

using f64 = double;
using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;


#ifndef PROFILER
#define PROFILER 0
#endif


static f64 EARTH_RADIUS {6372.8};


u64 read_os_timer() {
    timeval value;
    gettimeofday(&value, 0);
    return 1000000 * static_cast<u64>(value.tv_sec) + static_cast<u64>(value.tv_usec);
}


void print(const std::string &name, const u64 &rdtsc_duration_exclusive, const u64 &rdtsc_tot, const f64 &mult_rdtsc,
           const u64 &rdtsc_duration_inclusive = 0, const u64 &hit_count = 0, const u64 &processed_bytes = 0) {
    std::cout
        << std::setw(20) << name;

    if (hit_count > 0)
        std::cout << "[" << std::setw(10) << hit_count << "]";

    std::cout
        << std::setprecision(3)
        << ": " << std::setw(8) << rdtsc_duration_exclusive * mult_rdtsc << "s"
        << " (" << std::setw(8) << static_cast<float>(rdtsc_duration_exclusive) / static_cast<float>(rdtsc_tot) * 100 << "%)";

    if (rdtsc_duration_inclusive > 0)
        std::cout
            << std::setprecision(3)
            << " (" << std::setw(8) << static_cast<float>(rdtsc_duration_inclusive) / static_cast<float>(rdtsc_tot) * 100 << "% w/ children)";

    if (processed_bytes > 0) {
        f64 megabyte = 1024. * 1024.;
        f64 gigabyte = megabyte * 1024.;
        f64 seconds = static_cast<f64>(rdtsc_duration_inclusive) * mult_rdtsc;
        f64 bytes_per_second = processed_bytes / seconds;
        f64 megabytes = processed_bytes / megabyte;
        f64 gigabytes_per_second = bytes_per_second / gigabyte;
        std::cout
            << "  " << std::setprecision(3)
            << std::setw(8) << megabytes << "MB at "
            << std::setw(8) << gigabytes_per_second << "GB/s";
    }

    std::cout << std::endl;
}


struct ProfilingItem {
    u64 rdtsc_elapsed_inclusive {0};
    u64 rdtsc_elapsed_exclusive {0};
    u64 hit_count {0};
    u64 processed_bytes {0};
    char const *name {nullptr};

    void print(const u64 &rdtsc_tot, const f64 &mult_rdtsc) const {
        if (name != nullptr)
            ::print(name, rdtsc_elapsed_exclusive, rdtsc_tot, mult_rdtsc, rdtsc_elapsed_inclusive, hit_count, processed_bytes);
    }
};


struct Instrumentation {
    std::string name;
    u64 rdtsc_begin {0};
    u64 rdtsc_end {0};
    f64 os_begin;
    f64 os_end {0};
    ProfilingItem items[4098];
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
        print(rdtsc_tot, mult_rdtsc);
        for (const auto &el : items)
            el.print(rdtsc_tot, mult_rdtsc);
    }
};


static Instrumentation INSTRUMENTATION;


static u32 CURRENT_INSTRUMENT_INDEX {0};


struct ProfilingBlock {
    char const *name;
    u32 index;
    u32 parent_index;
    u64 rdtsc_begin;
    u64 old_rdtsc_inclusive;
    ProfilingBlock(char const *name, u32 index, u64 bytes_count = 0) :
        name{name}, index{index}, parent_index{CURRENT_INSTRUMENT_INDEX}
    {
        CURRENT_INSTRUMENT_INDEX = index;
        ProfilingItem* item = INSTRUMENTATION.items + index;
        old_rdtsc_inclusive = item->rdtsc_elapsed_inclusive;
        item->processed_bytes += bytes_count;
        rdtsc_begin = __rdtsc();
    }
    ~ProfilingBlock() {
        u64 elapsed = __rdtsc() - rdtsc_begin;

        ProfilingItem *item = INSTRUMENTATION.items + index;
        item->rdtsc_elapsed_exclusive += elapsed;
        item->rdtsc_elapsed_inclusive = old_rdtsc_inclusive + elapsed;
        ++item->hit_count;
        item->name = name;

        if (parent_index != index) {
            ProfilingItem *parent = INSTRUMENTATION.items + parent_index;
            parent->rdtsc_elapsed_exclusive -= elapsed;
            CURRENT_INSTRUMENT_INDEX = parent_index;
        }
    }
};

#define NAME_CONCAT2(A, B) A##B
#define NAME_CONCAT(A, B) NAME_CONCAT2(A, B)

#if PROFILER
#define PROFILE_BANDWIDTH(name, byte_count) ProfilingBlock NAME_CONCAT(block, __LINE__){name, __COUNTER__ + 1, byte_count};
#define PROFILE_BLOCK(name) PROFILE_BANDWIDTH(name, 0);
#define PROFILE_FUNC PROFILE_BLOCK(__FUNCTION__);
#define PROFILE_FUNC_OMP_MASTER _Pragma("omp master") PROFILE_FUNC;
#else
#define PROFILE_BANDWIDTH(...)
#define PROFILE_BLOCK(...)
#define PROFILE_FUNC
#define PROFILE_FUNC_OMP_MASTER
#endif


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
    PROFILE_FUNC_OMP_MASTER;

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


struct Data {
    u8 *data {nullptr};
    u64 size {0};
    u64 cursor {0};
};


static Data DATA;


void free_data() {
    PROFILE_FUNC;
    free(DATA.data);
    DATA.data = nullptr;
    DATA.size = 0;
    DATA.cursor = 0;
}


void read_file(const char* filename) {
    PROFILE_FUNC;
    FILE *file = fopen(filename, "rb");
    if (file) {
        struct stat _stat;
        stat(filename, &_stat);
        DATA.size = _stat.st_size;
        DATA.data = static_cast<u8*>(malloc(_stat.st_size));

        PROFILE_BANDWIDTH("fread", DATA.size);
        if (fread(DATA.data, DATA.size, 1, file) != 1) {
            free_data();
            throw std::runtime_error{"Failed to fread"};
        }
    }
    fclose(file);
}


void get_char(char* chr) {
    if(DATA.cursor < DATA.size)
        *chr = static_cast<char>(DATA.data[DATA.cursor++]);
    else
        *chr = '\0';
}


char get_next_token() {
    PROFILE_FUNC;
    char next;
    get_char(&next);
    while(next == ' ' || next == '\n' || next == '\t')
        get_char(&next);
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


std::string get_key() {
    PROFILE_FUNC;
    char next = {' '};
    while(next != '"')
        get_char(&next);
    std::stringstream content;
    get_char(&next);
    while(next != '"') {
        content << next;
        get_char(&next);
    }
    get_char(&next);
    assert(next == ':');
    return content.str();
}


f64 get_value_f64() {
    PROFILE_FUNC;
    char next = {' '};
    while(next == ' ')
        get_char(&next);
    std::stringstream content;
    while(next != ',' && next != '}') {
        content << next;
        get_char(&next);
    }
    return std::stod(content.str());
}


void add_point(points &ps) {
    PROFILE_FUNC;
    assert(get_next_token() == '{');
    assert(get_key() == "x0");
    ps.x0.emplace_back(get_value_f64());
    assert(get_key() == "y0");
    ps.y0.emplace_back(get_value_f64());
    assert(get_key() == "x1");
    ps.x1.emplace_back(get_value_f64());
    assert(get_key() == "y1");
    ps.y1.emplace_back(get_value_f64());
}


points get_points(std::string json_file_path) {
    PROFILE_FUNC;
    read_file(json_file_path.c_str());
    assert(get_next_token() == '{');
    assert(get_key() == "pairs");
    assert(get_next_token() == '[');
    points ps;
    add_point(ps);
    while(get_next_token() == ',')
        add_point(ps);
    free_data();
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
#pragma omp parallel for reduction(+:mean)
    for(u64 i = 0; i < ps.x0.size(); ++i)
        mean += reference_haversine(ps.x0[i], ps.x1[i], ps.y0[i], ps.y1[i]);

    mean /= ps.x0.size();
    std::cout << "mean: " << mean << std::endl;
}
