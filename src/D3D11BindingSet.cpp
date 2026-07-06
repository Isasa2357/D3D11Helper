#include <D3D11Helper/D3D11Gpu/D3D11BindingSet.hpp>

#include <algorithm>
#include <cassert>

namespace D3D11CoreLib {
namespace {

void ExtendRange(UINT slot, UINT& firstSlot, UINT& count) noexcept {
    if (count == 0) {
        firstSlot = slot;
        count = 1;
        return;
    }

    const UINT endSlot = std::max(firstSlot + count, slot + 1);
    firstSlot = std::min(firstSlot, slot);
    count = endSlot - firstSlot;
}



} // namespace

void D3D11ComputeBindingSet::SetShaderResource(UINT slot, ID3D11ShaderResourceView* srv) noexcept {
    assert(slot < ShaderResourceSlotCount);
    if (slot >= ShaderResourceSlotCount) {
        return;
    }
    shaderResources[slot] = srv;
    ExtendRange(slot, firstShaderResourceSlot, shaderResourceCount);
}

void D3D11ComputeBindingSet::SetUnorderedAccess(UINT slot,
                                                ID3D11UnorderedAccessView* uav,
                                                UINT initialCount) noexcept {
    assert(slot < UnorderedAccessSlotCount);
    if (slot >= UnorderedAccessSlotCount) {
        return;
    }
    unorderedAccesses[slot] = uav;
    uavInitialCounts[slot] = initialCount;
    ExtendRange(slot, firstUnorderedAccessSlot, unorderedAccessCount);
}

void D3D11ComputeBindingSet::SetConstantBuffer(UINT slot, ID3D11Buffer* buffer) noexcept {
    assert(slot < ConstantBufferSlotCount);
    if (slot >= ConstantBufferSlotCount) {
        return;
    }
    constantBuffers[slot] = buffer;
    ExtendRange(slot, firstConstantBufferSlot, constantBufferCount);
}

void D3D11ComputeBindingSet::SetSampler(UINT slot, ID3D11SamplerState* sampler) noexcept {
    assert(slot < SamplerSlotCount);
    if (slot >= SamplerSlotCount) {
        return;
    }
    samplers[slot] = sampler;
    ExtendRange(slot, firstSamplerSlot, samplerCount);
}

void D3D11ComputeBindingSet::Clear() noexcept {
    shaderResources.fill(nullptr);
    unorderedAccesses.fill(nullptr);
    uavInitialCounts.fill(UINT(-1));
    constantBuffers.fill(nullptr);
    samplers.fill(nullptr);
    firstShaderResourceSlot = shaderResourceCount = 0;
    firstUnorderedAccessSlot = unorderedAccessCount = 0;
    firstConstantBufferSlot = constantBufferCount = 0;
    firstSamplerSlot = samplerCount = 0;
}

void D3D11ComputeBindingSet::Bind(ID3D11DeviceContext* ctx) const {
    if (!ctx) {
        return;
    }
    if (shaderResourceCount != 0) {
        BindComputeShaderResources(ctx, firstShaderResourceSlot, shaderResourceCount,
                                   shaderResources.data() + firstShaderResourceSlot);
    }
    if (unorderedAccessCount != 0) {
        BindComputeUnorderedAccessViews(ctx, firstUnorderedAccessSlot, unorderedAccessCount,
                                        unorderedAccesses.data() + firstUnorderedAccessSlot,
                                        uavInitialCounts.data() + firstUnorderedAccessSlot);
    }
    if (constantBufferCount != 0) {
        BindComputeConstantBuffers(ctx, firstConstantBufferSlot, constantBufferCount,
                                   constantBuffers.data() + firstConstantBufferSlot);
    }
    if (samplerCount != 0) {
        BindComputeSamplers(ctx, firstSamplerSlot, samplerCount, samplers.data() + firstSamplerSlot);
    }
}

void D3D11ComputeBindingSet::Unbind(ID3D11DeviceContext* ctx) const {
    if (!ctx) {
        return;
    }
    UnbindComputeShaderResources(ctx, firstShaderResourceSlot, shaderResourceCount);
    UnbindComputeUnorderedAccessViews(ctx, firstUnorderedAccessSlot, unorderedAccessCount);
    UnbindComputeConstantBuffers(ctx, firstConstantBufferSlot, constantBufferCount);
    UnbindComputeSamplers(ctx, firstSamplerSlot, samplerCount);
}

void BindComputeShaderResources(ID3D11DeviceContext* ctx,
                                UINT startSlot,
                                UINT count,
                                ID3D11ShaderResourceView* const* srvs) {
    if (ctx && count != 0) {
        ctx->CSSetShaderResources(startSlot, count, srvs);
    }
}

void BindComputeUnorderedAccessViews(ID3D11DeviceContext* ctx,
                                     UINT startSlot,
                                     UINT count,
                                     ID3D11UnorderedAccessView* const* uavs,
                                     const UINT* initialCounts) {
    if (ctx && count != 0) {
        ctx->CSSetUnorderedAccessViews(startSlot, count, uavs, initialCounts);
    }
}

void BindComputeConstantBuffers(ID3D11DeviceContext* ctx,
                                UINT startSlot,
                                UINT count,
                                ID3D11Buffer* const* buffers) {
    if (ctx && count != 0) {
        ctx->CSSetConstantBuffers(startSlot, count, buffers);
    }
}

void BindComputeSamplers(ID3D11DeviceContext* ctx,
                         UINT startSlot,
                         UINT count,
                         ID3D11SamplerState* const* samplers) {
    if (ctx && count != 0) {
        ctx->CSSetSamplers(startSlot, count, samplers);
    }
}

void UnbindComputeShaderResources(ID3D11DeviceContext* ctx, UINT startSlot, UINT count) {
    if (!ctx || count == 0) {
        return;
    }
    std::array<ID3D11ShaderResourceView*, D3D11ComputeBindingSet::ShaderResourceSlotCount> nulls{};
    ctx->CSSetShaderResources(startSlot, count, nulls.data());
}

void UnbindComputeUnorderedAccessViews(ID3D11DeviceContext* ctx, UINT startSlot, UINT count) {
    if (!ctx || count == 0) {
        return;
    }
    std::array<ID3D11UnorderedAccessView*, D3D11ComputeBindingSet::UnorderedAccessSlotCount> nulls{};
    std::array<UINT, D3D11ComputeBindingSet::UnorderedAccessSlotCount> counts{};
    counts.fill(UINT(-1));
    ctx->CSSetUnorderedAccessViews(startSlot, count, nulls.data(), counts.data());
}

void UnbindComputeConstantBuffers(ID3D11DeviceContext* ctx, UINT startSlot, UINT count) {
    if (!ctx || count == 0) {
        return;
    }
    std::array<ID3D11Buffer*, D3D11ComputeBindingSet::ConstantBufferSlotCount> nulls{};
    ctx->CSSetConstantBuffers(startSlot, count, nulls.data());
}

void UnbindComputeSamplers(ID3D11DeviceContext* ctx, UINT startSlot, UINT count) {
    if (!ctx || count == 0) {
        return;
    }
    std::array<ID3D11SamplerState*, D3D11ComputeBindingSet::SamplerSlotCount> nulls{};
    ctx->CSSetSamplers(startSlot, count, nulls.data());
}

void D3D11UnbindComputeResources(ID3D11DeviceContext* ctx) {
    if (!ctx) {
        return;
    }
    UnbindComputeShaderResources(ctx, 0, D3D11ComputeBindingSet::ShaderResourceSlotCount);
    UnbindComputeUnorderedAccessViews(ctx, 0, D3D11ComputeBindingSet::UnorderedAccessSlotCount);
    UnbindComputeConstantBuffers(ctx, 0, D3D11ComputeBindingSet::ConstantBufferSlotCount);
    UnbindComputeSamplers(ctx, 0, D3D11ComputeBindingSet::SamplerSlotCount);
}

D3D11ScopedComputeBindings::D3D11ScopedComputeBindings(ID3D11DeviceContext* ctx,
                                                       const D3D11ComputeBindingSet& bindings)
    : m_ctx(ctx), m_ranges(bindings) {
    if (!m_ctx) {
        return;
    }

    if (m_ranges.shaderResourceCount != 0) {
        std::array<ID3D11ShaderResourceView*, D3D11ComputeBindingSet::ShaderResourceSlotCount> previous{};
        m_ctx->CSGetShaderResources(m_ranges.firstShaderResourceSlot,
                                    m_ranges.shaderResourceCount,
                                    previous.data());
        for (UINT i = 0; i < m_ranges.shaderResourceCount; ++i) {
            m_shaderResources[m_ranges.firstShaderResourceSlot + i].Attach(previous[i]);
        }
    }
    if (m_ranges.unorderedAccessCount != 0) {
        std::array<ID3D11UnorderedAccessView*, D3D11ComputeBindingSet::UnorderedAccessSlotCount> previous{};
        m_ctx->CSGetUnorderedAccessViews(m_ranges.firstUnorderedAccessSlot,
                                         m_ranges.unorderedAccessCount,
                                         previous.data());
        for (UINT i = 0; i < m_ranges.unorderedAccessCount; ++i) {
            m_unorderedAccesses[m_ranges.firstUnorderedAccessSlot + i].Attach(previous[i]);
        }
    }
    if (m_ranges.constantBufferCount != 0) {
        std::array<ID3D11Buffer*, D3D11ComputeBindingSet::ConstantBufferSlotCount> previous{};
        m_ctx->CSGetConstantBuffers(m_ranges.firstConstantBufferSlot,
                                    m_ranges.constantBufferCount,
                                    previous.data());
        for (UINT i = 0; i < m_ranges.constantBufferCount; ++i) {
            m_constantBuffers[m_ranges.firstConstantBufferSlot + i].Attach(previous[i]);
        }
    }
    if (m_ranges.samplerCount != 0) {
        std::array<ID3D11SamplerState*, D3D11ComputeBindingSet::SamplerSlotCount> previous{};
        m_ctx->CSGetSamplers(m_ranges.firstSamplerSlot,
                             m_ranges.samplerCount,
                             previous.data());
        for (UINT i = 0; i < m_ranges.samplerCount; ++i) {
            m_samplers[m_ranges.firstSamplerSlot + i].Attach(previous[i]);
        }
    }

    bindings.Bind(m_ctx);
}

D3D11ScopedComputeBindings::~D3D11ScopedComputeBindings() {
    Restore();
}

void D3D11ScopedComputeBindings::Restore() {
    if (!m_ctx) {
        return;
    }

    if (m_ranges.shaderResourceCount != 0) {
        std::array<ID3D11ShaderResourceView*, D3D11ComputeBindingSet::ShaderResourceSlotCount> previous{};
        for (UINT i = 0; i < m_ranges.shaderResourceCount; ++i) {
            previous[i] = m_shaderResources[m_ranges.firstShaderResourceSlot + i].Get();
        }
        m_ctx->CSSetShaderResources(m_ranges.firstShaderResourceSlot,
                                    m_ranges.shaderResourceCount,
                                    previous.data());
    }
    if (m_ranges.unorderedAccessCount != 0) {
        std::array<ID3D11UnorderedAccessView*, D3D11ComputeBindingSet::UnorderedAccessSlotCount> previous{};
        for (UINT i = 0; i < m_ranges.unorderedAccessCount; ++i) {
            previous[i] = m_unorderedAccesses[m_ranges.firstUnorderedAccessSlot + i].Get();
        }
        m_ctx->CSSetUnorderedAccessViews(m_ranges.firstUnorderedAccessSlot,
                                         m_ranges.unorderedAccessCount,
                                         previous.data(),
                                         nullptr);
    }
    if (m_ranges.constantBufferCount != 0) {
        std::array<ID3D11Buffer*, D3D11ComputeBindingSet::ConstantBufferSlotCount> previous{};
        for (UINT i = 0; i < m_ranges.constantBufferCount; ++i) {
            previous[i] = m_constantBuffers[m_ranges.firstConstantBufferSlot + i].Get();
        }
        m_ctx->CSSetConstantBuffers(m_ranges.firstConstantBufferSlot,
                                    m_ranges.constantBufferCount,
                                    previous.data());
    }
    if (m_ranges.samplerCount != 0) {
        std::array<ID3D11SamplerState*, D3D11ComputeBindingSet::SamplerSlotCount> previous{};
        for (UINT i = 0; i < m_ranges.samplerCount; ++i) {
            previous[i] = m_samplers[m_ranges.firstSamplerSlot + i].Get();
        }
        m_ctx->CSSetSamplers(m_ranges.firstSamplerSlot,
                             m_ranges.samplerCount,
                             previous.data());
    }

    m_ctx = nullptr;
}

} // namespace D3D11CoreLib
