#pragma once
//
// D3D11Gpu.hpp - v1.1.0 module umbrella
//
// GPU groups resource, view, sampler, shader, pipeline, transfer, and binding
// building blocks.  During the v1.1.0 migration this header temporarily forwards
// to D3D11Framework so the new module boundary can be introduced without moving
// implementation files in the first step.
//
// Later migration steps may replace this umbrella with direct includes from
// D3D11Gpu/* and leave D3D11Framework/* as compatibility wrappers.
//

#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>

namespace D3D11CoreLib {
namespace Gpu {

} // namespace Gpu
} // namespace D3D11CoreLib
