#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>


using u8 = uint8_t;
using u64 = uint64_t;
using f64 = double;

static size_t total_size {1ULL << 30};


u64 read_os_timer() {
    timeval value;
    gettimeofday(&value, 0);
    return 1000000 * static_cast<u64>(value.tv_sec) + static_cast<u64>(value.tv_usec);
}


void print(const std::string &name, const u64 &rdtsc_duration_exclusive, const f64 &mult_rdtsc,
           const u64 &rdtsc_tot = 0, const u64 &rdtsc_duration_inclusive = 0, const u64 &hit_count = 0,
           const u64 &processed_bytes = 0, const f64 &pagefaults = 0) {
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
            << " " << std::setprecision(4)
            << std::setw(8) << megabytes << "MB at "
            << std::setw(8) << gigabytes_per_second << "GB/s";
    }

    if (pagefaults > 0) {
        std::cout << " PF: " << std::setprecision(8)
            << std::setw(10) << pagefaults;

        if (processed_bytes > 0) {
            f64 kilobyte = 1024.;
            f64 kilobytes = processed_bytes / kilobyte;
            std::cout << " (" << std::setprecision(4)
                << std::setw(8) << kilobytes / pagefaults << "k/fault)";
        }
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
    u64 total_pagefaults {0};
    u64 pagefaults_max_time {0};
    u64 pagefaults_min_time {0};

    bool overwrite_if_min(u64 time, u64 pagefaults) {
        if (min_time == 0 or time < min_time) {
            pagefaults_min_time = pagefaults;
            min_time = time;
            return true;
        }
        return false;

    }

    bool add(u64 time, u64 pagefaults) {
        ++nb_try;
        total_time += time;
        total_pagefaults += pagefaults;
        if (time > max_time) {
            max_time = time;
            pagefaults_max_time = pagefaults;
        }
        return overwrite_if_min(time, pagefaults);
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
    u64 previous_pagefaults {0};
    rusage usage;
    bool verbose;

    RepetitionResult result;

    RepetitionTester(const char* name, u64 bytes_count, u64 try_for_secs, bool verbose = true) :
        name{name}, bytes_count{bytes_count}, mult_rdtsc{get_mult_rdtsc()},
        try_for{static_cast<u64>(try_for_secs / mult_rdtsc)}, started_try_at{__rdtsc()},
        verbose{verbose} {}

    bool start() {
        getrusage(RUSAGE_SELF, &usage);
        previous_pagefaults = static_cast<u64>(usage.ru_minflt);
        last_start = __rdtsc();
        return last_start - started_try_at < try_for;  // continue if true
    }

    void stop() {
        u64 elapsed = __rdtsc() - last_start;
        getrusage(RUSAGE_SELF, &usage);
        u64 pagefaults = static_cast<u64>(usage.ru_minflt) - previous_pagefaults;
        total_bytes += bytes_count;
        total_time += elapsed;
        if(result.add(elapsed, pagefaults)) {
            started_try_at = __rdtsc();
            if (verbose)
                ::print("new fastest found", elapsed, mult_rdtsc, 0, elapsed, 0, bytes_count);
        }
        ++repetition_count;
    }

    ~RepetitionTester() {
        std::cout << "Finished repetition testing " << name << "; total_time: "
            << std::setprecision(4) << static_cast<f64>(total_time) * mult_rdtsc << "s" << std::endl;
        ::print("fastest", result.min_time, mult_rdtsc, 0, result.min_time, 0, bytes_count, static_cast<f64>(result.pagefaults_min_time));
        ::print("slowest", result.max_time, mult_rdtsc, 0, result.max_time, 0, bytes_count, static_cast<f64>(result.pagefaults_max_time));
        u64 average = static_cast<u64>(total_time / repetition_count);
        ::print("average", average, mult_rdtsc, 0, average, 0, bytes_count, static_cast<f64>(result.total_pagefaults) / static_cast<f64>(repetition_count));
        std::cout << std::endl;
    }
};


u8 *alloc_mem() {
    void *ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    return static_cast<u8 *>(ptr);
}


void touch_memory(u8* tab) {
    for (u64 i = 0; i < total_size; ++i)
        tab[i] = (u8)i;
}


extern "C" void Read_mem_cache_set(u8 *, u64, u64, u64);


int main() {
    auto mem = alloc_mem();
    for (u64 i {0}; i < total_size; ++i)
        mem[i] = (u8)i;

    u64 outer_count = (1024 * 1024 * 1024) / 4096;

    for (u64 n_way_count {1}, inner_count{32}; n_way_count < 64; n_way_count<<=1, inner_count>>=1) {
        std::string name = std::to_string(n_way_count) + "-way associative test (" + std::to_string(inner_count * 2) + " sub-addrs)";
        {
            RepetitionTester tester{name.c_str(), total_size, 10, false};
            while(tester.start()) {
                Read_mem_cache_set(mem, outer_count, n_way_count, inner_count);
                tester.stop();
            }
        }
    }

    return EXIT_SUCCESS;
}
