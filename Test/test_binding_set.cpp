#include "TestUtil.hpp"
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

using namespace D3D11CoreLib;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Fn>
void ExpectThrows(const char* message, Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (...) {
        threw = true;
    }
    if (!threw) {
        throw std::runtime_error(message);
    }
}

ComPtr<ID3D11Buffer> CreateConstantBuffer(ID3D11Device* device) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = 16;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> buffer;
    ThrowIfFailed(device->CreateBuffer(&desc, nullptr, buffer.GetAddressOf()));
    return buffer;
}

ComPtr<ID3D11Buffer> CreateStructuredBuffer(ID3D11Device* device) {
    const UINT values[4] = { 1, 2, 3, 4 };

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(values);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(UINT);

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = values;

    ComPtr<ID3D11Buffer> buffer;
    ThrowIfFailed(device->CreateBuffer(&desc, &data, buffer.GetAddressOf()));
    return buffer;
}

ComPtr<ID3D11ShaderResourceView> CreateStructuredSrv(ID3D11Device* device, ID3D11Buffer* buffer) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;
    desc.Buffer.NumElements = 4;

    ComPtr<ID3D11ShaderResourceView> srv;
    ThrowIfFailed(device->CreateShaderResourceView(buffer, &desc, srv.GetAddressOf()));
    return srv;
}

ComPtr<ID3D11UnorderedAccessView> CreateStructuredUav(ID3D11Device* device, ID3D11Buffer* buffer) {
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;
    desc.Buffer.NumElements = 4;

    ComPtr<ID3D11UnorderedAccessView> uav;
    ThrowIfFailed(device->CreateUnorderedAccessView(buffer, &desc, uav.GetAddressOf()));
    return uav;
}

ComPtr<ID3D11SamplerState> CreateSampler(ID3D11Device* device, D3D11_FILTER filter) {
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = filter;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MaxAnisotropy = (filter == D3D11_FILTER_ANISOTROPIC) ? 4u : 1u;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> sampler;
    ThrowIfFailed(device->CreateSamplerState(&desc, sampler.GetAddressOf()));
    return sampler;
}

ComPtr<ID3D11ShaderResourceView> GetSrv(ID3D11DeviceContext* context, UINT slot) {
    ID3D11ShaderResourceView* raw = nullptr;
    context->CSGetShaderResources(slot, 1, &raw);
    ComPtr<ID3D11ShaderResourceView> value;
    value.Attach(raw);
    return value;
}

ComPtr<ID3D11UnorderedAccessView> GetUav(ID3D11DeviceContext* context, UINT slot) {
    ID3D11UnorderedAccessView* raw = nullptr;
    context->CSGetUnorderedAccessViews(slot, 1, &raw);
    ComPtr<ID3D11UnorderedAccessView> value;
    value.Attach(raw);
    return value;
}

ComPtr<ID3D11Buffer> GetCb(ID3D11DeviceContext* context, UINT slot) {
    ID3D11Buffer* raw = nullptr;
    context->CSGetConstantBuffers(slot, 1, &raw);
    ComPtr<ID3D11Buffer> value;
    value.Attach(raw);
    return value;
}

ComPtr<ID3D11SamplerState> GetSampler(ID3D11DeviceContext* context, UINT slot) {
    ID3D11SamplerState* raw = nullptr;
    context->CSGetSamplers(slot, 1, &raw);
    ComPtr<ID3D11SamplerState> value;
    value.Attach(raw);
    return value;
}

void TestEmptyAndClear() {
    D3D11ComputeBindingSet set;
    Require(set.Empty(), "new set should be empty");

    set.SetShaderResource(2, nullptr);
    set.SetUnorderedAccess(1, nullptr);
    set.SetConstantBuffer(3, nullptr);
    set.SetSampler(4, nullptr);
    Require(!set.Empty(), "touched null slots should still define binding ranges");

    set.Clear();
    Require(set.Empty(), "Clear should reset all ranges");
}

