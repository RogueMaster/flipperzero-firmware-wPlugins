import Foundation
import XCTest
@testable import BridgeCore

final class FIBPCodecTests: XCTestCase {
    func testCRCReferenceValue() {
        XCTAssertEqual(FIBPCRC32.checksum("123456789".utf8), 0xCBF4_3926)
    }

    func testPingMatchesNormativeVector() throws {
        let frame = FIBPFrame(
            messageType: .ping,
            requestID: 0,
            sequence: 1,
            payload: Data([0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01])
        )
        XCTAssertEqual(
            try FIBPCodec.encode(frame).hex,
            "4649425001001c3100000000000000000100000008000000bc314c10" +
                "0807060504030201beddfb32"
        )
    }

    func testRequestEndMatchesNormativeVector() throws {
        let frame = FIBPFrame(
            messageType: .requestEnd,
            flags: [.final],
            requestID: 0x1122_3344,
            sequence: 3
        )
        XCTAssertEqual(
            try FIBPCodec.encode(frame).hex,
            "4649425001001c13020000004433221103000000000000003dbd1c983dbd1c98"
        )
    }

    func testHelloAckPayloadIsExactly28Bytes() {
        let ack = FIBPHelloAcknowledgment(
            selectedMajor: 1,
            selectedMinor: 0,
            capabilities: .helperSupported,
            maximumPayload: 512,
            maximumResponseBytes: 16_384,
            echoedClientNonce: 0x0102_0304_0506_0708,
            serverNonce: 0x1112_1314_1516_1718
        )
        XCTAssertEqual(ack.encoded.count, 28)
        XCTAssertEqual(Array(ack.encoded.suffix(8)), [0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11])
    }
}

private extension Data {
    var hex: String { map { String(format: "%02x", $0) }.joined() }
}
