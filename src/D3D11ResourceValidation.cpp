#include <D3D11Helper/D3D11Gpu/D3D11ResourceValidation.hpp>

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace D3D11CoreLib {
namespace {

std::string Hex(UINT value) {
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << value;
    return os.str();
}

void CheckRequiredFlags(
    std::vector<std::string>& errors,
    const char* label,
    UINT actual,
    UINT required) {
    if ((actual & required) != required) {
        std::ostringstream os;
        os << label << " missing required flags " << Hex(required)
           << " (actual " << Hex(actual) << ")";
        errors.push_back(os.str());
    }
}

void CheckForbiddenFlags(
    std::vector<std::string>& errors,
    const char* label,
    UINT actual,
    UINT forbidden) {
    const UINT present = actual & forbidden;
    if (present != 0) {
        std::ostringstream os;
        os << label << " contains forbidden flags " << Hex(present)
           << " (actual " << Hex(actual) << ")";
        errors.push_back(os.str());
    }
}

} // namespace

std::string D3D11ValidationResult::Message() const {
    std::ostringstream os;
    for (std::size_t i = 0; i < errors.size(); ++i) {
        if (i != 0) os << "; ";
        os << errors[i];
    }
    return os.str();
}

D3D11ValidationResult ValidateTexture2DView(
    D3D11ResourceView resource,
    const D3D11Texture2DRequirement& requirement) {

    D3D11ValidationResult result;
    if (!resource.Get()) {
        result.errors.emplace_back("resource is null");
        return result;
    }

    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource.Get()->GetType(&dimension);
    if (dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        result.errors.emplace_back("resource dimension is not Texture2D");
        return result;
    }

    ComPtr<ID3D11Texture2D> texture;
    const HRESULT queryResult = resource.Get()->QueryInterface(IID_PPV_ARGS(&texture));
    if (FAILED(queryResult) || !texture) {
        result.errors.emplace_back("resource could not be queried as ID3D11Texture2D");
        return result;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);

    if (requirement.device) {
        ComPtr<ID3D11Device> actualDevice;
        resource.Get()->GetDevice(&actualDevice);
        if (actualDevice.Get() != requirement.device) {
            result.errors.emplace_back("resource belongs to a different D3D11 device");
        }
    }

    if (requirement.width != 0 && desc.Width != requirement.width) {
        std::ostringstream os;
        os << "width mismatch: actual " << desc.Width
           << ", expected " << requirement.width;
        result.errors.push_back(os.str());
    }
    if (requirement.height != 0 && desc.Height != requirement.height) {
        std::ostringstream os;
        os << "height mismatch: actual " << desc.Height
           << ", expected " << requirement.height;
        result.errors.push_back(os.str());
    }
    if (requirement.widthMultiple != 0 &&
        (desc.Width % requirement.widthMultiple) != 0) {
        std::ostringstream os;
        os << "width " << desc.Width << " is not a multiple of "
           << requirement.widthMultiple;
        result.errors.push_back(os.str());
    }
    if (requirement.heightMultiple != 0 &&
        (desc.Height % requirement.heightMultiple) != 0) {
        std::ostringstream os;
        os << "height " << desc.Height << " is not a multiple of "
           << requirement.heightMultiple;
        result.errors.push_back(os.str());
    }

    if (requirement.format != DXGI_FORMAT_UNKNOWN &&
        desc.Format != requirement.format) {
        std::ostringstream os;
        os << "format mismatch: actual " << static_cast<UINT>(desc.Format)
           << ", expected " << static_cast<UINT>(requirement.format);
        result.errors.push_back(os.str());
    }
    if (requirement.mipLevels != 0 && desc.MipLevels != requirement.mipLevels) {
        std::ostringstream os;
        os << "mip level mismatch: actual " << desc.MipLevels
           << ", expected " << requirement.mipLevels;
        result.errors.push_back(os.str());
    }
    if (requirement.arraySize != 0 && desc.ArraySize != requirement.arraySize) {
        std::ostringstream os;
        os << "array size mismatch: actual " << desc.ArraySize
           << ", expected " << requirement.arraySize;
        result.errors.push_back(os.str());
    }
    if (requirement.sampleCount != 0 &&
        desc.SampleDesc.Count != requirement.sampleCount) {
        std::ostringstream os;
        os << "sample count mismatch: actual " << desc.SampleDesc.Count
           << ", expected " << requirement.sampleCount;
        result.errors.push_back(os.str());
    }
    if (requirement.sampleQuality != (std::numeric_limits<UINT>::max)() &&
        desc.SampleDesc.Quality != requirement.sampleQuality) {
        std::ostringstream os;
        os << "sample quality mismatch: actual " << desc.SampleDesc.Quality
           << ", expected " << requirement.sampleQuality;
        result.errors.push_back(os.str());
    }
    if (requirement.checkUsage && desc.Usage != requirement.usage) {
        std::ostringstream os;
        os << "usage mismatch: actual " << static_cast<UINT>(desc.Usage)
           << ", expected " << static_cast<UINT>(requirement.usage);
        result.errors.push_back(os.str());
    }

    CheckRequiredFlags(result.errors, "bind flags", desc.BindFlags,
        requirement.requiredBindFlags);
    CheckForbiddenFlags(result.errors, "bind flags", desc.BindFlags,
        requirement.forbiddenBindFlags);
    CheckRequiredFlags(result.errors, "CPU access flags", desc.CPUAccessFlags,
        requirement.requiredCpuAccessFlags);
    CheckForbiddenFlags(result.errors, "CPU access flags", desc.CPUAccessFlags,
        requirement.forbiddenCpuAccessFlags);
    CheckRequiredFlags(result.errors, "misc flags", desc.MiscFlags,
        requirement.requiredMiscFlags);
    CheckForbiddenFlags(result.errors, "misc flags", desc.MiscFlags,
        requirement.forbiddenMiscFlags);

    return result;
}

void ValidateTexture2DViewOrThrow(
    D3D11ResourceView resource,
    const D3D11Texture2DRequirement& requirement,
    const char* argumentName) {

    const auto result = ValidateTexture2DView(resource, requirement);
    if (!result.IsValid()) {
        std::ostringstream os;
        os << (argumentName ? argumentName : "resource")
           << ": " << result.Message();
        throw std::invalid_argument(os.str());
    }
}

} // namespace D3D11CoreLib
