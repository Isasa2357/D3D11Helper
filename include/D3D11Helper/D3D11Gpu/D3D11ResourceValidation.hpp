#pragma once
//
// D3D11ResourceValidation.hpp
// Aggregate and throwing Texture2D validation for owned or external resources.
//
#include <D3D11Helper/D3D11Gpu/D3D11ResourceView.hpp>

#include <limits>
#include <string>
#include <vector>

namespace D3D11CoreLib {

struct D3D11Texture2DRequirement {
    ID3D11Device* device = nullptr;

    UINT width = 0;
    UINT height = 0;
    UINT widthMultiple = 0;
    UINT heightMultiple = 0;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mipLevels = 0;
    UINT arraySize = 0;
    UINT sampleCount = 0;
    UINT sampleQuality = (std::numeric_limits<UINT>::max)();

    bool checkUsage = false;
    D3D11_USAGE usage = D3D11_USAGE_DEFAULT;

    UINT requiredBindFlags = 0;
    UINT forbiddenBindFlags = 0;
    UINT requiredCpuAccessFlags = 0;
    UINT forbiddenCpuAccessFlags = 0;
    UINT requiredMiscFlags = 0;
    UINT forbiddenMiscFlags = 0;
};

struct D3D11ValidationResult {
    std::vector<std::string> errors;

    bool IsValid() const noexcept { return errors.empty(); }
    explicit operator bool() const noexcept { return IsValid(); }
    std::string Message() const;
};

D3D11ValidationResult ValidateTexture2DView(
    D3D11ResourceView resource,
    const D3D11Texture2DRequirement& requirement = {});

void ValidateTexture2DViewOrThrow(
    D3D11ResourceView resource,
    const D3D11Texture2DRequirement& requirement = {},
    const char* argumentName = "resource");

inline D3D11ValidationResult ValidateTexture2D(
    const D3D11Resource& resource,
    const D3D11Texture2DRequirement& requirement = {}) {
    return ValidateTexture2DView(D3D11ResourceView(resource), requirement);
}

inline void ValidateTexture2DOrThrow(
    const D3D11Resource& resource,
    const D3D11Texture2DRequirement& requirement = {},
    const char* argumentName = "resource") {
    ValidateTexture2DViewOrThrow(D3D11ResourceView(resource), requirement, argumentName);
}

} // namespace D3D11CoreLib
