#include <D3D11Helper/D3D11Gpu/D3D11BindingSet.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

namespace D3D11CoreLib {
namespace {

void ValidateContext(ID3D11DeviceContext* context, const char* name) {
    if (!context) {
        throw std::invalid_argument(std::string(name) + ": context must not be null");
    }
}

void ValidateRange(UINT startSlot, UINT count, UINT slotCount, const char* name) {
    if (count == 0) {
        return;
    }
    if (startSlot >= slotCount || count > slotCount - startSlot) {
        throw std::out_of_range(std::string(name) + ": slot range is out of bounds");
    }
}

template <typename T>
void ValidatePointerArray(UINT count, T const* values, const char* name) {
    if (count != 0 && !values) {
        throw std::invalid_argument(std::string(name) + ": pointer array must not be null");
    }
}

template <typename T, size_t N>
T* const* PointerAt(const std::array<T*, N>& values, UINT slot) noexcept {
    return values.data() + slot;
}

template <typename T, size_t N>
void ResetComPtrRange(std::array<ComPtr<T>, N>& values, UINT first, UINT count) noexcept {
    for (UINT i = 0; i < count; ++i) {
        values[first + i].Reset();
    }
}

} // namespace

void D3D11ComputeBindingSet::SlotRange::Include(UINT slot) noexcept {
    if (count == 0) {
        first = slot;
        count = 1;
        return;
    }

    const UINT last = std::max(first + count - 1, slot);
    first = std::min(first, slot);
    count = last - first + 1;
}

void D3D11ComputeBindingSet::Clear() noexcept {
    srvs_.fill(nullptr);
    uavs_.fill(nullptr);
    uavInitialCounts_.fill(KeepUavInitialCount);
    constantBuffers_.fill(nullptr);
    samplers_.fill(nullptr);
    srvRange_.Clear();
    uavRange_.Clear();
    constantBufferRange_.Clear();
    samplerRange_.Clear();
}

bool D3D11ComputeBindingSet::Empty() const noexcept {
    return srvRange_.Empty() && uavRange_.Empty() && constantBufferRange_.Empty() && samplerRange_.Empty();
}

void D3D11ComputeBindingSet::SetShaderResource(UINT slot, ID3D11ShaderResourceView* srv) {
    ValidateRange(slot, 1, ShaderResourceSlotCount, "D3D11ComputeBindingSet::SetShaderResource");
    srvs_[slot] = srv;
    srvRange_.Include(slot);
}

void D3D11ComputeBindingSet::SetUnorderedAccess(UINT slot, ID3D11UnorderedAccessView* uav, UINT initialCount) {
    ValidateRange(slot, 1, UnorderedAccessSlotCount, "D3D11ComputeBindingSet::SetUnorderedAccess");
    uavs_[slot] = uav;
    uavInitialCounts_[slot] = initialCount;
    uavRange_.Include(slot);
}

void D3D11ComputeBindingSet::SetConstantBuffer(UINT slot, ID3D11Buffer* buffer) {
    ValidateRange(slot, 1, ConstantBufferSlotCount, "D3D11ComputeBindingSet::SetConstantBuffer");
    constantBuffers_[slot] = buffer;
    constantBufferRange_.Include(slot);
}

void D3D11ComputeBindingSet::SetSampler(UINT slot, ID3D11SamplerState* sampler) {
    ValidateRange(slot, 1, SamplerSlotCount, "D3D11ComputeBindingSet::SetSampler");
    samplers_[slot] = sampler;
    samplerRange_.Include(slot);
}

void D3D11ComputeBindingSet::Bind(ID3D11DeviceContext* context) const {
    ValidateContext(context, "D3D11ComputeBindingSet::Bind");
    if (!srvRange_.Empty()) {
        BindComputeShaderResources(context, srvRange_.first, srvRange_.count, PointerAt(srvs_, srvRange_.first));
    }
    if (!uavRange_.Empty()) {
        BindComputeUnorderedAccessViews(context, uavRange_.first, uavRange_.count,
                                        PointerAt(uavs_, uavRange_.first), uavInitialCounts_.data() + uavRange_.first);
    }
    if (!constantBufferRange_.Empty()) {
        BindComputeConstantBuffers(context, constantBufferRange_.first, constantBufferRange_.count,
                                   PointerAt(constantBuffers_, constantBufferRange_.first));
    }
    if (!samplerRange_.Empty()) {
        BindComputeSamplers(context, samplerRange_.first, samplerRange_.count, PointerAt(samplers_, samplerRange_.first));
    }
}

void D3D11ComputeBindingSet::Unbind(ID3D11DeviceContext* context) const {
    ValidateContext(context, "D3D11ComputeBindingSet::Unbind");
    if (!srvRange_.Empty()) UnbindComputeShaderResources(context, srvRange_.first, srvRange_.count);
    if (!uavRange_.Empty()) UnbindComputeUnorderedAccessViews(context, uavRange_.first, uavRange_.count);
    if (!constantBufferRange_.Empty()) UnbindComputeConstantBuffers(context, constantBufferRange_.first, constantBufferRange_.count);
    if (!samplerRange_.Empty()) UnbindComputeSamplers(context, samplerRange_.first, samplerRange_.count);
}

void BindComputeShaderResources(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11ShaderResourceView* const* srvs) {
    ValidateContext(context, "BindComputeShaderResources");
    ValidateRange(startSlot, count, D3D11ComputeBindingSet::ShaderResourceSlotCount, "BindComputeShaderResources");
    ValidatePointerArray(count, srvs, "BindComputeShaderResources");
    if (count != 0) context->CSSetShaderResources(startSlot, count, srvs);
}

void BindComputeUnorderedAccessViews(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11UnorderedAccessView* const* uavs, const UINT* initialCounts) {
    ValidateContext(context, "BindComputeUnorderedAccessViews");
    ValidateRange(startSlot, count, D3D11ComputeBindingSet::UnorderedAccessSlotCount, "BindComputeUnorderedAccessViews");
    ValidatePointerArray(count, uavs, "BindComputeUnorderedAccessViews");
    if (count != 0) context->CSSetUnorderedAccessViews(startSlot, count, uavs, initialCounts);
}

void BindComputeConstantBuffers(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11Buffer* const* buffers) {
    ValidateContext(context, "BindComputeConstantBuffers");
    ValidateRange(startSlot, count, D3D11ComputeBindingSet::ConstantBufferSlotCount, "BindComputeConstantBuffers");
    ValidatePointerArray(count, buffers, "BindComputeConstantBuffers");
    if (count != 0) context->CSSetConstantBuffers(startSlot, count, buffers);
}

void BindComputeSamplers(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11SamplerState* const* samplers) {
    ValidateContext(context, "BindComputeSamplers");
    ValidateRange(startSlot, count, D3D11ComputeBindingSet::SamplerSlotCount, "BindComputeSamplers");
    ValidatePointerArray(count, samplers, "BindComputeSamplers");
    if (count != 0) context->CSSetSamplers(startSlot, count, samplers);
}

void UnbindComputeShaderResources(ID3D11DeviceContext* context, UINT startSlot, UINT count) {
    ValidateContext(context, "UnbindComputeShaderResources");
    ValidateRange(startSlot, count, D3D11ComputeBindingSet::ShaderResourceSlotCount, "UnbindComputeShaderResources");
    std::array<ID3D11ShaderResourceView*, D3D11ComputeBindingSet::ShaderResourceSlotCount> nulls = {};
    if (count != 0) context->CSSetShaderResources(startSlot, count, nulls.data());
}

void UnbindComputeUnorderedAccessViews(ID3D11DeviceContext* context, UINT startSlot, UINT count) {
    ValidateContext(context, "UnbindComputeUnorderedAccessViews");
    ValidateRange(startSlot, count, D3D11ComputeBindingSet::UnorderedAccessSlotCount, "UnbindComputeUnorderedAccessViews");
    std::array<ID3D11UnorderedAccessView*, D3D11ComputeBindingSet::UnorderedAccessSlotCount> nulls = {};
    if (count != 0) context->CSSetUnorderedAccessViews(startSlot, count, nulls.data(), nullptr);
}

void UnbindComputeConstantBuffers(ID3D11DeviceContext* context, UINT startSlot, UINT count) {
    ValidateContext(context, "UnbindComputeConstantBuffers");
    ValidateRange(startSlot, count, D3D11ComputeBindingSet::ConstantBufferSlotCount, "UnbindComputeConstantBuffers");
    std::array<ID3D11Buffer*, D3D11ComputeBindingSet::ConstantBufferSlotCount> nulls = {};
    if (count != 0) context->CSSetConstantBuffers(startSlot, count, nulls.data());
}

void UnbindComputeSamplers(ID3D11DeviceContext* context, UINT startSlot, UINT count) {
    ValidateContext(context, "UnbindComputeSamplers");
    ValidateRange(startSlot, count, D3D11ComputeBindingSet::SamplerSlotCount, "UnbindComputeSamplers");
    std::array<ID3D11SamplerState*, D3D11ComputeBindingSet::SamplerSlotCount> nulls = {};
    if (count != 0) context->CSSetSamplers(startSlot, count, nulls.data());
}

void D3D11UnbindComputeResources(ID3D11DeviceContext* context) {
    ValidateContext(context, "D3D11UnbindComputeResources");
    UnbindComputeShaderResources(context, 0, D3D11ComputeBindingSet::ShaderResourceSlotCount);
    UnbindComputeUnorderedAccessViews(context, 0, D3D11ComputeBindingSet::UnorderedAccessSlotCount);
    UnbindComputeConstantBuffers(context, 0, D3D11ComputeBindingSet::ConstantBufferSlotCount);
    UnbindComputeSamplers(context, 0, D3D11ComputeBindingSet::SamplerSlotCount);
}

D3D11ScopedComputeBindings::D3D11ScopedComputeBindings(ID3D11DeviceContext* context, const D3D11ComputeBindingSet& bindings)
    : context_(context), restored_(false) {
    ValidateContext(context_, "D3D11ScopedComputeBindings");
    Capture(bindings);
    bindings.Bind(context_);
}

D3D11ScopedComputeBindings::~D3D11ScopedComputeBindings() noexcept { Restore(); }

D3D11ScopedComputeBindings::D3D11ScopedComputeBindings(D3D11ScopedComputeBindings&& other) noexcept { MoveFrom(other); }

D3D11ScopedComputeBindings& D3D11ScopedComputeBindings::operator=(D3D11ScopedComputeBindings&& other) noexcept {
    if (this != &other) {
        Restore();
        ClearCapturedState();
        MoveFrom(other);
    }
    return *this;
}

void D3D11ScopedComputeBindings::Capture(const D3D11ComputeBindingSet& bindings) {
    srvRange_ = bindings.srvRange_;
    uavRange_ = bindings.uavRange_;
    constantBufferRange_ = bindings.constantBufferRange_;
    samplerRange_ = bindings.samplerRange_;

    if (!srvRange_.Empty()) {
        std::array<ID3D11ShaderResourceView*, D3D11ComputeBindingSet::ShaderResourceSlotCount> captured = {};
        context_->CSGetShaderResources(srvRange_.first, srvRange_.count, captured.data());
        for (UINT i = 0; i < srvRange_.count; ++i) {
            srvs_[srvRange_.first + i].Attach(captured[i]);
        }
    }
    if (!uavRange_.Empty()) {
        std::array<ID3D11UnorderedAccessView*, D3D11ComputeBindingSet::UnorderedAccessSlotCount> captured = {};
        context_->CSGetUnorderedAccessViews(uavRange_.first, uavRange_.count, captured.data());
        for (UINT i = 0; i < uavRange_.count; ++i) {
            uavs_[uavRange_.first + i].Attach(captured[i]);
        }
    }
    if (!constantBufferRange_.Empty()) {
        std::array<ID3D11Buffer*, D3D11ComputeBindingSet::ConstantBufferSlotCount> captured = {};
        context_->CSGetConstantBuffers(constantBufferRange_.first, constantBufferRange_.count, captured.data());
        for (UINT i = 0; i < constantBufferRange_.count; ++i) {
            constantBuffers_[constantBufferRange_.first + i].Attach(captured[i]);
        }
    }
    if (!samplerRange_.Empty()) {
        std::array<ID3D11SamplerState*, D3D11ComputeBindingSet::SamplerSlotCount> captured = {};
        context_->CSGetSamplers(samplerRange_.first, samplerRange_.count, captured.data());
        for (UINT i = 0; i < samplerRange_.count; ++i) {
            samplers_[samplerRange_.first + i].Attach(captured[i]);
        }
    }
}

void D3D11ScopedComputeBindings::Restore() noexcept {
    if (restored_ || !context_) return;

    if (!srvRange_.Empty()) {
        std::array<ID3D11ShaderResourceView*, D3D11ComputeBindingSet::ShaderResourceSlotCount> values = {};
        for (UINT i = 0; i < srvRange_.count; ++i) values[i] = srvs_[srvRange_.first + i].Get();
        context_->CSSetShaderResources(srvRange_.first, srvRange_.count, values.data());
    }
    if (!uavRange_.Empty()) {
        std::array<ID3D11UnorderedAccessView*, D3D11ComputeBindingSet::UnorderedAccessSlotCount> values = {};
        for (UINT i = 0; i < uavRange_.count; ++i) values[i] = uavs_[uavRange_.first + i].Get();
        context_->CSSetUnorderedAccessViews(uavRange_.first, uavRange_.count, values.data(), nullptr);
    }
    if (!constantBufferRange_.Empty()) {
        std::array<ID3D11Buffer*, D3D11ComputeBindingSet::ConstantBufferSlotCount> values = {};
        for (UINT i = 0; i < constantBufferRange_.count; ++i) values[i] = constantBuffers_[constantBufferRange_.first + i].Get();
        context_->CSSetConstantBuffers(constantBufferRange_.first, constantBufferRange_.count, values.data());
    }
    if (!samplerRange_.Empty()) {
        std::array<ID3D11SamplerState*, D3D11ComputeBindingSet::SamplerSlotCount> values = {};
        for (UINT i = 0; i < samplerRange_.count; ++i) values[i] = samplers_[samplerRange_.first + i].Get();
        context_->CSSetSamplers(samplerRange_.first, samplerRange_.count, values.data());
    }

    restored_ = true;
    ClearCapturedState();
}

void D3D11ScopedComputeBindings::MoveFrom(D3D11ScopedComputeBindings& other) noexcept {
    context_ = other.context_;
    restored_ = other.restored_;
    srvRange_ = other.srvRange_;
    uavRange_ = other.uavRange_;
    constantBufferRange_ = other.constantBufferRange_;
    samplerRange_ = other.samplerRange_;
    srvs_ = std::move(other.srvs_);
    uavs_ = std::move(other.uavs_);
    constantBuffers_ = std::move(other.constantBuffers_);
    samplers_ = std::move(other.samplers_);

    other.context_ = nullptr;
    other.restored_ = true;
    other.srvRange_.Clear();
    other.uavRange_.Clear();
    other.constantBufferRange_.Clear();
    other.samplerRange_.Clear();
}

void D3D11ScopedComputeBindings::ClearCapturedState() noexcept {
    ResetComPtrRange(srvs_, srvRange_.first, srvRange_.count);
    ResetComPtrRange(uavs_, uavRange_.first, uavRange_.count);
    ResetComPtrRange(constantBuffers_, constantBufferRange_.first, constantBufferRange_.count);
    ResetComPtrRange(samplers_, samplerRange_.first, samplerRange_.count);
}

} // namespace D3D11CoreLib

