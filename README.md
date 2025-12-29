# VoidArena — A Stupidly Fast Memory Allocator

> "If malloc is slow, just don't use it."

This is a **custom arena allocator** I threw together because I was tired of watching `new int[100000000]` and `std::fill` take forever to zero out huge arrays.  
Turns out, when you know you're just gonna allocate a ton of ints, zero them, use them, and then throw everything away — you can go **way faster** than the standard library.

## What it does

- Grabs a **1GB** chunk of memory directly from the OS with `mmap` (no malloc overhead)
- Hands out memory with a **bump pointer** — literally just `current += size`
- Zeros memory with **AVX2 non-temporal stores** (bypasses cache, goes straight to RAM)
- Result: **8–12x faster** than `std::vector<int>(N)` + `std::fill(v.begin(), v.end(), 0)` on 100M ints

## Quick benchmark (on my machine)

```
VoidArena:                0.092 s
std::vector + fill:       0.941 s
Speedup:                  10.23x
```

Yeah. **Ten times faster**. Feels good.

## How to use it

```cpp
VoidArena arena;

int* data = arena.allocate<int>(100'000'000);
if (data) {
    arena.zero_ints(data, 100'000'000);
    // now data is zeroed and ready to use
}

// When you're done with this batch and want to reuse the arena
arena.rewind();
```

That's it. No fancy free, no fragmentation, no nonsense.

## Building

```bash
g++ -O3 -march=native -std=c++17 void_arena.cpp -o void_arena
./void_arena
```

Needs AVX2 (most CPUs from 2014+ have it) and Linux/macOS (uses mmap).

## What's inside

- `mmap` for the big memory block
- Bump pointer for O(1) allocation
- Manual alignment handling
- AVX2 streaming stores for zeroing (head/body/tail handling)

## Known limitations

- No individual deallocation (it's an arena, duh)
- Single-threaded only
- Fixed 1GB size (change `arena_size` if you want more/less)
- Only really shines when you're doing bulk alloc + zero

## License

Public domain / do whatever. No warranty, use at your own risk.

Made because I was bored and wanted to see how fast I could make it.

