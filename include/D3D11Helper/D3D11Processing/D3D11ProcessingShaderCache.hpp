#pragma once
//
// D3D11ProcessingShaderCache.hpp
// Runtime shader compiler/cache for D3D11 Processing HLSL files.
//
#include <D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ShaderCompiler.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace D3D11CoreLib {
namespace Processing {

class D3D11ProcessingShaderCache {
public:
    void Initialize(D3D11ProcessingContext& context);

    const ShaderBytecode& GetComputeShader(
        const std::string& fileName,
        const std::string& entryPoint = "main",
        const std::vector<ShaderMacro>& defines = {});

    void Clear();

private:
    std::string MakeKey(const std::string& fileName,
                        const std::string& entryPoint,
                        const std::vector<ShaderMacro>& defines) const;

    D3D11ProcessingContext* m_context = nullptr;
    std::unordered_map<std::string, ShaderBytecode> m_cache;
};

} // namespace Processing
} // namespace D3D11CoreLib
