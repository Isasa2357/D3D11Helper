#pragma once
//
// D3D11InfoQueue.hpp - ID3D11InfoQueue diagnostics helpers
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

ComPtr<ID3D11InfoQueue> TryGetD3D11InfoQueue(ID3D11Device* device);
ComPtr<ID3D11InfoQueue> GetD3D11InfoQueue(ID3D11Device* device);

UINT64 GetInfoQueueMessageCount(ID3D11InfoQueue* queue);
void ClearInfoQueueMessages(ID3D11InfoQueue* queue);

void SetInfoQueueBreakOnSeverity(
    ID3D11InfoQueue* queue,
    D3D11_MESSAGE_SEVERITY severity,
    bool enable);

void AddInfoQueueStorageFilterDenySeverity(
    ID3D11InfoQueue* queue,
    const D3D11_MESSAGE_SEVERITY* severities,
    UINT severityCount);

} // namespace D3D11CoreLib
