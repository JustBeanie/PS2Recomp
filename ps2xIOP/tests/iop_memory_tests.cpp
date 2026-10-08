#include "emulator/core/iop_memory.h"
#include "emulator/services/iop_module_loader.h"

#include <cstdint>
#include <iostream>

using ps2x::iop::detail::IopMemory;

namespace
{
    int failures = 0;

    void check(bool value, const char *message)
    {
        if (!value)
        {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    }
}

int main()
{
    {
        // The SYSMEM pool covers the RAM modules don't use. A fixed 832 KB
        // window (0x120000-0x1F0000) starved Sly 2's sound driver, whose
        // 240 KB stream buffer failed after the other drivers' allocations.
        IopMemory memory;
        const uint32_t big = memory.allocate(0x100000u, 256u);
        check(big != 0u, "1 MB AllocSysMemory must fit in the shared pool");
        check((big & 0xFFu) == 0u, "Pool blocks must be 256-byte aligned");
        check(big >= 0x00010000u && big + 0x100000u <= IopMemory::HeapLimit,
              "Pool blocks must stay out of the kernel area and the call stacks");
    }

    {
        // Blocks never overlap each other, so modules and allocations taken
        // from the same pool can't collide.
        IopMemory memory;
        const uint32_t a = memory.allocate(0x8000u, 256u);
        const uint32_t b = memory.allocate(0x8000u, 256u);
        check(a != 0u && b != 0u && (b >= a + 0x8000u || a >= b + 0x8000u),
              "Pool blocks must not overlap");
        check(memory.freeAllocation(a), "A pool block must be freeable");
        const uint32_t c = memory.allocate(0x8000u, 256u);
        check(c == a, "A freed block must be reusable");
    }

    if (failures != 0)
        return 1;
    std::cout << "ps2xIOP memory tests passed\n";
    return 0;
}
