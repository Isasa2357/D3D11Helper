#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>

#include <iostream>

int main() {
    std::cout << "D3D11Helper package smoke test\n";
    D3D11CoreLib::D3D11CoreConfig config;
    config.allowWarpAdapter = true;
    return 0;
}
