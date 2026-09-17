import Foundation

public struct FIBPCapabilities: OptionSet, Equatable, Sendable {
    public let rawValue: UInt32

    public init(rawValue: UInt32) { self.rawValue = rawValue }

    public static let httpsGET = FIBPCapabilities(rawValue: 0x0000_0001)
    public static let httpsPOST = FIBPCapabilities(rawValue: 0x0000_0002)
    public static let requestHeaders = FIBPCapabilities(rawValue: 0x0000_0004)
    public static let responseHeaders = FIBPCapabilities(rawValue: 0x0000_0008)
    public static let cancellation = FIBPCapabilities(rawValue: 0x0000_0010)
    public static let helperSupported: FIBPCapabilities = [
        .httpsGET, .httpsPOST, .requestHeaders, .responseHeaders, .cancellation,
    ]
}

public struct FIBPHello: Equatable, Sendable {
    public let minimumMajor: UInt8
    public let minimumMinor: UInt8
    public let maximumMajor: UInt8
    public let maximumMinor: UInt8
    public let capabilities: FIBPCapabilities
    public let maximumReceivePayload: UInt16
    public let maximumResponseBytes: UInt32
    public let clientNonce: UInt64
    public let model: String
    public let name: String
    public let idType: UInt8
    public let deviceID: Data
    public let appVersion: String

    public init(
        minimumMajor: UInt8 = 1,
        minimumMinor: UInt8 = 0,
        maximumMajor: UInt8 = 1,
        maximumMinor: UInt8 = 0,
        capabilities: FIBPCapabilities,
        maximumReceivePayload: UInt16,
        maximumResponseBytes: UInt32,
        clientNonce: UInt64,
        model: String,
        name: String,
        idType: UInt8,
        deviceID: Data,
        appVersion: String
    ) {
        self.minimumMajor = minimumMajor
        self.minimumMinor = minimumMinor
        self.maximumMajor = maximumMajor
        self.maximumMinor = maximumMinor
        self.capabilities = capabilities
        self.maximumReceivePayload = maximumReceivePayload
        self.maximumResponseBytes = maximumResponseBytes
        self.clientNonce = clientNonce
        self.model = model
        self.name = name
        self.idType = idType
        self.deviceID = deviceID
        self.appVersion = appVersion
    }

    public init(payload: Data) throws {
        var cursor = ByteCursor(payload)
        minimumMajor = try cursor.readUInt8()
        minimumMinor = try cursor.readUInt8()
        maximumMajor = try cursor.readUInt8()
        maximumMinor = try cursor.readUInt8()
        capabilities = FIBPCapabilities(rawValue: try cursor.readUInt32())
        maximumReceivePayload = try cursor.readUInt16()
        maximumResponseBytes = try cursor.readUInt32()
        clientNonce = try cursor.readUInt64()
        model = try cursor.readLengthPrefixedString(lengthBytes: 1, maximum: 16)
        name = try cursor.readLengthPrefixedString(lengthBytes: 1, maximum: 32)
        idType = try cursor.readUInt8()
        let idLength = Int(try cursor.readUInt8())
        guard (1...16).contains(idLength) else { throw FIBPPayloadError.invalidLength }
        deviceID = try cursor.readData(count: idLength)
        appVersion = try cursor.readLengthPrefixedString(lengthBytes: 1, maximum: 16)
        try cursor.requireEnd()

        guard maximumReceivePayload > 0,
              minimumMajor <= maximumMajor,
              idType == 1 else {
            throw FIBPPayloadError.invalidValue
        }
    }

