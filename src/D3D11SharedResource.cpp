//
// D3D11SharedResource.cpp
//
#include <D3D11Helper/D3D11Interop/D3D11SharedResource.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <stdexcept>

namespace D3D11CoreLib {

HANDLE D3D11SharedResource::CreateSharedHandle(
    ID3D11Resource* resource,
    LPCWSTR name) {

    if (!resource) {
        throw std::runtime_error("D3D11SharedResource::CreateSharedHandle: null resource");
    }

    ComPtr<IDXGIResource1> dxgiResource;
    D3D11CORE_THROW_IF_FAILED_MSG(
        resource->QueryInterface(IID_PPV_ARGS(&dxgiResource)),
        "QueryInterface(IDXGIResource1) failed "
        "(resource must be created with D3D11_RESOURCE_MISC_SHARED_NTHANDLE)");

    HANDLE handle = nullptr;
    D3D11CORE_THROW_IF_FAILED_MSG(
        dxgiResource->CreateSharedHandle(nullptr, GENERIC_ALL, name, &handle),
        "CreateSharedHandle failed "
        "(resource must be created with SHARED_NTHANDLE | SHARED)");
    return handle;
}

D3D11SharedHandle D3D11SharedResource::CreateSharedHandleOwned(
    ID3D11Resource* resource,
    LPCWSTR name) {
    return D3D11SharedHandle(CreateSharedHandle(resource, name));
}

ComPtr<ID3D11Resource> D3D11SharedResource::OpenSharedHandle(
    ID3D11Device* device,
    HANDLE handle) {

    if (!device || !handle) {
        throw std::runtime_error("D3D11SharedResource::OpenSharedHandle: null argument");
    }

    ComPtr<ID3D11Device1> device1;
    D3D11CORE_THROW_IF_FAILED(device->QueryInterface(IID_PPV_ARGS(&device1)));

    ComPtr<ID3D11Resource> resource;
    D3D11CORE_THROW_IF_FAILED(
        device1->OpenSharedResource1(handle, IID_PPV_ARGS(&resource)));
    return resource;
}

ComPtr<ID3D11Texture2D> D3D11SharedResource::OpenSharedTexture2D(
    ID3D11Device* device,
    HANDLE handle) {

    ComPtr<ID3D11Resource> resource = OpenSharedHandle(device, handle);
    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED_MSG(
        resource.As(&texture),
        "OpenSharedTexture2D: resource is not a Texture2D");
    return texture;
}

} // namespace D3D11CoreLib
