#include "D3D11Processing/D3D11ProcessingShaderCache.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace D3D11CoreLib {
namespace Processing {

void D3D11ProcessingShaderCache::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_cache.clear();
}

const ShaderBytecode& D3D11ProcessingShaderCache::GetComputeShader(
    const std::string& fileName,
    const std::string& entryPoint,
    const std::vector<ShaderMacro>& defines) {

    if (!m_context) {
        throw ValidationError("D3D11ProcessingShaderCache::GetComputeShader: cache is not initialized");
    }

    const std::string key = MakeKey(fileName, entryPoint, defines);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return it->second;
    }

    const auto path = m_context->ShaderDirectory() / fileName;
    if (!std::filesystem::exists(path)) {
        std::ostringstream os;
        os << "D3D11ProcessingShaderCache::GetComputeShader: shader file not found: " << path.string();
        throw ValidationError(os.str());
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        std::ostringstream os;
        os << "D3D11ProcessingShaderCache::GetComputeShader: cannot open shader file: " << path.string();
        throw ValidationError(os.str());
    }
    std::ostringstream sourceStream;
    sourceStream << ifs.rdbuf();
    const std::string source = sourceStream.str();

    ShaderCompileDesc desc = {};
    desc.sourcePath = path;
    desc.entryPoint = entryPoint;
    desc.target = "cs_5_0";
    desc.includeDirs.push_back(m_context->ShaderDirectory());
    desc.defines = defines;
    desc.useDxc = false;

    auto bytecode = CompileShaderFromSource(source, desc, fileName);
    auto inserted = m_cache.emplace(key, std::move(bytecode));
    return inserted.first->second;
}

void D3D11ProcessingShaderCache::Clear() {
    m_cache.clear();
}

std::string D3D11ProcessingShaderCache::MakeKey(
    const std::string& fileName,
    const std::string& entryPoint,
    const std::vector<ShaderMacro>& defines) const {

    std::ostringstream os;
    os << fileName << "|" << entryPoint;
    for (const auto& d : defines) {
        os << "|" << d.name << "=" << d.value;
    }
    return os.str();
}

} // namespace Processing
} // namespace D3D11CoreLib
