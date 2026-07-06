//
// 20_ComputeBindingSet / main.cpp
//
// Minimal console sample for compute-stage binding helpers. The sample creates
// small WARP-compatible resources, binds them with D3D11ScopedComputeBindings,
// and explicitly unbinds compute state before exit.
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <iostream>
#include <stdexcept>

using namespace D3D11CoreLib;

namespace {

ComPtr<ID3D11Buffer> CreateConstantBuffer(ID3D11Device* device) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = 16;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ComPtr<ID3D11Buffer> buffer;
    ThrowIfFailed(device->CreateBuffer(&desc, nullptr, buffer.GetAddressOf()));
    return buffer;
}

ComPtr<ID3D11Buffer> CreateStructuredBuffer(ID3D11Device* device) {
    const UINT data[4] = { 1, 2, 3, 4 };

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = sizeof(data);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(UINT);

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = data;

    ComPtr<ID3D11Buffer> buffer;
    ThrowIfFailed(device->CreateBuffer(&desc, &init, buffer.GetAddressOf()));
    return buffer;
}

ComPtr<ID3D11ShaderResourceView> CreateSrv(ID3D11Device* device, ID3D11Buffer* buffer) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;
    desc.Buffer.NumElements = 4;

    ComPtr<ID3D11ShaderResourceView> srv;
    ThrowIfFailed(device->CreateShaderResourceView(buffer, &desc, srv.GetAddressOf()));
    return srv;
}

ComPtr<ID3D11UnorderedAccessView> CreateUav(ID3D11Device* device, ID3D11Buffer* buffer) {
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;
    desc.Buffer.NumElements = 4;

    ComPtr<ID3D11UnorderedAccessView> uav;
    ThrowIfFailed(device->CreateUnorderedAccessView(buffer, &desc, uav.GetAddressOf()));
    return uav;
}

ComPtr<ID3D11SamplerState> CreateSampler(ID3D11Device* device) {
    D3D11_SAMPLER_DESC desc{};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MaxAnisotropy = 1;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    ComPtr<ID3D11SamplerState> sampler;
    ThrowIfFailed(device->CreateSamplerState(&desc, sampler.GetAddressOf()));
    return sampler;
}

} // namespace

int main() {
    try {
        D3D11CoreConfig config;
        config.enableDebugLayer = false;
        config.enableInfoQueue = false;
        config.allowWarpAdapter = true;

        auto core = D3D11Core::CreateShared(config);
        auto* device = core->GetDevice();
        auto* context = core->GetImmediateContext();

        auto constantBuffer = CreateConstantBuffer(device);
        auto srvBuffer = CreateStructuredBuffer(device);
        auto uavBuffer = CreateStructuredBuffer(device);
        auto srv = CreateSrv(device, srvBuffer.Get());
        auto uav = CreateUav(device, uavBuffer.Get());
        auto sampler = CreateSampler(device);

        D3D11ComputeBindingSet bindings;
        bindings.SetShaderResource(0, srv.Get());
        bindings.SetUnorderedAccess(0, uav.Get());
        bindings.SetConstantBuffer(0, constantBuffer.Get());
        bindings.SetSampler(0, sampler.Get());

        {
            D3D11ScopedComputeBindings scoped(context, bindings);
            // No compute shader is bound in this minimal binding-focused sample,
            // so Dispatch(1, 1, 1) is intentionally omitted.
        }

        D3D11UnbindComputeResources(context);
        core->Flush();

        std::cout << "Compute binding set sample completed successfully." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
