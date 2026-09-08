import Foundation

public enum BridgeConfiguration {
    public static let protocolMajor: UInt8 = 1
    public static let protocolMinor: UInt8 = 0
    public static let permissionSchemaVersion = 1

    public static let frameHeaderSize = 28
    public static let frameCRCSize = 4
    public static let maximumWirePayload = 512
    public static let maximumBufferedSerialBytes = 4_096
    public static let responseChunkSize = 192

    public static let maximumURLBytes = 384
    public static let maximumHeaderCount = 8
    public static let maximumHeaderNameBytes = 64
    public static let maximumHeaderValueBytes = 256
    public static let maximumAggregateRequestHeaderBytes = 1_024
    public static let maximumRequestBodyBytes = 4 * 1_024
    public static let maximumResponseBytes = 4 * 1_024 * 1_024
    public static let maximumRedirects = 3
    public static let defaultRequestTimeoutMilliseconds: UInt32 = 25_000
    public static let maximumRequestTimeoutMilliseconds: UInt32 = 30_000
    public static let frameAssemblyTimeout: TimeInterval = 1
    public static let requestAssemblyTimeout: TimeInterval = 5
    public static let handshakeTimeout: TimeInterval = 5
    public static let pongTimeout: TimeInterval = 2
    public static let idleSessionTimeout: TimeInterval = 30
    public static let maximumErrorDetailBytes = 128
    // HELLO_ACK is the largest mandatory pre-negotiation payload (28 bytes).
    public static let minimumAdvertisedReceivePayload = 28
    public static let fixedUserAgent = "FlipperUSBInternetBridge/0.1"
    // USB CDC ignores the nominal baud rate and can otherwise overrun Flipper's
    // small receive queue when a server delivers a large body in one burst.
    public static let serialWriteChunkSize = 64
    public static let serialWritePacingMicroseconds: UInt32 = 1_000

    public static let flipperVendorID: UInt16 = 0x0483
    public static let flipperProductID: UInt16 = 0x5740
    public static let bridgeUSBInterfaceNumbers: Set<Int> = [2, 3]

    public static let requestHeaderAllowList: Set<String> = [
        "accept",
        "accept-language",
        "content-type",
    ]

    public static let responseHeaderAllowList: Set<String> = [
        "cache-control",
        "content-language",
        "content-length",
        "content-type",
        "date",
        "etag",
        "last-modified",
    ]
}
