#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <linux/perf_event.h>    /* Definition of PERF_* constants */
#include <linux/hw_breakpoint.h> /* Definition of HW_* constants */
#include <sys/syscall.h>         /* Definition of SYS_* constants */
#include <unistd.h>


using u8 = uint8_t;
using u64 = uint64_t;

static size_t page_size {4096};
static size_t page_count {4000};
//static size_t page_count {128};
static size_t total_size {page_size * page_count};

/*
void print_ptr(void *ptr) {
    auto int_ptr = reinterpret_cast<u64>(ptr);
    u64 first = 63;
    for (u64 i = first; i > first - 16; --i)
        std::cout << (int_ptr >> i & 1);
    std::cout << "|";
    first -= 16;
    for (u64 i = first; i > first - 9; --i)
        std::cout << (int_ptr >> i & 1);
    std::cout << "|";
    first -= 9;
    for (u64 i = first; i > first - 9; --i)
        std::cout << (int_ptr >> i & 1);
    std::cout << "|";
    first -= 9;
    for (u64 i = first; i > first - 9; --i)
        std::cout << (int_ptr >> i & 1);
    std::cout << "|";
    first -= 9;
    for (u64 i = first; i > first - 9; --i)
        std::cout << (int_ptr >> i & 1);
    std::cout << "|";
    first -= 9;
    for (u64 i = first + 1; i > 0; --i)
        std::cout << (int_ptr >> (i - 1) & 1);
    std::cout << std::endl;
}*/


void touch_memory(u8* tab, const u64 &touch_count) {
    for (u64 i = 0; i < touch_count * page_size; ++i)
        tab[i] = (u8)i;
}


int main() {
    remove("page_faults.txt");
    std::ofstream outfile;
    outfile.open("page_faults.txt");

    for (size_t touch_count {1}; touch_count <= page_count; ++touch_count) {
        /*
        struct perf_event_attr pf_attr = {
            .type = PERF_TYPE_SOFTWARE,
            .size = sizeof(pf_attr),
            .config = PERF_COUNT_SW_PAGE_FAULTS,
            .disabled = 0,
            .exclude_kernel = 0, // 0 or 1, doesn't matter
            .exclude_hv = 1,
        };
        int fd = syscall(SYS_perf_event_open, &pf_attr, 0, -1, -1, 0);
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        */

        void *ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        //std::cout << std::setw(3) << touch_count << ": ";
        //print_ptr(ptr);
        if (ptr == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }
        u8 *tab = (u8 *)ptr;
        //rusage usage;

        //getrusage(RUSAGE_SELF, &usage);
        //long soft_page_faults {usage.ru_minflt};
        //long hard_page_faults {usage.ru_majflt};

        touch_memory(tab, touch_count);

        /*
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        u64 val;
        read(fd, &val, sizeof(val));
        close(fd);
        */
        //getrusage(RUSAGE_SELF, &usage);
        //soft_page_faults = usage.ru_minflt - soft_page_faults;
        //hard_page_faults = usage.ru_majflt - hard_page_faults;

        //munmap(ptr, total_size);
        //outfile << touch_count << ":" << soft_page_faults << "," << hard_page_faults << std::endl;
        //outfile << touch_count << ":" << val << std::endl;
        std::cout << touch_count << "/" << page_count << std::endl;
    }
    outfile.close();
    return EXIT_SUCCESS;
}
