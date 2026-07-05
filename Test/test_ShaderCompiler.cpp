#include "TestCommon.hpp"

#include <filesystem>
#include <fstream>

using namespace D3D11CoreLib;

TEST(ShaderCompiler, CompileFromSourceWithDefines) {
    const char* hlsl = R"(
#ifndef VALUE
#error VALUE missing
#endif
[numthreads(1,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint v = VALUE + id.x;
}
)";

    ShaderCompileDesc desc;
    desc.entryPoint = "main";
    desc.target = "cs_5_0";
    desc.defines.push_back({ "VALUE", "7" });

    ShaderBytecode bytecode = CompileShaderFromSource(hlsl, desc, "define_test.hlsl");
    CHECK(!bytecode.Empty());
}

TEST(ShaderCompiler, CompileFromFileWithIncludeDirs) {
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() / "d3d11helper_shadercompiler_test";
    const fs::path includeDir = root / "include";
    fs::create_directories(includeDir);

    const fs::path includePath = includeDir / "common.hlsli";
    const fs::path shaderPath = root / "main.hlsl";

    {
        std::ofstream ofs(includePath, std::ios::binary);
        ofs << "uint MakeValue() { return VALUE; }\n";
    }
    {
        std::ofstream ofs(shaderPath, std::ios::binary);
        ofs << "#include \"common.hlsli\"\n"
               "[numthreads(1,1,1)]\n"
               "void main(uint3 id : SV_DispatchThreadID) { uint v = MakeValue() + id.x; }\n";
    }

    ShaderCompileDesc desc;
    desc.sourcePath = shaderPath;
    desc.entryPoint = "main";
    desc.target = "cs_5_0";
    desc.includeDirs.push_back(includeDir);
    desc.defines.push_back({ "VALUE", "11" });

    ShaderBytecode bytecode = CompileShaderFromFile(desc);
    CHECK(!bytecode.Empty());

    fs::remove_all(root);
}

TEST(ShaderCompiler, InvalidInputsThrow) {
    ShaderCompileDesc desc;
    desc.entryPoint = "main";
    desc.target = "cs_5_0";

    CHECK_THROWS(CompileShaderFromFile(desc));

    ShaderCompileDesc noEntry;
    noEntry.target = "cs_5_0";
    CHECK_THROWS(CompileShaderFromSource("[numthreads(1,1,1)] void main() {}", noEntry));
}