    public func encoded() throws -> Data {
        let modelBytes = Array(model.utf8)
        let nameBytes = Array(name.utf8)
        let versionBytes = Array(appVersion.utf8)
        guard (1...16).contains(modelBytes.count),
              (1...32).contains(nameBytes.count),
              (1...16).contains(deviceID.count),
              (1...16).contains(versionBytes.count),
              Self.validText(modelBytes), Self.validText(nameBytes), Self.validText(versionBytes) else {
            throw FIBPPayloadError.invalidLength
        }
        var bytes = [minimumMajor, minimumMinor, maximumMajor, maximumMinor]
        bytes.appendLittleEndian(capabilities.rawValue)
        bytes.appendLittleEndian(maximumReceivePayload)
        bytes.appendLittleEndian(maximumResponseBytes)
        bytes.appendLittleEndian(clientNonce)
        bytes.append(UInt8(modelBytes.count)); bytes.append(contentsOf: modelBytes)
        bytes.append(UInt8(nameBytes.count)); bytes.append(contentsOf: nameBytes)
        bytes.append(idType)
        bytes.append(UInt8(deviceID.count)); bytes.append(contentsOf: deviceID)
        bytes.append(UInt8(versionBytes.count)); bytes.append(contentsOf: versionBytes)
        return Data(bytes)
    }

    private static func validText(_ bytes: [UInt8]) -> Bool {
        !bytes.contains { $0 == 0 || $0 < 0x20 || $0 == 0x7F }
    }
}

public struct FlipperIdentity: Equatable, Sendable {
    public let model: String
    public let name: String
    public let idType: UInt8
    public let deviceID: Data
    public let appVersion: String
    public let protocolMajor: UInt8
    public let protocolMinor: UInt8

    public var redactedID: String {
        let prefix = deviceID.prefix(4).map { String(format: "%02X", $0) }.joined()
        return deviceID.count > 4 ? "\(prefix)…" : prefix
    }

    /// Device-controlled text is never interpolated into AppKit/SwiftUI/logs
    /// without removing bidi overrides, zero-width format characters, controls,
    /// and other non-display scalar categories.
    public var displayName: String {
        let safeScalars = name.unicodeScalars.filter { scalar in
            switch scalar.properties.generalCategory {
            case .control, .format, .lineSeparator, .paragraphSeparator,
                 .surrogate, .privateUse, .unassigned:
                return false
            default:
                return true
            }
        }
        let cleaned = String(String.UnicodeScalarView(safeScalars))
            .trimmingCharacters(in: .whitespaces)
        return cleaned.isEmpty ? "Unnamed device" : String(cleaned.prefix(32))
    }
}

public struct FIBPHelloAcknowledgment: Equatable, Sendable {
    public let selectedMajor: UInt8
    public let selectedMinor: UInt8
    public let capabilities: FIBPCapabilities
    public let maximumPayload: UInt16
    public let maximumResponseBytes: UInt32
    public let echoedClientNonce: UInt64
    public let serverNonce: UInt64

    public var encoded: Data {
        var bytes = [selectedMajor, selectedMinor]
        bytes.appendLittleEndian(capabilities.rawValue)
        bytes.appendLittleEndian(maximumPayload)
        bytes.appendLittleEndian(maximumResponseBytes)
        bytes.appendLittleEndian(echoedClientNonce)
        bytes.appendLittleEndian(serverNonce)
        return Data(bytes)
    }
}

public enum BridgePermissionState: UInt8, Equatable, Sendable {
    case denied = 0
    case allowedOnce = 1
    case alwaysAllowed = 2
}

public enum BridgePermissionDecision: Equatable, Sendable {
    case deny
    case allowOnce
    case alwaysAllow
}

public enum FIBPHTTPMethod: UInt8, Equatable, Sendable {
    case get = 1
    case post = 2
}

public struct FIBPRequestStart: Equatable, Sendable {
    public let method: FIBPHTTPMethod
    public let timeoutMilliseconds: UInt32
    public let url: String
    public let declaredBodyLength: UInt32
    public let declaredHeaderCount: UInt8

    public init(payload: Data) throws {
        var cursor = ByteCursor(payload)
        guard let method = FIBPHTTPMethod(rawValue: try cursor.readUInt8()) else {
            throw FIBPPayloadError.invalidValue
        }
        self.method = method
        timeoutMilliseconds = try cursor.readUInt32()
        let urlLength = Int(try cursor.readUInt16())
        declaredBodyLength = try cursor.readUInt32()
        declaredHeaderCount = try cursor.readUInt8()
        url = try cursor.readString(count: urlLength, maximum: BridgeConfiguration.maximumURLBytes)
        try cursor.requireEnd()

        guard timeoutMilliseconds > 0,
              timeoutMilliseconds <= BridgeConfiguration.maximumRequestTimeoutMilliseconds,
              declaredBodyLength <= UInt32(BridgeConfiguration.maximumRequestBodyBytes),
              Int(declaredHeaderCount) <= BridgeConfiguration.maximumHeaderCount,
              !(method == .get && declaredBodyLength != 0) else {
            throw FIBPPayloadError.invalidValue
        }
    }

