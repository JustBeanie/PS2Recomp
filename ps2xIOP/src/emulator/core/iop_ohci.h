#pragma once

#include <array>
#include <cstdint>

namespace ps2x::iop::detail
{
    // Minimal OHCI 1.0 host controller at the IOP's USB register block.
    //
    // Models only what a driver needs to bring the controller up and find
    // nothing plugged in: a valid revision, a software reset that completes,
    // write-1-to-clear interrupt status, enable/disable pairs, and a two-port
    // root hub with no devices connected. No schedules are ever processed.
    // Without it every register reads back as plain storage, the reset bit
    // never clears, and usbd ("USB_driver") unloads itself at start.
    class IopOhci
    {
    public:
        static constexpr uint32_t Base = 0x1F801600u;
        static constexpr uint32_t Size = 0x100u;

        IopOhci() noexcept { reset(); }

        void reset() noexcept;
        [[nodiscard]] static bool contains(uint32_t physical) noexcept
        {
            return physical >= Base && physical < Base + Size;
        }
        [[nodiscard]] uint32_t read32(uint32_t physical) const noexcept;
        void write32(uint32_t physical, uint32_t value) noexcept;

    private:
        static constexpr uint32_t kRegisterCount = Size / 4u;
        std::array<uint32_t, kRegisterCount> m_regs{};
        uint32_t m_interruptEnable = 0u;
    };
}
