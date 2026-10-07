#include "emulator/core/iop_memory.h"

#include <cstdint>
#include <iostream>

using ps2x::iop::detail::IopMemory;

namespace
{
    constexpr uint32_t kOhci = 0xBF801600u; // kseg1 view, as drivers address it

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
    IopMemory memory;

    check(memory.read32(kOhci + 0x00u) == 0x10u, "HcRevision must report OHCI 1.0");

    // usbd resets the controller and spins until HCR clears.
    memory.write32(kOhci + 0x04u, 0x80u);
    memory.write32(kOhci + 0x08u, 1u);
    check((memory.read32(kOhci + 0x08u) & 1u) == 0u, "HcCommandStatus.HCR must self-clear");
    check(memory.read32(kOhci + 0x04u) == 0u, "Reset must return HcControl to USBRESET");
    check(memory.read32(kOhci + 0x34u) == 0x2EDFu, "Reset must restore HcFmInterval");

    memory.write32(kOhci + 0x10u, 0x8000005Au);
    memory.write32(kOhci + 0x14u, 0x00000002u);
    check(memory.read32(kOhci + 0x10u) == 0x80000058u, "HcInterruptDisable must clear enable bits");
    check(memory.read32(kOhci + 0x14u) == 0x80000058u, "HcInterruptDisable must read back the enable mask");

    memory.write32(kOhci + 0x0Cu, 0xFFFFFFFFu);
    check(memory.read32(kOhci + 0x0Cu) == 0u, "HcInterruptStatus is write-1-to-clear");

    memory.write32(kOhci + 0x00u, 0xDEADBEEFu);
    check(memory.read32(kOhci + 0x00u) == 0x10u, "HcRevision is read-only");

    check((memory.read32(kOhci + 0x48u) & 0xFFu) == 2u, "Root hub must expose two ports");
    for (uint32_t port = 0; port < 2u; ++port)
    {
        const uint32_t status = memory.read32(kOhci + 0x54u + port * 4u);
        check((status & 1u) == 0u, "Root hub ports must report no device connected");
        check((status & 0x100u) != 0u, "Root hub ports must report power on");
    }

    // Byte access goes through the same register model.
    check(memory.read8(kOhci + 0x00u) == 0x10u, "Byte read of HcRevision");

    if (failures != 0)
        return 1;
    std::cout << "ps2xIOP OHCI tests passed\n";
    return 0;
}
