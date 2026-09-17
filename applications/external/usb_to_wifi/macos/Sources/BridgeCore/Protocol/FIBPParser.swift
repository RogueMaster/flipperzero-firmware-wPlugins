import Foundation

public final class FIBPStreamParser {
    private static let magic: [UInt8] = [0x46, 0x49, 0x42, 0x50]
    private var buffer = [UInt8]()
    private var lastProgress: Date?

    public init() {}

    public var bufferedByteCount: Int { buffer.count }

    public func reset() {
        buffer.removeAll(keepingCapacity: true)
        lastProgress = nil
    }

    public func feed(_ data: Data, now: Date = Date()) -> [FIBPParserEvent] {
        guard !data.isEmpty else { return [] }
        if buffer.count + data.count > BridgeConfiguration.maximumBufferedSerialBytes {
            let visible = buffer.isEmpty ? [UInt8](data.prefix(8)) : Array(buffer.prefix(8))
            let isError = visible.count > 7
                && Array(visible.prefix(4)) == Self.magic
                && visible[7] == FIBPMessageType.error.rawValue
            reset()
            return [isError ? .rejectedErrorFrame(.receiveOverflow) : .error(.receiveOverflow)]
        }

        buffer.append(contentsOf: data)
        lastProgress = now
        var events = [FIBPParserEvent]()

        while true {
            guard alignToMagic() else { break }
            guard buffer.count >= BridgeConfiguration.frameHeaderSize else { break }

            let headerPrefix = Array(buffer[0..<24])
            guard buffer[6] == UInt8(BridgeConfiguration.frameHeaderSize),
                  buffer.littleEndianUInt16(at: 10) == 0 else {
                events.append(errorEvent(.invalidHeader))
                buffer.removeFirst()
                continue
            }

            let expectedHeaderCRC = buffer.littleEndianUInt32(at: 24)
            guard FIBPCRC32.checksum(headerPrefix) == expectedHeaderCRC else {
                events.append(errorEvent(.badHeaderCRC))
                buffer.removeFirst()
                continue
            }

            let payloadLength = Int(buffer.littleEndianUInt32(at: 20))
            guard payloadLength <= BridgeConfiguration.maximumWirePayload else {
                events.append(errorEvent(.payloadTooLarge))
                buffer.removeFirst()
                continue
            }

            let frameLength = BridgeConfiguration.frameHeaderSize
                + payloadLength
                + BridgeConfiguration.frameCRCSize
            guard buffer.count >= frameLength else { break }

            let payloadStart = BridgeConfiguration.frameHeaderSize
            let payload = Data(buffer[payloadStart..<(payloadStart + payloadLength)])
            let expectedFrameCRC = buffer.littleEndianUInt32(
                at: payloadStart + payloadLength
            )
            var actualFrameCRC = FIBPCRC32.update(0xFFFF_FFFF, with: headerPrefix)
            actualFrameCRC = FIBPCRC32.update(actualFrameCRC, with: payload)
            actualFrameCRC = FIBPCRC32.finish(actualFrameCRC)
            guard actualFrameCRC == expectedFrameCRC else {
                events.append(errorEvent(.badFrameCRC))
                buffer.removeFirst()
                continue
            }

            let frame = FIBPFrame(
                major: buffer[4],
                minor: buffer[5],
                rawMessageType: buffer[7],
                flags: FIBPFlags(rawValue: buffer.littleEndianUInt16(at: 8)),
                requestID: buffer.littleEndianUInt32(at: 12),
                sequence: buffer.littleEndianUInt32(at: 16),
                payload: payload
            )
            events.append(.frame(frame))
            buffer.removeFirst(frameLength)
            if buffer.isEmpty { lastProgress = nil }
        }
        return events
    }

    public func resetIfStalled(now: Date = Date()) -> FIBPParserEvent? {
        guard !buffer.isEmpty, let lastProgress,
              now.timeIntervalSince(lastProgress) >= BridgeConfiguration.frameAssemblyTimeout else {
            return nil
        }
        let event = errorEvent(.assemblyTimeout)
        reset()
        return event
    }

    private func alignToMagic() -> Bool {
        if buffer.count >= Self.magic.count,
           Array(buffer.prefix(Self.magic.count)) == Self.magic {
            return true
        }

        if let index = firstMagicIndex() {
            if index > 0 { buffer.removeFirst(index) }
            return true
        }

        let keep = longestMagicPrefixAtBufferEnd()
        if keep == 0 {
            buffer.removeAll(keepingCapacity: true)
        } else if buffer.count > keep {
            buffer = Array(buffer.suffix(keep))
        }
        return false
    }

    private func firstMagicIndex() -> Int? {
        guard buffer.count >= Self.magic.count else { return nil }
        for index in 0...(buffer.count - Self.magic.count) {
            if Array(buffer[index..<(index + Self.magic.count)]) == Self.magic {
                return index
            }
        }
        return nil
    }

    private func longestMagicPrefixAtBufferEnd() -> Int {
        let maximum = min(Self.magic.count - 1, buffer.count)
        guard maximum > 0 else { return 0 }
        for length in stride(from: maximum, through: 1, by: -1) {
            if Array(buffer.suffix(length)) == Array(Self.magic.prefix(length)) {
                return length
            }
        }
        return 0
    }

    private func errorEvent(_ error: FIBPParseError) -> FIBPParserEvent {
        if buffer.count > 7, buffer[7] == FIBPMessageType.error.rawValue {
            return .rejectedErrorFrame(error)
        }
        return .error(error)
    }
}
