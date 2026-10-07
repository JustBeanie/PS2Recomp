#include "iop_ohci.h"

namespace ps2x::iop::detail
{
    namespace
    {
        // Register offsets, OHCI 1.0a section 7.
        constexpr uint32_t kRevision = 0x00u;
        constexpr uint32_t kCommandStatus = 0x08u;
        constexpr uint32_t kInterruptStatus = 0x0Cu;
        constexpr uint32_t kInterruptEnable = 0x10u;
        constexpr uint32_t kInterruptDisable = 0x14u;
        constexpr uint32_t kFmInterval = 0x34u;
        constexpr uint32_t kLsThreshold = 0x44u;
        constexpr uint32_t kRhDescriptorA = 0x48u;
        constexpr uint32_t kRhPortStatus1 = 0x54u;
        constexpr uint32_t kRhPortStatus2 = 0x58u;

        constexpr uint32_t kRevision10 = 0x10u;
        constexpr uint32_t kHostControllerReset = 1u << 0;
        constexpr uint32_t kDefaultFmInterval = 0x2EDFu;
        constexpr uint32_t kDefaultLsThreshold = 0x0628u;
        constexpr uint32_t kPortCount = 2u;
        constexpr uint32_t kNoPowerSwitching = 1u << 9;
        constexpr uint32_t kPortPowerStatus = 1u << 8;
    }

    void IopOhci::reset() noexcept
    {
        m_regs.fill(0u);
        m_interruptEnable = 0u;
        m_regs[kRevision / 4u] = kRevision10;
        m_regs[kFmInterval / 4u] = kDefaultFmInterval;
        m_regs[kLsThreshold / 4u] = kDefaultLsThreshold;
        // Ports are always powered and nothing is ever connected.
        m_regs[kRhDescriptorA / 4u] = kPortCount | kNoPowerSwitching;
        m_regs[kRhPortStatus1 / 4u] = kPortPowerStatus;
        m_regs[kRhPortStatus2 / 4u] = kPortPowerStatus;
    }

    uint32_t IopOhci::read32(uint32_t physical) const noexcept
    {
        const uint32_t offset = (physical - Base) & ~3u;
        switch (offset)
        {
        case kInterruptEnable:
        case kInterruptDisable:
            return m_interruptEnable;
        default:
            return m_regs[offset / 4u];
        }
    }

    void IopOhci::write32(uint32_t physical, uint32_t value) noexcept
    {
        const uint32_t offset = (physical - Base) & ~3u;
        switch (offset)
        {
        case kRevision:
        case kRhDescriptorA:
            return; // read-only
        case kCommandStatus:
            if (value & kHostControllerReset)
            {
                reset(); // completes immediately, so HCR reads back clear
                return;
            }
            m_regs[offset / 4u] |= value;
            return;
        case kInterruptStatus:
            m_regs[offset / 4u] &= ~value;
            return;
        case kInterruptEnable:
            m_interruptEnable |= value;
            return;
        case kInterruptDisable:
            m_interruptEnable &= ~value;
            return;
        case kRhPortStatus1:
        case kRhPortStatus2:
            return; // no device: connect/enable/reset requests have nothing to act on
        default:
            m_regs[offset / 4u] = value;
            return;
        }
    }
}
