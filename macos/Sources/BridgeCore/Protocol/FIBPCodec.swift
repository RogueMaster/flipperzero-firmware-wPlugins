import Foundation

public enum FIBPCRC32 {
    public static func checksum<S: Sequence>(_ bytes: S) -> UInt32 where S.Element == UInt8 {
        finish(update(0xFFFF_FFFF, with: bytes))
    }

    static func update<S: Sequence>(_ initial: UInt32, with bytes: S) -> UInt32
    where S.Element == UInt8 {
        var crc = initial
        for byte in bytes {
            crc ^= UInt32(byte)
            for _ in 0..<8 {
                let mask = UInt32(bitPattern: -Int32(crc & 1))
                crc = (crc >> 1) ^ (0xEDB8_8320 & mask)
            }
        }
        return crc
    }

    static func finish(_ crc: UInt32) -> UInt32 {
        crc ^ 0xFFFF_FFFF
    }
}

public enum FIBPCodecError: Error, Equatable {
    case payloadTooLarge
}

public enum FIBPCodec {
    private static let magic: [UInt8] = [0x46, 0x49, 0x42, 0x50]

    public static func encode(_ frame: FIBPFrame) throws -> Data {
        guard frame.payload.count <= BridgeConfiguration.maximumWirePayload else {
            throw FIBPCodecError.payloadTooLarge
        }

        var header = [UInt8]()
        header.reserveCapacity(BridgeConfiguration.frameHeaderSize)
        header.append(contentsOf: magic)
        header.append(frame.major)
        header.append(frame.minor)
        header.append(UInt8(BridgeConfiguration.frameHeaderSize))
        header.append(frame.rawMessageType)
        header.appendLittleEndian(frame.flags.rawValue)
        header.appendLittleEndian(UInt16(0))
        header.appendLittleEndian(frame.requestID)
        header.appendLittleEndian(frame.sequence)
        header.appendLittleEndian(UInt32(frame.payload.count))

        let headerCRC = FIBPCRC32.checksum(header)
        var encoded = header
        encoded.appendLittleEndian(headerCRC)
        encoded.append(contentsOf: frame.payload)

        var frameCRC = FIBPCRC32.update(0xFFFF_FFFF, with: header)
        frameCRC = FIBPCRC32.update(frameCRC, with: frame.payload)
        encoded.appendLittleEndian(FIBPCRC32.finish(frameCRC))
        return Data(encoded)
    }
}

extension Array where Element == UInt8 {
    mutating func appendLittleEndian(_ value: UInt16) {
        append(UInt8(truncatingIfNeeded: value))
        append(UInt8(truncatingIfNeeded: value >> 8))
    }

    mutating func appendLittleEndian(_ value: UInt32) {
        for shift in stride(from: 0, through: 24, by: 8) {
            append(UInt8(truncatingIfNeeded: value >> UInt32(shift)))
        }
    }

    mutating func appendLittleEndian(_ value: UInt64) {
        for shift in stride(from: 0, through: 56, by: 8) {
            append(UInt8(truncatingIfNeeded: value >> UInt64(shift)))
        }
    }

    func littleEndianUInt16(at offset: Int) -> UInt16 {
        UInt16(self[offset]) | (UInt16(self[offset + 1]) << 8)
    }

    func littleEndianUInt32(at offset: Int) -> UInt32 {
        UInt32(self[offset])
            | (UInt32(self[offset + 1]) << 8)
            | (UInt32(self[offset + 2]) << 16)
            | (UInt32(self[offset + 3]) << 24)
    }
}