void TestSlotValidation() {
    D3D11ComputeBindingSet set;
    ExpectThrows("SRV slot should throw", [&] {
        set.SetShaderResource(D3D11ComputeBindingSet::ShaderResourceSlotCount, nullptr);
    });
    ExpectThrows("UAV slot should throw", [&] {
        set.SetUnorderedAccess(D3D11ComputeBindingSet::UnorderedAccessSlotCount, nullptr);
    });
    ExpectThrows("CB slot should throw", [&] {
        set.SetConstantBuffer(D3D11ComputeBindingSet::ConstantBufferSlotCount, nullptr);
    });
    ExpectThrows("Sampler slot should throw", [&] {
        set.SetSampler(D3D11ComputeBindingSet::SamplerSlotCount, nullptr);
    });
}

void TestHelperRangeValidation(ID3D11DeviceContext* context) {
    ID3D11ShaderResourceView* const* nullSrvs = nullptr;
    ID3D11UnorderedAccessView* const* nullUavs = nullptr;
    ID3D11Buffer* const* nullBuffers = nullptr;
    ID3D11SamplerState* const* nullSamplers = nullptr;
    ID3D11ShaderResourceView* srvNull = nullptr;
    ID3D11UnorderedAccessView* uavNull = nullptr;
    ID3D11Buffer* bufferNull = nullptr;
    ID3D11SamplerState* samplerNull = nullptr;

    BindComputeShaderResources(context, 0, 0, nullSrvs);
    BindComputeUnorderedAccessViews(context, 0, 0, nullUavs, nullptr);
    BindComputeConstantBuffers(context, 0, 0, nullBuffers);
    BindComputeSamplers(context, 0, 0, nullSamplers);

    ExpectThrows("null context should throw", [&] {
        BindComputeShaderResources(nullptr, 0, 0, nullSrvs);
    });
    ExpectThrows("non-zero null SRV array should throw", [&] {
        BindComputeShaderResources(context, 0, 1, nullSrvs);
    });
    ExpectThrows("out-of-range SRV should throw", [&] {
        BindComputeShaderResources(context, D3D11ComputeBindingSet::ShaderResourceSlotCount, 1, &srvNull);
    });
    ExpectThrows("overflow SRV range should throw", [&] {
        BindComputeShaderResources(context, UINT(-1), 2, &srvNull);
    });
    ExpectThrows("non-zero null UAV array should throw", [&] {
        BindComputeUnorderedAccessViews(context, 0, 1, nullUavs, nullptr);
    });
    ExpectThrows("out-of-range UAV should throw", [&] {
        BindComputeUnorderedAccessViews(context, D3D11ComputeBindingSet::UnorderedAccessSlotCount, 1, &uavNull, nullptr);
    });
    ExpectThrows("non-zero null CB array should throw", [&] {
        BindComputeConstantBuffers(context, 0, 1, nullBuffers);
    });
    ExpectThrows("out-of-range CB should throw", [&] {
        BindComputeConstantBuffers(context, D3D11ComputeBindingSet::ConstantBufferSlotCount, 1, &bufferNull);
    });
    ExpectThrows("non-zero null sampler array should throw", [&] {
        BindComputeSamplers(context, 0, 1, nullSamplers);
    });
    ExpectThrows("out-of-range sampler should throw", [&] {
        BindComputeSamplers(context, D3D11ComputeBindingSet::SamplerSlotCount, 1, &samplerNull);
    });
}

void TestContiguousRangesBindHolesAsNull(ID3D11Device* device, ID3D11DeviceContext* context) {
    D3D11UnbindComputeResources(context);

    auto buffer = CreateStructuredBuffer(device);
    auto srv = CreateStructuredSrv(device, buffer.Get());

    D3D11ComputeBindingSet set;
    set.SetShaderResource(2, srv.Get());
    set.SetShaderResource(5, srv.Get());
    set.Bind(context);

    Require(GetSrv(context, 2).Get() == srv.Get(), "slot 2 should be bound");
    Require(GetSrv(context, 3).Get() == nullptr, "slot 3 hole should be null");
    Require(GetSrv(context, 4).Get() == nullptr, "slot 4 hole should be null");
    Require(GetSrv(context, 5).Get() == srv.Get(), "slot 5 should be bound");
}

