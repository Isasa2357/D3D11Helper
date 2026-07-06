# D3D11Diagnostics

`D3D11Diagnostics` は、D3D11 Debug Layer / InfoQueue / LiveObject report などの診断機能を扱います。

## Include

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
```

## D3D11Debug

```cpp
void SetupInfoQueue(
    ID3D11Device* device,
    bool breakOnError,
    bool breakOnCorruption,
    bool breakOnWarning);

void ReportLiveObjects(ID3D11Device* device);

template <typename T>
void SetDebugName(T* object, const char* name);
```

## 今後の拡張予定

後続バージョンでは、次を追加する予定です。

- GPU timer
- frame profiler
- device removed reason helper
- InfoQueue message dump
- scoped debug marker

## 互換パス

旧パスも v1.x では維持されます。

```cpp
#include <D3D11Helper/D3D11Core/D3D11Debug.hpp>
```
