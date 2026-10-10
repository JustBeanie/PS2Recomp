#pragma once

#include "runtime/gs/gs_backend.h"
#include "runtime/gs/gs_texture_page_cache.h"

#include <array>
#include <barrier>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class GSCpuBackend final : public GSRasterBackend
{
public:
    GSCpuBackend();
    ~GSCpuBackend() override;

    void Initialize(uint8_t *vram, uint32_t vramSize) override;
    void Reset() override;

    void Submit(const GSPrimitiveBatch &batch) override;
    void LoadClut(const GSTex0Reg &tex0, const GSTexClutReg &texclut) override;
    void BeginTransfer(const GSTransferCommand &command) override;
    void UploadImage(const uint8_t *data, uint32_t sizeBytes) override;

    void Flush() override;
    void TextureFlush() override;
    void Sync(GSSyncReason reason) override;
    PresentationFrame Present(const GSPresentationRequest &request) override;

    bool ClearFramebuffer(const GSContext &context, uint32_t rgba) override;
    uint32_t ConsumeLocalToHostBytes(uint8_t *dst, uint32_t maxBytes) override;

    uint32_t ReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y) const override;
    void WriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y, uint32_t value) override;
    void SnapshotVram(std::vector<uint8_t> &out) const override;
    GSTransferSnapshot GetTransferSnapshot() const override;

private:
    // Draws are queued and rasterized by worker threads, each owning interleaved
    // 8-row bands of every primitive, so per-pixel submission order is kept and no
    // two workers write the same pixel. Every operation that reads or writes VRAM
    // outside a draw first drains the queue (DrainDraws).
    struct RasterWorker
    {
        std::thread thread;
        GSMem::TexturePageCache textureCache;
        uint64_t textureCacheEpoch = 0;
    };

    void StartWorkers(uint32_t count);
    void StopWorkers();
    void WorkerMain(uint32_t index);
    void KickPendingDraws();
    void WaitForDraws();
    void DrainDraws();
    const uint16_t *CurrentClutSnapshot();
    // Pages a draw writes (frame, Z) and samples (texture), cached by the registers
    // that determine them: consecutive draws almost always share both.
    void PageSetsFor(const GSDrawState &state, std::array<uint64_t, 8> &written, std::array<uint64_t, 8> &sampled);

    void ResetUnlocked();
    void LoadClutUnlocked(const GSTex0Reg &tex0, const GSTexClutReg &texclut);
    uint32_t ReadVramUnlocked(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y) const;
    uint32_t ReadTextureVramUnlocked(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y);
    void WriteVramUnlocked(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y, uint32_t value);

    void DrawPrimitive(const GSPrimitiveBatch &batch);
    void DumpTextureOnce(const GSDrawState &state);
    void DrawSprite(const GSPrimitiveBatch &batch);
    void DrawTriangle(const GSPrimitiveBatch &batch);
    void DrawLine(const GSPrimitiveBatch &batch);
    void WritePixel(const GSDrawState &state, int x, int y, int z, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t fog);
    uint32_t SampleTexture(const GSDrawState &state, float s, float t, float q, uint16_t u, uint16_t v);
    uint32_t LookupCLUT(const GSDrawState &state, uint8_t index, uint8_t cpsm, uint8_t csm, uint8_t csa, uint8_t sourcePsm);

    void PerformLocalToLocalTransfer();
    void PerformLocalToHostTransfer();
    PresentationFrame PresentFromLocalMemory(const GSPresentationRequest &request);
    bool CopyFrameToHostRgba(const GSFrameReg &frame,
                             uint32_t width,
                             uint32_t height,
                             std::vector<uint8_t> &outPixels,
                             bool preserveAlpha,
                             bool useLocalMemoryLayout,
                             bool frameBaseIsPages,
                             uint32_t sourceOriginX,
                             uint32_t sourceOriginY) const;

    using WriteVramFunc = void (*)(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    using ReadVramFunc = uint32_t (*)(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);

    static constexpr size_t kPsmHandlerCount = 1u << 6u;
    mutable std::mutex m_mutex;
    uint8_t *m_vram = nullptr;
    uint32_t m_vramSize = 0;
    std::array<ReadVramFunc, kPsmHandlerCount> m_readVramFuncs{};
    std::array<WriteVramFunc, kPsmHandlerCount> m_writeVramFuncs{};
    std::array<uint16_t, 512> m_clut{};
    // Immutable copies of m_clut handed to queued draws (GSDrawState::clut). A new
    // copy is made only when the CLUT changed since the last draw; all but the
    // newest are freed when the queue drains.
    std::vector<std::unique_ptr<std::array<uint16_t, 512>>> m_clutSnapshots;
    bool m_clutDirty = true;
    std::array<uint32_t, 2> m_clutCbp{};
    GSMem::TexturePageCache m_texturePageCache;

    GSTransferCommand m_transfer{};
    GSTransferSnapshot m_transferState{};
    std::vector<uint8_t> m_localToHostBuffer;
    size_t m_localToHostReadPos = 0;

    std::vector<std::unique_ptr<RasterWorker>> m_workers;
    std::vector<GSPrimitiveBatch> m_pendingDraws; // filled by Submit
    std::vector<GSPrimitiveBatch> m_activeDraws;  // being rasterized by the workers
    std::mutex m_workMutex;
    std::condition_variable m_workCv;
    std::condition_variable m_doneCv;
    uint64_t m_workGeneration = 0;
    uint32_t m_workersDone = 0;
    bool m_batchInFlight = false;
    bool m_stopWorkers = false;
    uint64_t m_textureCacheEpoch = 0; // bumped by TEXFLUSH; workers invalidate on change
    // 8 KB VRAM pages written / sampled by queued or in-flight draws (512 pages = 4 MB).
    std::array<uint64_t, 8> m_pendingWritePages{};
    std::array<uint64_t, 8> m_pendingReadPages{};
    // Pages written / sampled by the pending batch since its last worker barrier;
    // a draw that conflicts with them gets barrierBefore instead of a drain.
    std::array<uint64_t, 8> m_segmentWritePages{};
    std::array<uint64_t, 8> m_segmentReadPages{};
    std::unique_ptr<std::barrier<>> m_drawBarrier;
    std::array<uint64_t, 8> m_cachedWrittenPages{};
    std::array<uint64_t, 8> m_cachedSampledPages{};
    std::array<uint64_t, 3> m_cachedWrittenKey{~0ull, ~0ull, ~0ull};
    std::array<uint64_t, 2> m_cachedSampledKey{~0ull, ~0ull};
};
