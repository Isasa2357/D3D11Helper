# D3D11Foundation

`D3D11Foundation` は、D3D11 device / context を所有しない基礎 utility モジュールです。

## 含まれるヘッダ

```cpp
#include <D3D11Helper/D3D11Foundation/D3D11Foundation.hpp>
```

個別 include:

```cpp
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Foundation/DxgiUtil.hpp>
#include <D3D11Helper/D3D11Foundation/DxgiAdapterSelector.hpp>
#include <D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp>
```

## 役割

- Windows / D3D11 / DXGI の共通 include
- `ComPtr<T>` エイリアス
- HRESULT 例外化
- LUID utility
- DXGI adapter 選択
- `DXGI_FORMAT` trait

## 互換パス

v1.x では旧パスも wrapper として残します。

```cpp
#include <D3D11Helper/D3D11Core/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Core/DxgiUtil.hpp>
#include <D3D11Helper/D3D11Core/DxgiAdapterSelector.hpp>
#include <D3D11Helper/D3D11Core/D3D11FormatUtil.hpp>
```

新規コードでは `D3D11Foundation/` を推奨します。
