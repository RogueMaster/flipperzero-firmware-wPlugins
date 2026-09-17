import Foundation

public protocol PermissionPrompting: AnyObject {
    func requestPermission(
        for identity: FlipperIdentity,
        completion: @escaping (BridgePermissionDecision) -> Void
    )
    func cancelPendingPrompt()
}

public enum BridgeConnectionState: Equatable, Sendable {
    case disconnected
    case openingSerial
    case waitingForHello
    case permissionPending
    case permissionDenied
    case ready
    case receivingRequest
    case performingRequest
    case sendingResponse
    case error
}

public struct BridgeStatusSnapshot: Equatable, Sendable {
    public let state: BridgeConnectionState
    public let statusText: String
    public let device: SerialDevice?
    public let identity: FlipperIdentity?
    public let permission: BridgePermissionState?
    public let activeRequestID: UInt32?

    public init(
        state: BridgeConnectionState,
        statusText: String,
        device: SerialDevice?,
        identity: FlipperIdentity?,
        permission: BridgePermissionState?,
        activeRequestID: UInt32?
    ) {
        self.state = state
        self.statusText = statusText
        self.device = device
        self.identity = identity
        self.permission = permission
        self.activeRequestID = activeRequestID
    }

    public var isConnected: Bool { device != nil }
    public var internetAccessEnabled: Bool {
        permission == .allowedOnce || permission == .alwaysAllowed
    }
}