    public init(
        method: FIBPHTTPMethod,
        timeoutMilliseconds: UInt32,
        url: String,
        declaredBodyLength: UInt32,
        declaredHeaderCount: UInt8
    ) {
        self.method = method
        self.timeoutMilliseconds = timeoutMilliseconds
        self.url = url
        self.declaredBodyLength = declaredBodyLength
        self.declaredHeaderCount = declaredHeaderCount
    }

    public var encoded: Data {
        let urlBytes = Array(url.utf8)
        var bytes = [method.rawValue]
        bytes.appendLittleEndian(timeoutMilliseconds)
        bytes.appendLittleEndian(UInt16(urlBytes.count))
        bytes.appendLittleEndian(declaredBodyLength)
        bytes.append(declaredHeaderCount)
        bytes.append(contentsOf: urlBytes)
        return Data(bytes)
    }
}

public struct BridgeHTTPHeader: Equatable, Sendable {
    public let name: String
    public let value: String

    public init(name: String, value: String) {
        self.name = name
        self.value = value
    }

    public init(payload: Data) throws {
        var cursor = ByteCursor(payload)
        let nameLength = Int(try cursor.readUInt8())
        let valueLength = Int(try cursor.readUInt16())
        guard (1...BridgeConfiguration.maximumHeaderNameBytes).contains(nameLength),
              (0...BridgeConfiguration.maximumHeaderValueBytes).contains(valueLength),
              let parsedName = String(
                data: try cursor.readData(count: nameLength),
                encoding: .utf8
              ),
              let parsedValue = String(
                data: try cursor.readData(count: valueLength),
                encoding: .utf8
              ),
              Self.isValidName(parsedName), Self.isValidValue(parsedValue) else {
            throw FIBPPayloadError.invalidText
        }
        name = parsedName
        value = parsedValue
        try cursor.requireEnd()
    }

    public static func isValidName(_ value: String) -> Bool {
        !value.isEmpty && value.utf8.allSatisfy { byte in
            switch byte {
            case 0x30...0x39, 0x41...0x5A, 0x61...0x7A,
                 0x21, 0x23...0x27, 0x2A, 0x2B, 0x2D, 0x2E,
                 0x5E, 0x5F, 0x60, 0x7C, 0x7E:
                return true
            default:
                return false
            }
        }
    }

    public static func isValidValue(_ value: String) -> Bool {
        value.utf8.allSatisfy { byte in
            byte == 0x09 || (byte >= 0x20 && byte != 0x7F)
        }
    }

    public var encoded: Data {
        let nameBytes = Array(name.utf8)
        let valueBytes = Array(value.utf8)
        var bytes = [UInt8(nameBytes.count)]
        bytes.appendLittleEndian(UInt16(valueBytes.count))
        bytes.append(contentsOf: nameBytes)
        bytes.append(contentsOf: valueBytes)
        return Data(bytes)
    }
}

public struct FIBPRemoteErrorPayload: Equatable, Sendable {
    public let code: UInt16
    public let scope: UInt8
    public let offendingType: UInt8
    public let detail: String

    public init(payload: Data) throws {
        var cursor = ByteCursor(payload)
        code = try cursor.readUInt16()
        scope = try cursor.readUInt8()
        offendingType = try cursor.readUInt8()
        let length = Int(try cursor.readUInt16())
        guard scope <= 1, length <= BridgeConfiguration.maximumErrorDetailBytes else {
            throw FIBPPayloadError.invalidValue
        }
        let detailData = try cursor.readData(count: length)
        guard detailData.allSatisfy({ $0 >= 0x20 && $0 != 0x7F }),
              let detail = String(data: detailData, encoding: .utf8) else {
            throw FIBPPayloadError.invalidText
        }
        self.detail = detail
        try cursor.requireEnd()
    }
}

