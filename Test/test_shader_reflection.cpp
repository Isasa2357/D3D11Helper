#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Gpu/D3D11ShaderReflection.hpp>

using namespace D3D11CoreLib;

namespace {

const char* kVertexShader = R"(
cbuffer CameraConstants : register(b0)
{
    float4x4 viewProj;
};

struct VSIn {
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
    uint instanceId : INSTANCE_ID;
};

struct VSOut {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

VSOut main(VSIn input) {
    VSOut output;
    output.position = mul(float4(input.position, 1.0f), viewProj);
    output.texcoord = input.texcoord;
    return output;
}
)";

const char* kPixelShader = R"(
cbuffer MaterialConstants : register(b0)
{
    float4 tint;
    float gain;
};

Texture2D gTexture : register(t3);
SamplerState gSampler : register(s1);

float4 main(float2 texcoord : TEXCOORD0) : SV_Target {
    return gTexture.Sample(gSampler, texcoord) * tint * gain;
}
)";

const char* kComputeShader = R"(
RWStructuredBuffer<uint> gOut : register(u0);
[numthreads(8,4,2)]
void main(uint3 id : SV_DispatchThreadID) {
    gOut[id.x] = id.x;
}
)";

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    TEST_RUN("Reflect resource bindings and constant buffers", {
        auto bytecode = CompileShaderFromSource_D3DCompile(kPixelShader, "main", "ps_5_0");
        auto reflection = ReflectShaderBytecode(bytecode);

        Require(reflection.boundResourceCount >= 3, "expected at least 3 bound resources");
        Require(reflection.inputParameterCount == 1, "expected one input parameter");
        Require(reflection.outputParameterCount >= 1, "expected output parameters");

        const auto* cb = FindConstantBuffer(reflection, "MaterialConstants");
        Require(cb != nullptr, "MaterialConstants not found");
        Require(cb->sizeBytes >= 20, "constant buffer size too small");
        Require(FindConstantBufferVariable(*cb, "tint") != nullptr, "tint variable not found");
        Require(FindConstantBufferVariable(*cb, "gain") != nullptr, "gain variable not found");

        const auto* texture = FindResourceBinding(reflection, "gTexture");
        Require(texture != nullptr, "gTexture not found");
        Require(texture->type == D3D_SIT_TEXTURE, "gTexture type mismatch");
        Require(texture->bindPoint == 3, "gTexture bind point mismatch");
        Require(texture->space == 0, "D3D11 binding space should be zero");

        const auto* sampler = FindResourceBinding(reflection, "gSampler");
        Require(sampler != nullptr, "gSampler not found");
        Require(sampler->type == D3D_SIT_SAMPLER, "gSampler type mismatch");
        Require(sampler->bindPoint == 1, "gSampler bind point mismatch");
    });

    TEST_RUN("Build input layout elements", {
        auto bytecode = CompileShaderFromSource_D3DCompile(kVertexShader, "main", "vs_5_0");
        auto reflection = ReflectShaderBytecode(bytecode);

        auto elements = MakeInputLayoutElementsFromReflection(reflection);
        Require(elements.size() == 3, "input element count mismatch");

        Require(elements[0].semanticName == "POSITION", "POSITION semantic mismatch");
        Require(elements[0].format == DXGI_FORMAT_R32G32B32_FLOAT, "POSITION format mismatch");

        Require(elements[1].semanticName == "TEXCOORD", "TEXCOORD semantic mismatch");
        Require(elements[1].format == DXGI_FORMAT_R32G32_FLOAT, "TEXCOORD format mismatch");

        Require(elements[2].semanticName == "INSTANCE_ID", "INSTANCE_ID semantic mismatch");
        Require(elements[2].format == DXGI_FORMAT_R32_UINT, "INSTANCE_ID format mismatch");

        auto d3dDescs = MakeD3D11InputElementDescs(elements);
        Require(d3dDescs.size() == elements.size(), "D3D11 descriptor count mismatch");
        Require(d3dDescs[0].SemanticName != nullptr, "D3D11 semantic name is null");
        Require(d3dDescs[0].AlignedByteOffset == D3D11_APPEND_ALIGNED_ELEMENT, "aligned offset mismatch");
    });

    TEST_RUN("Reflect compute thread group size", {
        auto bytecode = CompileShaderFromSource_D3DCompile(kComputeShader, "main", "cs_5_0");
        auto reflection = ReflectShaderBytecode(bytecode);

        Require(reflection.threadGroupSizeX == 8, "thread group x mismatch");
        Require(reflection.threadGroupSizeY == 4, "thread group y mismatch");
        Require(reflection.threadGroupSizeZ == 2, "thread group z mismatch");

        const auto* out = FindResourceBinding(reflection, "gOut");
        Require(out != nullptr, "gOut not found");
        Require(out->type == D3D_SIT_UAV_RWSTRUCTURED, "gOut type mismatch");
        Require(out->bindPoint == 0, "gOut bind point mismatch");
    });

    TEST_RUN("Throw on empty bytecode", {
        bool ok = false;
        try {
            ReflectShaderBytecode(nullptr, 0);
        } catch (...) {
            ok = true;
        }
        Require(ok, "empty pointer reflection did not throw");

        ok = false;
        try {
            ShaderBytecode empty;
            ReflectShaderBytecode(empty);
        } catch (...) {
            ok = true;
        }
        Require(ok, "empty ShaderBytecode reflection did not throw");
    });

    return TestUtil::Result("ShaderReflection");
}
