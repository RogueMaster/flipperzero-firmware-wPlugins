import Foundation

public enum FIBPMessageType: UInt8, CaseIterable, Sendable {
    case hello = 0x01
    case helloAck = 0x02
    case permissionStatus = 0x03
    case permissionRequired = 0x04
    case requestStart = 0x10
    case requestHeader = 0x11
    case requestBodyChunk = 0x12
    case requestEnd = 0x13
    case responseStart = 0x20
    case responseHeader = 0x21
    case responseBodyChunk = 0x22
    case responseEnd = 0x23
    case cancel = 0x30
    case ping = 0x31
    case pong = 0x32
    case error = 0x7E
    case disconnect = 0x7F
}

public struct FIBPFlags: OptionSet, Equatable, Sendable {
    public let rawValue: UInt16

    public init(rawValue: UInt16) {
        self.rawValue = rawValue
    }

    public static let acknowledgmentRequired = FIBPFlags(rawValue: 0x0001)
    public static let final = FIBPFlags(rawValue: 0x0002)
    public static let truncated = FIBPFlags(rawValue: 0x0004)
    public static let retryable = FIBPFlags(rawValue: 0x0008)
}

public struct FIBPFrame: Equatable, Sendable {
    public var major: UInt8
    public var minor: UInt8
    public var rawMessageType: UInt8
    public var flags: FIBPFlags
    public var requestID: UInt32
    public var sequence: UInt32
    public var payload: Data

    public var messageType: FIBPMessageType? {
        FIBPMessageType(rawValue: rawMessageType)
    }

    public init(
        major: UInt8 = BridgeConfiguration.protocolMajor,
        minor: UInt8 = BridgeConfiguration.protocolMinor,
        messageType: FIBPMessageType,
        flags: FIBPFlags = [],
        requestID: UInt32,
        sequence: UInt32,
        payload: Data = Data()
    ) {
        self.major = major
        self.minor = minor
        self.rawMessageType = messageType.rawValue
        self.flags = flags
        self.requestID = requestID
        self.sequence = sequence
        self.payload = payload
    }

    public init(
        major: UInt8,
        minor: UInt8,
        rawMessageType: UInt8,
        flags: FIBPFlags,
        requestID: UInt32,
        sequence: UInt32,
        payload: Data
    ) {
        self.major = major
        self.minor = minor
        self.rawMessageType = rawMessageType
        self.flags = flags
        self.requestID = requestID
        self.sequence = sequence
        self.payload = payload
    }
}

public enum FIBPErrorCode: UInt16, Equatable, Sendable {
    case malformedFrame = 0x0001
    case badHeaderCRC = 0x0002
    case badFrameCRC = 0x0003
    case payloadTooLarge = 0x0004
    case unsupportedVersion = 0x0005
    case unsupportedMessage = 0x0006
    case duplicateSequence = 0x0007
    case sequenceGap = 0x0008
    case invalidState = 0x0009
    case duplicateRequestID = 0x000A
    case permissionDenied = 0x000B
    case invalidRequest = 0x000C
    case securityBlocked = 0x000D
    case timeout = 0x000E
    case cancelled = 0x000F
    case responseTooLarge = 0x0010
    case receiveOverflow = 0x0011
    case internalError = 0x0012
    case transportLost = 0x0013
    case networkFailure = 0x0014
}

public enum FIBPParseError: Error, Equatable, Sendable {
    case invalidHeader
    case badHeaderCRC
    case payloadTooLarge
    case badFrameCRC
    case receiveOverflow
    case assemblyTimeout

    public var wireErrorCode: FIBPErrorCode {
        switch self {
        case .invalidHeader, .assemblyTimeout:
            return .malformedFrame
        case .badHeaderCRC:
            return .badHeaderCRC
        case .payloadTooLarge:
            return .payloadTooLarge
        case .badFrameCRC:
            return .badFrameCRC
        case .receiveOverflow:
            return .receiveOverflow
        }
    }
}

public enum FIBPParserEvent: Equatable, Sendable {
    case frame(FIBPFrame)
    case error(FIBPParseError)
    /// A corrupt frame whose visible type byte says ERROR. It is kept distinct
    /// so higher layers can obey the no-ERROR-to-ERROR invariant.
    case rejectedErrorFrame(FIBPParseError)
}

public enum FIBPPayloadError: Error, Equatable, Sendable {
    case truncated
    case trailingBytes
    case invalidLength
    case invalidText
    case invalidValue
}