public enum FIBPPayloadEncoder {
    public static func permissionRequired(reason: UInt8) -> Data { Data([reason]) }

    public static func permissionStatus(state: BridgePermissionState, reason: UInt8) -> Data {
        Data([state.rawValue, reason])
    }

    public static func responseStart(
        status: UInt16,
        headerCount: UInt8,
        declaredBodyLength: UInt32
    ) -> Data {
        var bytes = [UInt8]()
        bytes.appendLittleEndian(status)
        bytes.append(headerCount)
        bytes.append(0)
        bytes.appendLittleEndian(declaredBodyLength)
        return Data(bytes)
    }

    public static func responseEnd(result: UInt8, bytesSent: UInt32) -> Data {
        var bytes = [result]
        bytes.appendLittleEndian(bytesSent)
        return Data(bytes)
    }

    public static func error(
        code: FIBPErrorCode,
        scope: UInt8,
        offendingType: UInt8,
        detail: String,
        maximumPayload: Int = BridgeConfiguration.maximumWirePayload
    ) -> Data {
        let clean = detail.unicodeScalars.filter { scalar in
            scalar.value >= 0x20 && scalar.value != 0x7F
        }
        var cleanDetail = String(String.UnicodeScalarView(clean))
        let detailLimit = min(
            BridgeConfiguration.maximumErrorDetailBytes,
            max(0, maximumPayload - 6)
        )
        while cleanDetail.utf8.count > detailLimit {
            cleanDetail.removeLast()
        }
        let detailBytes = Array(cleanDetail.utf8)
        var bytes = [UInt8]()
        bytes.appendLittleEndian(code.rawValue)
        bytes.append(scope)
        bytes.append(offendingType)
        bytes.appendLittleEndian(UInt16(detailBytes.count))
        bytes.append(contentsOf: detailBytes)
        return Data(bytes)
    }
}

private struct ByteCursor {
    private let bytes: [UInt8]
    private var offset = 0

    init(_ data: Data) { bytes = Array(data) }

    mutating func readUInt8() throws -> UInt8 {
        guard offset < bytes.count else { throw FIBPPayloadError.truncated }
        defer { offset += 1 }
        return bytes[offset]
    }

    mutating func readUInt16() throws -> UInt16 {
        guard offset + 2 <= bytes.count else { throw FIBPPayloadError.truncated }
        defer { offset += 2 }
        return bytes.littleEndianUInt16(at: offset)
    }

    mutating func readUInt32() throws -> UInt32 {
        guard offset + 4 <= bytes.count else { throw FIBPPayloadError.truncated }
        defer { offset += 4 }
        return bytes.littleEndianUInt32(at: offset)
    }

    mutating func readUInt64() throws -> UInt64 {
        let low = UInt64(try readUInt32())
        let high = UInt64(try readUInt32())
        return low | (high << 32)
    }

    mutating func readData(count: Int) throws -> Data {
        guard count >= 0, offset + count <= bytes.count else {
            throw FIBPPayloadError.truncated
        }
        defer { offset += count }
        return Data(bytes[offset..<(offset + count)])
    }

    mutating func readLengthPrefixedString(lengthBytes: Int, maximum: Int) throws -> String {
        let length: Int
        switch lengthBytes {
        case 1: length = Int(try readUInt8())
        case 2: length = Int(try readUInt16())
        default: throw FIBPPayloadError.invalidLength
        }
        return try readString(count: length, maximum: maximum)
    }

    mutating func readString(count: Int, maximum: Int) throws -> String {
        guard (1...maximum).contains(count) else { throw FIBPPayloadError.invalidLength }
        let data = try readData(count: count)
        guard !data.contains(where: { $0 == 0 || $0 < 0x20 || $0 == 0x7F }),
              let value = String(data: data, encoding: .utf8) else {
            throw FIBPPayloadError.invalidText
        }
        return value
    }

    func requireEnd() throws {
        guard offset == bytes.count else { throw FIBPPayloadError.trailingBytes }
    }
}