void TestBindAndUnbindWithNullEntries(ID3D11Device* device, ID3D11DeviceContext* context) {
    D3D11UnbindComputeResources(context);

    auto cb = CreateConstantBuffer(device);
    auto sampler = CreateSampler(device, D3D11_FILTER_MIN_MAG_MIP_POINT);

    D3D11ComputeBindingSet set;
    set.SetConstantBuffer(0, cb.Get());
    set.SetConstantBuffer(2, nullptr);
    set.SetSampler(0, sampler.Get());
    set.SetSampler(2, nullptr);
    set.Bind(context);

    Require(GetCb(context, 0).Get() == cb.Get(), "CB slot 0 should be bound");
    Require(GetCb(context, 1).Get() == nullptr, "CB slot 1 hole should be null");
    Require(GetSampler(context, 0).Get() == sampler.Get(), "sampler slot 0 should be bound");
    Require(GetSampler(context, 1).Get() == nullptr, "sampler slot 1 hole should be null");

    set.Unbind(context);
    Require(GetCb(context, 0).Get() == nullptr, "CB slot 0 should be unbound");
    Require(GetSampler(context, 0).Get() == nullptr, "sampler slot 0 should be unbound");
}

void TestGlobalUnbind(ID3D11Device* device, ID3D11DeviceContext* context) {
    auto srvBuffer = CreateStructuredBuffer(device);
    auto uavBuffer = CreateStructuredBuffer(device);
    auto srv = CreateStructuredSrv(device, srvBuffer.Get());
    auto uav = CreateStructuredUav(device, uavBuffer.Get());
    auto cb = CreateConstantBuffer(device);
    auto sampler = CreateSampler(device, D3D11_FILTER_MIN_MAG_MIP_LINEAR);

    D3D11ComputeBindingSet set;
    set.SetShaderResource(0, srv.Get());
    set.SetUnorderedAccess(0, uav.Get());
    set.SetConstantBuffer(0, cb.Get());
    set.SetSampler(0, sampler.Get());
    set.Bind(context);

    D3D11UnbindComputeResources(context);
    Require(GetSrv(context, 0).Get() == nullptr, "SRV should be unbound");
    Require(GetUav(context, 0).Get() == nullptr, "UAV should be unbound");
    Require(GetCb(context, 0).Get() == nullptr, "CB should be unbound");
    Require(GetSampler(context, 0).Get() == nullptr, "sampler should be unbound");
}

void TestScopedRestoreAllComputeResources(ID3D11Device* device, ID3D11DeviceContext* context) {
    D3D11UnbindComputeResources(context);

    auto srvBufferA = CreateStructuredBuffer(device);
    auto srvBufferB = CreateStructuredBuffer(device);
    auto uavBufferA = CreateStructuredBuffer(device);
    auto uavBufferB = CreateStructuredBuffer(device);
    auto srvA = CreateStructuredSrv(device, srvBufferA.Get());
    auto srvB = CreateStructuredSrv(device, srvBufferB.Get());
    auto uavA = CreateStructuredUav(device, uavBufferA.Get());
    auto uavB = CreateStructuredUav(device, uavBufferB.Get());
    auto cbA = CreateConstantBuffer(device);
    auto cbB = CreateConstantBuffer(device);
    auto samplerA = CreateSampler(device, D3D11_FILTER_MIN_MAG_MIP_POINT);
    auto samplerB = CreateSampler(device, D3D11_FILTER_ANISOTROPIC);

    D3D11ComputeBindingSet original;
    original.SetShaderResource(1, srvA.Get());
    original.SetUnorderedAccess(1, uavA.Get());
    original.SetConstantBuffer(1, cbA.Get());
    original.SetSampler(1, samplerA.Get());
    original.Bind(context);

    D3D11ComputeBindingSet replacement;
    replacement.SetShaderResource(1, srvB.Get());
    replacement.SetUnorderedAccess(1, uavB.Get());
    replacement.SetConstantBuffer(1, cbB.Get());
    replacement.SetSampler(1, samplerB.Get());

    {
        D3D11ScopedComputeBindings scoped(context, replacement);
        Require(GetSrv(context, 1).Get() == srvB.Get(), "scoped SRV should be bound");
        Require(GetUav(context, 1).Get() == uavB.Get(), "scoped UAV should be bound");
        Require(GetCb(context, 1).Get() == cbB.Get(), "scoped CB should be bound");
        Require(GetSampler(context, 1).Get() == samplerB.Get(), "scoped sampler should be bound");
    }

    Require(GetSrv(context, 1).Get() == srvA.Get(), "SRV should be restored");
    Require(GetUav(context, 1).Get() == uavA.Get(), "UAV view should be restored");
    Require(GetCb(context, 1).Get() == cbA.Get(), "CB should be restored");
    Require(GetSampler(context, 1).Get() == samplerA.Get(), "sampler should be restored");
}

