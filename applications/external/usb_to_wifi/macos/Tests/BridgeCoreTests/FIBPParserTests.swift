import Foundation
import XCTest
@testable import BridgeCore

final class FIBPParserTests: XCTestCase {
    func testFragmentedAndCoalescedFrames() throws {
        let ping = try FIBPCodec.encode(FIBPFrame(
            messageType: .ping,
            requestID: 0,
            sequence: 1,
            payload: Data(repeating: 0xA5, count: 8)
        ))
        let pong = try FIBPCodec.encode(FIBPFrame(
            messageType: .pong,
            requestID: 0,
            sequence: 1,
            payload: Data(repeating: 0xA5, count: 8)
        ))
        let bytes = Data([0x00, 0x46, 0x00]) + ping + pong
        let parser = FIBPStreamParser()
        var events = [FIBPParserEvent]()
        for byte in bytes {
            events += parser.feed(Data([byte]))
        }
        XCTAssertEqual(events.compactMap(\.frameValue).map(\.messageType), [.ping, .pong])
    }

    func testBadFrameCRCRecoversToNextFrame() throws {
        var corrupt = try FIBPCodec.encode(FIBPFrame(
            messageType: .ping,
            requestID: 0,
            sequence: 1,
            payload: Data(repeating: 1, count: 8)
        ))
        corrupt[30] ^= 0xFF
        let valid = try FIBPCodec.encode(FIBPFrame(
            messageType: .pong,
            requestID: 0,
            sequence: 1,
            payload: Data(repeating: 2, count: 8)
        ))
        let events = FIBPStreamParser().feed(corrupt + valid)
        XCTAssertTrue(events.contains(.error(.badFrameCRC)))
        XCTAssertEqual(events.compactMap(\.frameValue).last?.messageType, .pong)
    }

    func testOversizedHeaderRejectedWithoutWaitingForPayload() throws {
        let bytes = try XCTUnwrap(Data(hex:
            "4649425001001c310000000000000000010000000102000058aac06e"
        ))
        XCTAssertEqual(FIBPStreamParser().feed(bytes), [.error(.payloadTooLarge)])
    }

    func testPartialFrameTimesOut() throws {
        let now = Date(timeIntervalSince1970: 1_000)
        let parser = FIBPStreamParser()
        XCTAssertTrue(parser.feed(Data("FIBP".utf8), now: now).isEmpty)
        XCTAssertEqual(
            parser.resetIfStalled(now: now.addingTimeInterval(1.1)),
            .error(.assemblyTimeout)
        )
        XCTAssertEqual(parser.bufferedByteCount, 0)
    }
}

private extension FIBPParserEvent {
    var frameValue: FIBPFrame? {
        guard case let .frame(frame) = self else { return nil }
        return frame
    }
}

private extension Data {
    init?(hex: String) {
        guard hex.count.isMultiple(of: 2) else { return nil }
        var bytes = [UInt8]()
        var index = hex.startIndex
        while index < hex.endIndex {
            let next = hex.index(index, offsetBy: 2)
            guard let byte = UInt8(hex[index..<next], radix: 16) else { return nil }
            bytes.append(byte)
            index = next
        }
        self.init(bytes)
    }
}
