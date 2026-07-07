# D3D11Mipmap

`D3D11Mipmap` は、D3D11 の automatic mip generation を扱う helper です。

これは `ID3D11DeviceContext::GenerateMips()` を使うための薄い wrapper であり、独自 mipmap shader や画像処理 pipeline ではありません。

## Include

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Mipmap.hpp>
```

`D3D11Gpu.hpp` からも include されます。

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

---

## Types

```cpp
struct D3D11MipGenerationSrvDesc {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mostDetailedMip = 0;
    UINT mipLevels = UINT(-1);
};
```

`format == DXGI_FORMAT_UNKNOWN` の場合は texture の format を使います。`mipLevels == UINT(-1)` は SRV の残り mip chain を表します。

---

## Capability checks

```cpp
bool IsAutoGenerateMipsSupported(ID3D11Device* device, DXGI_FORMAT format);
bool CanGenerateMipsForTexture2D(ID3D11Device* device, ID3D11Texture2D* texture);
```

`CanGenerateMipsForTexture2D()` は以下を確認します。

- `D3D11_BIND_SHADER_RESOURCE`
- `D3D11_BIND_RENDER_TARGET`
- `D3D11_RESOURCE_MISC_GENERATE_MIPS`
- `MipLevels > 1`
- `SampleDesc.Count == 1`
- format が `D3D11_FORMAT_SUPPORT_MIP_AUTOGEN` に対応

---

## SRV creation

```cpp
ComPtr<ID3D11ShaderResourceView> CreateMipGenerationTexture2DSRV(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11MipGenerationSrvDesc& desc = {});
```

`CreateMipGenerationTexture2DSRV()` は `GenerateMips()` 用の `Texture2D` SRV を作成します。

主な validation:

- device / texture が null ではない
- texture が mip generation 用 flag を満たす
- `mostDetailedMip` が範囲内
- `mipLevels` が範囲内
- SRV format が `DXGI_FORMAT_UNKNOWN` ではない
- SRV format が automatic mip generation に対応

---

## GenerateMips

```cpp
void GenerateMips(ID3D11DeviceContext* context, ID3D11ShaderResourceView* srv);
```

`GenerateMips()` は D3D11 の `ID3D11DeviceContext::GenerateMips()` を呼びます。

---

## Example

```cpp
if (CanGenerateMipsForTexture2D(core->GetDevice(), texture.Get())) {
    auto srv = CreateMipGenerationTexture2DSRV(core->GetDevice(), texture.Get());
    GenerateMips(core->GetImmediateContext(), srv.Get());
}
```

texture 作成時には、少なくとも以下が必要です。

```cpp
BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
MipLevels > 1;
SampleDesc.Count = 1;
```

---

## Scope

対応:

- automatic mip generation capability check
- `Texture2D` eligibility check
- `Texture2D` SRV creation for mip generation
- `GenerateMips()` wrapper

非対応:

- custom mipmap shader
- compute-shader mip generation
- Cubemap / Texture3D convenience helper
- offline image pyramid generation
- file I/O