void TestScopedRestoreIsIdempotent(ID3D11Device* device, ID3D11DeviceContext* context) {
    D3D11UnbindComputeResources(context);

    auto cbA = CreateConstantBuffer(device);
    auto cbB = CreateConstantBuffer(device);

    D3D11ComputeBindingSet original;
    original.SetConstantBuffer(2, cbA.Get());
    original.Bind(context);

    D3D11ComputeBindingSet replacement;
    replacement.SetConstantBuffer(2, cbB.Get());

    {
        D3D11ScopedComputeBindings scoped(context, replacement);
        scoped.Restore();
        scoped.Restore();
        Require(GetCb(context, 2).Get() == cbA.Get(), "Restore should be idempotent");
    }

    Require(GetCb(context, 2).Get() == cbA.Get(), "destructor after Restore should not change state");
}

void TestScopedMoveConstructionRestoresFromMovedToObject(ID3D11Device* device, ID3D11DeviceContext* context) {
    D3D11UnbindComputeResources(context);

    auto cbA = CreateConstantBuffer(device);
    auto cbB = CreateConstantBuffer(device);

    D3D11ComputeBindingSet original;
    original.SetConstantBuffer(3, cbA.Get());
    original.Bind(context);

    D3D11ComputeBindingSet replacement;
    replacement.SetConstantBuffer(3, cbB.Get());

    {
        D3D11ScopedComputeBindings temp(context, replacement);
        D3D11ScopedComputeBindings movedTo(std::move(temp));
        Require(GetCb(context, 3).Get() == cbB.Get(), "moved-to object should keep replacement bound");
    }

    Require(GetCb(context, 3).Get() == cbA.Get(), "moved-to object should restore original");
}

void TestScopedMoveAssignmentRestoresExactlyOnce(ID3D11Device* device, ID3D11DeviceContext* context) {
    D3D11UnbindComputeResources(context);

    auto cbA = CreateConstantBuffer(device);
    auto cbB = CreateConstantBuffer(device);

    D3D11ComputeBindingSet original;
    original.SetConstantBuffer(3, cbA.Get());
    original.Bind(context);

    D3D11ComputeBindingSet replacement;
    replacement.SetConstantBuffer(3, cbB.Get());

    {
        D3D11ScopedComputeBindings first(context, replacement);
        D3D11ScopedComputeBindings second(context, original);
        second = std::move(first);
        Require(GetCb(context, 3).Get() == cbB.Get(), "move assignment should restore overwritten scoped state first");
    }

    Require(GetCb(context, 3).Get() == cbA.Get(), "moved scoped state should restore original captured state exactly once");
}

} // namespace

int main() {
    auto core = TestUtil::MakeCore();
    ID3D11Device* device = core->GetDevice();
    ID3D11DeviceContext* context = core->GetImmediateContext();

    TestUtil::Run("Empty and Clear", [] {
        TestEmptyAndClear();
    });

    TestUtil::Run("Slot validation", [] {
        TestSlotValidation();
    });

    TestUtil::Run("Helper range validation", [&] {
        TestHelperRangeValidation(context);
    });

    TestUtil::Run("Contiguous ranges bind holes as null", [&] {
        TestContiguousRangesBindHolesAsNull(device, context);
    });

    TestUtil::Run("Bind and Unbind with null entries", [&] {
        TestBindAndUnbindWithNullEntries(device, context);
    });

    TestUtil::Run("D3D11UnbindComputeResources clears compute state", [&] {
        TestGlobalUnbind(device, context);
    });

    TestUtil::Run("Scoped bindings restore all compute resources", [&] {
        TestScopedRestoreAllComputeResources(device, context);
    });

    TestUtil::Run("Scoped Restore is idempotent", [&] {
        TestScopedRestoreIsIdempotent(device, context);
    });

    TestUtil::Run("Scoped move construction restores from moved-to object", [&] {
        TestScopedMoveConstructionRestoresFromMovedToObject(device, context);
    });

    TestUtil::Run("Scoped move assignment restores exactly once", [&] {
        TestScopedMoveAssignmentRestoresExactlyOnce(device, context);
    });

    D3D11UnbindComputeResources(context);
    core->Flush();
    return TestUtil::Result("BindingSet");
}
