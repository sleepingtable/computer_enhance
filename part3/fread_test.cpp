#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <immintrin.h>
#include <sys/stat.h>
#include <sys/time.h>

using f64 = double;
using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;


#ifndef PROFILER
#define PROFILER 0
#endif


u64 read_os_timer() {
    timeval value;
    gettimeofday(&value, 0);
    return 1000000 * static_cast<u64>(value.tv_sec) + static_cast<u64>(value.tv_usec);
}


void print(const std::string &name, const u64 &rdtsc_duration_exclusive, const f64 &mult_rdtsc,
           const u64 &rdtsc_tot = 0, const u64 &rdtsc_duration_inclusive = 0, const u64 &hit_count = 0,
           const u64 &processed_bytes = 0) {
    std::cout
        << std::setw(20) << name;

    if (hit_count > 0)
        std::cout << "[" << std::setw(10) << hit_count << "]";

    std::cout << std::setprecision(3)
        << ": " << std::setw(8) << rdtsc_duration_exclusive * mult_rdtsc << "s";

    if (rdtsc_tot > 0)
        std::cout << std::setprecision(3)
            << " (" << std::setw(8)
            << static_cast<float>(rdtsc_duration_exclusive) / static_cast<float>(rdtsc_tot) * 100 << "%)";

    if (rdtsc_duration_inclusive > 0 and rdtsc_tot > 0)
        std::cout << std::setprecision(3)
            << " (" << std::setw(8)
            << static_cast<float>(rdtsc_duration_inclusive) / static_cast<float>(rdtsc_tot) * 100 << "% w/ children)";

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
            ::print(name, rdtsc_elapsed_exclusive, mult_rdtsc, rdtsc_tot, rdtsc_elapsed_inclusive, hit_count, processed_bytes);
    }
};


struct RepetitionResult {
    u64 nb_try {0};
    u64 total_time {0};
    u64 max_time {0};
    u64 min_time {0};

    bool overwrite_if_min(u64 time) {
        if (min_time == 0 or time < min_time) {
            min_time = time;
            return true;
        }
        return false;

    }

    bool add(u64 time) {
        ++nb_try;
        total_time += time;
        max_time = std::max(max_time, time);
        return overwrite_if_min(time);
    }
};


f64 get_mult_rdtsc() {
    u64 end = __rdtsc();
    u64 os_timer_start = read_os_timer();
    u64 start = __rdtsc();
    u64 os_timer_end = read_os_timer();
    while (os_timer_end - os_timer_start < 100000) {
        os_timer_end = read_os_timer();
        end = __rdtsc();
    }
    return (static_cast<f64>(os_timer_end - os_timer_start) / 1000000) / static_cast<f64>(end - start);
}


struct RepetitionTester {
    const char *name;
    u64 bytes_count;
    f64 mult_rdtsc;
    u64 try_for;
    u64 started_try_at;
    u64 last_start;
    u64 total_time {0};
    u64 total_bytes {0};
    u64 repetition_count {0};
    bool verbose;

    RepetitionResult result;

    RepetitionTester(const char* name, u64 bytes_count, u64 try_for_secs, bool verbose = true) :
        name{name}, bytes_count{bytes_count}, mult_rdtsc{get_mult_rdtsc()},
        try_for{static_cast<u64>(try_for_secs / mult_rdtsc)}, started_try_at{__rdtsc()},
        verbose{verbose} {}

    bool start() {
        last_start = __rdtsc();
        return last_start - started_try_at < try_for;  // continue if true
    }

    void stop() {
        u64 elapsed = __rdtsc() - last_start;
        total_bytes += bytes_count;
        total_time += elapsed;
        if(result.add(elapsed)) {
            started_try_at = __rdtsc();
            if (verbose)
                ::print("new fastest found", elapsed, mult_rdtsc, 0, elapsed, 0, bytes_count);
        }
        ++repetition_count;
    }

    ~RepetitionTester() {
        std::cout << "Finished repetition testing " << name << "; total_time: "
            << std::setprecision(4) << static_cast<f64>(total_time) * mult_rdtsc << "s" << std::endl;
        ::print("fastest", result.min_time, mult_rdtsc, 0, result.min_time, 0, bytes_count);
        ::print("slowest", result.max_time, mult_rdtsc, 0, result.max_time, 0, bytes_count);
        u64 average = static_cast<u64>(total_time / repetition_count);
        ::print("average", average, mult_rdtsc, 0, average, 0, bytes_count);
        std::cout << std::endl;
    }
};

struct Data {
    u8 *data {nullptr};
    u64 size {0};
    u64 cursor {0};
};


static Data DATA;


int main() {
    const char *filename = "haversine_large_input.json";
    FILE *file = fopen(filename, "rb");

    if (file) {
        struct stat _stat;
        stat(filename, &_stat);
        DATA.size = _stat.st_size;
        DATA.data = static_cast<u8*>(malloc(_stat.st_size));
        fclose(file);

        {
            RepetitionTester tester{"no malloc", DATA.size, 3, false};
            while(tester.start()) {
                file = fopen(filename, "rb");
                fread(DATA.data, DATA.size, 1, file);
                fclose(file);
                tester.stop();
            }
        }

        free(DATA.data);
        DATA.data = static_cast<u8*>(malloc(DATA.size));

        RepetitionTester tester{"with new malloc", DATA.size, 3, false};
        while(tester.start()) {
            file = fopen(filename, "rb");
            fread(DATA.data, DATA.size, 1, file);
            fclose(file);
            tester.stop();

            free(DATA.data);
            DATA.data = static_cast<u8*>(malloc(DATA.size));
        }
    }
}
