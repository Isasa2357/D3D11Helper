//
// D3D11InfoQueue.cpp
//
#include <D3D11Helper/D3D11Diagnostics/D3D11InfoQueue.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <stdexcept>

namespace D3D11CoreLib {

ComPtr<ID3D11InfoQueue> TryGetD3D11InfoQueue(ID3D11Device* device) {
    if (!device) {
        throw std::invalid_argument("TryGetD3D11InfoQueue requires a non-null device");
    }

    ComPtr<ID3D11InfoQueue> queue;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&queue)))) {
        return nullptr;
    }
    return queue;
}

ComPtr<ID3D11InfoQueue> GetD3D11InfoQueue(ID3D11Device* device) {
    auto queue = TryGetD3D11InfoQueue(device);
    if (!queue) {
        throw std::runtime_error("D3D11 InfoQueue is unavailable; create the device with the debug layer enabled");
    }
    return queue;
}

UINT64 GetInfoQueueMessageCount(ID3D11InfoQueue* queue) {
    if (!queue) {
        throw std::invalid_argument("GetInfoQueueMessageCount requires a non-null queue");
    }
    return queue->GetNumStoredMessagesAllowedByRetrievalFilter();
}

void ClearInfoQueueMessages(ID3D11InfoQueue* queue) {
    if (!queue) {
        throw std::invalid_argument("ClearInfoQueueMessages requires a non-null queue");
    }
    queue->ClearStoredMessages();
}

void SetInfoQueueBreakOnSeverity(
    ID3D11InfoQueue* queue,
    D3D11_MESSAGE_SEVERITY severity,
    bool enable) {
    if (!queue) {
        throw std::invalid_argument("SetInfoQueueBreakOnSeverity requires a non-null queue");
    }
    D3D11CORE_THROW_IF_FAILED(queue->SetBreakOnSeverity(severity, enable ? TRUE : FALSE));
}

void AddInfoQueueStorageFilterDenySeverity(
    ID3D11InfoQueue* queue,
    const D3D11_MESSAGE_SEVERITY* severities,
    UINT severityCount) {
    if (!queue) {
        throw std::invalid_argument("AddInfoQueueStorageFilterDenySeverity requires a non-null queue");
    }
    if (severityCount == 0) {
        return;
    }
    if (!severities) {
        throw std::invalid_argument("AddInfoQueueStorageFilterDenySeverity requires severities when severityCount is non-zero");
    }

    D3D11_INFO_QUEUE_FILTER filter{};
    filter.DenyList.NumSeverities = severityCount;
    filter.DenyList.pSeverityList = const_cast<D3D11_MESSAGE_SEVERITY*>(severities);
    D3D11CORE_THROW_IF_FAILED(queue->AddStorageFilterEntries(&filter));
}

} // namespace D3D11CoreLib
