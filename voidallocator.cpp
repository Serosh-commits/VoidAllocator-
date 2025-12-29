#include <immintrin.h>
#include <sys/mman.h>
#include <unistd.h>
#include <chrono>
#include <vector>
#include <cstdio>
#include <cstddef>
#include <cstdint>

class VoidArena {
    char* base;
    char* ptr;
    char* limit;
    const size_t arena_size = 1ULL << 30;  // 1GB

public:
    VoidArena() {
        base = (char*)mmap(NULL, arena_size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) {
            base = ptr = limit = nullptr;
            return;
        }
        ptr = base;
        limit = base + arena_size;
    }

    ~VoidArena() {
        if (base) {
            munmap(base, arena_size);
        }
    }

    template<typename T>
    T* allocate(std::size_t count) {
        if (!base) return nullptr;

        std::size_t bytes = count * sizeof(T);
        std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(ptr);
        std::size_t alignment = alignof(T);

        raw = (raw + alignment - 1) & ~(alignment - 1);
        char* aligned = reinterpret_cast<char*>(raw);

        if (aligned + bytes > limit) {
            return nullptr;
        }

        char* old_ptr = ptr;
        ptr = aligned + bytes;
        return reinterpret_cast<T*>(aligned);
    }

    void zero_ints(int* data, std::size_t count) {
        if (count == 0) return;

        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(data);
        std::uintptr_t aligned_addr = (addr + 31) & ~31ULL;
        std::size_t head_count = (aligned_addr - addr) / sizeof(int);

        for (std::size_t i = 0; i < head_count && i < count; ++i) {
            data[i] = 0;
        }

        std::size_t remaining = count - head_count;
        if (remaining == 0) return;

        int* body_start = reinterpret_cast<int*>(aligned_addr);
        __m256i zero_vec = _mm256_setzero_si256();

        std::size_t chunks = remaining / 8;
        for (std::size_t i = 0; i < chunks; ++i) {
            _mm256_stream_si256(reinterpret_cast<__m256i*>(body_start + i * 8), zero_vec);
        }

        std::size_t tail = remaining % 8;
        int* tail_start = body_start + chunks * 8;
        for (std::size_t i = 0; i < tail; ++i) {
            tail_start[i] = 0;
        }
    }

    void rewind() {
        ptr = base;
    }
};

int main() {
    constexpr std::size_t N = 100'000'000;

    VoidArena arena;

    auto start_void = std::chrono::high_resolution_clock::now();
    int* void_arr = arena.allocate<int>(N);
    if (void_arr) arena.zero_ints(void_arr, N);
    auto end_void = std::chrono::high_resolution_clock::now();
    double void_seconds = std::chrono::duration<double>(end_void - start_void).count();

    std::vector<int> std_vec(N);

    auto start_std = std::chrono::high_resolution_clock::now();
    std::fill(std_vec.begin(), std_vec.end(), 0);
    auto end_std = std::chrono::high_resolution_clock::now();
    double std_seconds = std::chrono::duration<double>(end_std - start_std).count();

    printf("VoidArena time:  %.3f s\n", void_seconds);
    printf("std::vector + fill time:  %.3f s\n", std_seconds);
    printf("Speedup: %.2fx\n", std_seconds / void_seconds);

    return 0;
}
