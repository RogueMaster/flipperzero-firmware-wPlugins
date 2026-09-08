import Foundation
import XCTest
@testable import BridgeCore

final class PayloadAndPermissionTests: XCTestCase {
    func testHelloRoundTrip() throws {
        let hello = FIBPHello(
            capabilities: .helperSupported,
            maximumReceivePayload: 512,
            maximumResponseBytes: 16_384,
            clientNonce: 42,
            model: "Flipper Zero",
            name: "Mico",
            idType: 1,
            deviceID: Data([1, 2, 3, 4, 5, 6, 7, 8]),
            appVersion: "0.1.0"
        )
        XCTAssertEqual(try FIBPHello(payload: hello.encoded()), hello)
    }

    func testPersistentPermissionBindsProtocolVersionAndStoresNoRawUID() {
        let suite = "FIBPTests-\(UUID().uuidString)"
        let defaults = UserDefaults(suiteName: suite)!
        defer { defaults.removePersistentDomain(forName: suite) }
        let store = UserDefaultsPermissionStore(defaults: defaults)
        let identity = makeIdentity(minor: 0)

        XCTAssertFalse(store.contains(identity))
        store.grant(identity)
        XCTAssertTrue(store.contains(identity))
        XCTAssertFalse(store.contains(makeIdentity(minor: 1)))

        let serialized = defaults.stringArray(forKey: "persistent-device-permissions")?.joined() ?? ""
        XCTAssertTrue(serialized.hasPrefix("sha256:"))
        XCTAssertFalse(serialized.contains("0102030405060708"))

        store.revoke(identity)
        XCTAssertFalse(store.contains(identity))
    }

    func testRequestTimeoutAndBodyLimitsAreRejectedDuringDecode() {
        for timeout: UInt32 in [0, BridgeConfiguration.maximumRequestTimeoutMilliseconds + 1] {
            let encoded = FIBPRequestStart(
                method: .get,
                timeoutMilliseconds: timeout,
                url: "https://example.com/",
                declaredBodyLength: 0,
                declaredHeaderCount: 0
            ).encoded
            XCTAssertThrowsError(try FIBPRequestStart(payload: encoded))
        }

        let oversized = FIBPRequestStart(
            method: .post,
            timeoutMilliseconds: 1_000,
            url: "https://example.com/",
            declaredBodyLength: UInt32(BridgeConfiguration.maximumRequestBodyBytes + 1),
            declaredHeaderCount: 0
        )
        XCTAssertThrowsError(try FIBPRequestStart(payload: oversized.encoded))
    }

    func testDeviceDisplayNameRemovesBidiAndZeroWidthFormatting() {
        let identity = FlipperIdentity(
            model: "Flipper Zero",
            name: "Mi\u{202E}ço\u{200B}",
            idType: 1,
            deviceID: Data([1]),
            appVersion: "1.0",
            protocolMajor: 1,
            protocolMinor: 0
        )
        XCTAssertEqual(identity.displayName, "Miço")

        let unnamed = FlipperIdentity(
            model: "Flipper Zero",
            name: "\u{202E}\u{200B}",
            idType: 1,
            deviceID: Data([1]),
            appVersion: "1.0",
            protocolMajor: 1,
            protocolMinor: 0
        )
        XCTAssertEqual(unnamed.displayName, "Unnamed device")
    }

    func testBoundedErrorDetailNeverSplitsUTF8Scalar() {
        let payload = FIBPPayloadEncoder.error(
            code: .internalError,
            scope: 0,
            offendingType: 0,
            detail: String(repeating: "🐬", count: 100)
        )
        let detail = payload.dropFirst(6)
        XCTAssertTrue(detail.count <= BridgeConfiguration.maximumErrorDetailBytes)
        XCTAssertTrue(String(data: detail, encoding: .utf8) != nil)
        XCTAssertEqual(FIBPPayloadEncoder.error(
            code: .internalError,
            scope: 0,
            offendingType: 0,
            detail: "123456789",
            maximumPayload: 10
        ).count, 10)
    }

    func testHTTPHeaderNameAndValueGrammar() throws {
        func payload(name: [UInt8], value: [UInt8]) -> Data {
            var bytes = [UInt8(name.count), UInt8(value.count & 0xFF), UInt8(value.count >> 8)]
            bytes.append(contentsOf: name)
            bytes.append(contentsOf: value)
            return Data(bytes)
        }

        for invalidName in [
            Array("Bad Name".utf8), Array("Bad:Name".utf8),
            Array("Bad\tName".utf8), Array("Nämé".utf8),
        ] {
            XCTAssertThrowsError(try BridgeHTTPHeader(payload: payload(
                name: invalidName,
                value: Array("ok".utf8)
            )))
        }
        for invalidValue in [[UInt8(0x01)], [UInt8(0x7F)]] {
            XCTAssertThrowsError(try BridgeHTTPHeader(payload: payload(
                name: Array("Accept".utf8),
                value: invalidValue
            )))
        }
        XCTAssertEqual(
            try BridgeHTTPHeader(payload: payload(
                name: Array("X-Test".utf8),
                value: Array("one\ttwo".utf8)
            )),
            BridgeHTTPHeader(name: "X-Test", value: "one\ttwo")
        )
    }

    private func makeIdentity(minor: UInt8) -> FlipperIdentity {
        FlipperIdentity(
            model: "Flipper Zero",
            name: "Mico",
            idType: 1,
            deviceID: Data([1, 2, 3, 4, 5, 6, 7, 8]),
            appVersion: "0.1.0",
            protocolMajor: 1,
            protocolMinor: minor
        )
    }
}
