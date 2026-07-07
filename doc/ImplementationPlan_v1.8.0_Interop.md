# ImplementationPlan v1.8.0 - D3D11Interop

## 前提

この計画は `main` の `v1.7.0` を基準にする。
実装段階では `CMakeLists.txt` の version は `1.7.0` のままにする。
`1.8.0` への version bump、release notes、migration guide、README 更新は実装完了後に別 patch で行う。

## Branch

推奨 branch:

```bat
git switch main
git pull

git switch -c feature/v1.8.0-interop
git push -u origin feature/v1.8.0-interop
```

実装をさらに分ける場合:

```text
feature/v1.8.0-interop
  round1/shared
  round2/keyed-mutex
  round3/fence-sample
```

ただし、実装 patch は feature branch 上に順番に適用してよい。

---

# Round 1 - SharedHandle / SharedTexture

## 目的

HANDLE ownership と shared Texture2D 作成 / open を安全にする。

## 追加

```text
include/D3D11Helper/D3D11Interop/D3D11SharedHandle.hpp
src/D3D11SharedHandle.cpp

include/D3D11Helper/D3D11Interop/D3D11SharedTexture.hpp
src/D3D11SharedTexture.cpp

Test/test_interop_shared.cpp
```

## 更新

```text
CMakeLists.txt
include/D3D11Helper/D3D11Interop/D3D11Interop.hpp
include/D3D11Helper/D3D11Interop/D3D11SharedResource.hpp
src/D3D11SharedResource.cpp

include/D3D11Helper/D3D11Gpu/D3D11Helpers.hpp
src/D3D11Helpers.cpp

Test/CMakeLists.txt
Test/test_module_headers.cpp
```

## 重要修正

`CreateSharedTexture2D(D3D11Core&, ..., D3D11SharedTextureSyncMode)` の header / cpp signature mismatch を修正する。

## Test

```text
Test/test_interop_shared.cpp
```

必須:

- shared handle RAII
- shared handle move
- shared texture creation
- shared handle creation
- same device open
- existing D3D11Helpers::CreateSharedTexture2D link/run
- invalid desc

## Commit

```text
Add D3D11 shared handle and shared texture helpers
```

---

# Round 2 - KeyedMutex

## 目的

DXGI keyed mutex を RAII で扱えるようにする。

## 追加

```text
include/D3D11Helper/D3D11Interop/D3D11KeyedMutex.hpp
src/D3D11KeyedMutex.cpp
Test/test_interop_keyed_mutex.cpp
```

## 更新

```text
CMakeLists.txt
include/D3D11Helper/D3D11Interop/D3D11Interop.hpp
Test/CMakeLists.txt
Test/test_module_headers.cpp
```

## Test

```text
Test/test_interop_keyed_mutex.cpp
```

必須:

- keyed mutex texture 作成
- attach
- acquire/release key sequence
- scoped acquire
- non-keyed-mutex texture attach failure

## Commit

```text
Add D3D11 keyed mutex helpers
```

---

# Round 3 - FenceSupport / Fence hardening / Sample

## 目的

既存 `D3D11Fence` を壊さず、support check と timeout wait を追加する。
Interop sample を追加する。

## 追加

```text
include/D3D11Helper/D3D11Interop/D3D11FenceSupport.hpp
src/D3D11FenceSupport.cpp

Test/test_interop_fence.cpp
sample/24_Interop/main.cpp
```

## 更新

```text
CMakeLists.txt
include/D3D11Helper/D3D11Interop/D3D11Interop.hpp
include/D3D11Helper/D3D11Interop/D3D11Fence.hpp
src/D3D11Fence.cpp

Test/CMakeLists.txt
Test/test_module_headers.cpp
sample/CMakeLists.txt
```

## Test

```text
Test/test_interop_fence.cpp
```

必須:

- unsupported environment で skip-style success
- supported environment で Initialize / Signal / Flush / CpuWait
- shared handle raw + owned
- OpenSharedHandle

## Sample

```text
sample/24_Interop
```

console-only。

## Commit

```text
Harden D3D11 fence interop helpers and add interop sample
```

---

# Final implementation audit before docs

実装完了後、docs patch に入る前に次を確認する。

```bat
build_and_test.cmd

findstr /N "D3D11SharedHandle D3D11SharedTexture D3D11KeyedMutex D3D11FenceSupport" CMakeLists.txt
findstr /N "D3D11SharedHandle D3D11SharedTexture D3D11KeyedMutex D3D11FenceSupport" include\D3D11Helper\D3D11Interop\D3D11Interop.hpp

findstr /N "VERSION 1.7.0" CMakeLists.txt

dir include\D3D11Helper\D3D11Interop\D3D11SharedHandle.hpp
dir include\D3D11Helper\D3D11Interop\D3D11SharedTexture.hpp
dir include\D3D11Helper\D3D11Interop\D3D11KeyedMutex.hpp
dir include\D3D11Helper\D3D11Interop\D3D11FenceSupport.hpp

dir src\D3D11SharedHandle.cpp
dir src\D3D11SharedTexture.cpp
dir src\D3D11KeyedMutex.cpp
dir src\D3D11FenceSupport.cpp

dir Test\test_interop_shared.cpp
dir Test\test_interop_keyed_mutex.cpp
dir Test\test_interop_fence.cpp
dir sample\24_Interop\main.cpp
```

期待:

- build/test が通る。
- 実装 file がすべて登録されている。
- `D3D11Interop.hpp` から include されている。
- version はまだ `1.7.0`。
