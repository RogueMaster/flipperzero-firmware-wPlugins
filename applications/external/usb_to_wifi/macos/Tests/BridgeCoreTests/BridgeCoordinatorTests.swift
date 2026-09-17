import Foundation
import XCTest
@testable import BridgeCore

final class BridgeCoordinatorTests: XCTestCase {
    func testSuccessfulHandshakePermissionAndHTTPSGETChunking() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.connect()
        harness.sendHello()

        XCTAssertTrue(waitUntil { harness.transport.frames.contains { $0.messageType == .permissionRequired } })
        harness.prompt.decide(.allowOnce)
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().state == .ready })

        harness.sendGET(requestID: 41)
        XCTAssertTrue(waitUntil { harness.http.request?.requestID == 41 })
        harness.http.respond(status: 200, headers: [BridgeHTTPHeader(name: "Content-Type", value: "text/plain")])
        harness.http.send(Data(repeating: 0x41, count: 400))
        harness.http.complete(.success(()))

        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { $0.messageType == .responseEnd && $0.requestID == 41 }
        })
        let response = harness.transport.frames.filter { $0.requestID == 41 }
        XCTAssertEqual(response.compactMap(\.messageType), [
            .responseStart, .responseHeader, .responseBodyChunk,
            .responseBodyChunk, .responseBodyChunk, .responseEnd,
        ])
        XCTAssertEqual(response.map(\.sequence), Array(0...5).map(UInt32.init))
        XCTAssertEqual(response.filter { $0.messageType == .responseBodyChunk }.map(\.payload.count), [192, 192, 16])
    }

    func testDenialPreventsNetworkSideEffect() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.connect()
        harness.sendHello()
        XCTAssertTrue(waitUntil { harness.prompt.requestCount == 1 })
        harness.prompt.decide(.deny)
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().state == .permissionDenied })

        harness.sendGET(requestID: 10)
        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains {
                $0.messageType == .error && $0.requestID == 10
            }
        })
        XCTAssertNil(harness.http.request)
    }

    func testSavedPermissionIsBoundToIdentityAndSkipsPrompt() throws {
        let store = InMemoryPermissionStore()
        store.grant(Harness.identity)
        let harness = Harness(permissionStore: store)
        defer { harness.stop() }
        try harness.connect()
        harness.sendHello()

        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().permission == .alwaysAllowed })
        XCTAssertEqual(harness.prompt.requestCount, 0)
        let status = harness.transport.frames.last { $0.messageType == .permissionStatus }
        XCTAssertEqual(status?.payload, Data([BridgePermissionState.alwaysAllowed.rawValue, 1]))
    }

    func testIncompatibleVersionClosesCandidateBeforePrompt() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.connect()
        var hello = Harness.helloFrame
        hello.major = 2
        harness.transport.inject(try FIBPCodec.encode(hello))

        XCTAssertTrue(waitUntil { harness.transport.closeCount > 0 })
        XCTAssertEqual(harness.prompt.requestCount, 0)
        XCTAssertNil(harness.http.request)
    }

    func testNegotiatedPayloadMustFitMandatoryHelloAcknowledgment() throws {
        do {
            let harness = Harness()
            defer { harness.stop() }
            try harness.connect()
            harness.sendHello(Harness.makeHello(maximumReceivePayload: 27))
            XCTAssertTrue(waitUntil { harness.transport.closeCount > 0 })
            XCTAssertEqual(harness.prompt.requestCount, 0)
        }
        do {
            let harness = Harness()
            defer { harness.stop() }
            try harness.connect()
            harness.sendHello(Harness.makeHello(maximumReceivePayload: 28))
            XCTAssertTrue(waitUntil { harness.prompt.requestCount == 1 })
            let ack = harness.transport.frames.last { $0.messageType == .helloAck }
            XCTAssertEqual(ack?.payload.count, 28)
        }
    }

    func testERRORNeverTriggersERRORBeforeHelloOrOnVersionMismatch() throws {
        do {
            let harness = Harness()
            defer { harness.stop() }
            try harness.connect()
            let before = harness.transport.frames.filter { $0.messageType == .error }.count
            harness.inject(FIBPFrame(messageType: .error, requestID: 0,
                                     sequence: 0, payload: Data()))
            Thread.sleep(forTimeInterval: 0.05)
            XCTAssertEqual(harness.transport.frames.filter { $0.messageType == .error }.count, before)
            harness.sendHello()
            XCTAssertTrue(waitUntil { harness.prompt.requestCount == 1 })
        }
        do {
            let harness = Harness()
            defer { harness.stop() }
            try harness.connect()
            let before = harness.transport.frames.filter { $0.messageType == .error }.count
            var mismatched = FIBPFrame(messageType: .error, requestID: 0,
                                       sequence: 0, payload: Data())
            mismatched.major = 2
            harness.inject(mismatched)
            XCTAssertTrue(waitUntil { harness.transport.closeCount > 0 })
            XCTAssertEqual(harness.transport.frames.filter { $0.messageType == .error }.count, before)
        }
    }

    func testSequenceGapAbortsIncompleteRequest() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        let start = FIBPRequestStart(
            method: .get,
            timeoutMilliseconds: 1_000,
            url: "https://example.com/",
            declaredBodyLength: 0,
            declaredHeaderCount: 0
        )
        harness.inject(FIBPFrame(messageType: .requestStart, requestID: 55, sequence: 0, payload: start.encoded))
        harness.inject(FIBPFrame(messageType: .requestEnd, flags: [.final], requestID: 55, sequence: 2))

        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { frame in
                guard frame.messageType == .error, frame.requestID == 55, frame.payload.count >= 2 else { return false }
                return [UInt8](frame.payload).littleEndianUInt16ForTest == FIBPErrorCode.sequenceGap.rawValue
            }
        })
        XCTAssertNil(harness.http.request)
    }

    func testDuplicateRequestIDNeverExecutesTwice() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        harness.sendGET(requestID: 77)
        XCTAssertTrue(waitUntil { harness.http.executeCount == 1 })
        harness.http.respond(status: 204)
        harness.http.complete(.success(()))
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().activeRequestID == nil })

        harness.sendGET(requestID: 77)
        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { frame in
                guard frame.messageType == .error, frame.requestID == 77, frame.payload.count >= 2 else { return false }
                return [UInt8](frame.payload).littleEndianUInt16ForTest == FIBPErrorCode.duplicateRequestID.rawValue
            }
        })
        XCTAssertEqual(harness.http.executeCount, 1)
    }

    func testUSBRemovalCancelsActiveRequestImmediately() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        harness.sendGET(requestID: 90)
        XCTAssertTrue(waitUntil { harness.http.executeCount == 1 })

        harness.monitor.remove(Harness.device)
        XCTAssertTrue(waitUntil { harness.http.cancelCount > 0 })
        XCTAssertFalse(harness.coordinator.snapshot().isConnected)
        XCTAssertNil(harness.coordinator.snapshot().activeRequestID)
    }

    func testNetworkTimeoutProducesBoundedRequestError() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        harness.sendGET(requestID: 99)
        XCTAssertTrue(waitUntil { harness.http.executeCount == 1 })
        harness.http.complete(.failure(.timeout))

        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { frame in
                guard frame.messageType == .error, frame.requestID == 99, frame.payload.count >= 2 else { return false }
                return [UInt8](frame.payload).littleEndianUInt16ForTest == FIBPErrorCode.timeout.rawValue
            }
        })
    }

    func test404StatusIsForwardedAsOrdinaryHTTPResponse() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        harness.sendGET(requestID: 404)
        XCTAssertTrue(waitUntil { harness.http.executeCount == 1 })
        harness.http.respond(status: 404)
        harness.http.send(Data("not found".utf8))
        harness.http.complete(.success(()))

        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { $0.messageType == .responseEnd && $0.requestID == 404 }
        })
        let start = harness.transport.frames.first {
            $0.messageType == .responseStart && $0.requestID == 404
        }
        XCTAssertEqual(start.map { [UInt8]($0.payload).littleEndianUInt16ForTest }, 404)
    }

    func testOversizedResponseIsTruncatedAtNegotiatedLimit() throws {
        let negotiatedLimit = 4_096
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection(hello: Harness.makeHello(
            maximumResponseBytes: UInt32(negotiatedLimit)
        ))
        harness.sendGET(requestID: 120)
        XCTAssertTrue(waitUntil { harness.http.executeCount == 1 })
        harness.http.respond(status: 200)
        harness.http.send(Data(repeating: 0x42, count: negotiatedLimit + 1))
        XCTAssertTrue(waitUntil { harness.http.cancelCount > 0 })
        harness.http.complete(.failure(.cancelled))

        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { $0.messageType == .responseEnd && $0.requestID == 120 }
        })
        let frames = harness.transport.frames.filter { $0.requestID == 120 }
        let bodyBytes = frames.filter { $0.messageType == .responseBodyChunk }
            .reduce(0) { $0 + $1.payload.count }
        XCTAssertEqual(bodyBytes, negotiatedLimit)
        let end = frames.last { $0.messageType == .responseEnd }
        XCTAssertEqual(end?.payload.first, 1)
        XCTAssertTrue(end?.flags.contains(.truncated) == true)
    }

    func testUserCancellationCancelsNetworkAndNotifiesFlipper() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        harness.sendGET(requestID: 130)
        XCTAssertTrue(waitUntil { harness.http.executeCount == 1 })
        let baseline = harness.http.cancelCount
        harness.coordinator.cancelActiveRequest()

        XCTAssertTrue(waitUntil { harness.http.cancelCount > baseline })
        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { $0.messageType == .cancel && $0.requestID == 130 }
        })
        XCTAssertNil(harness.coordinator.snapshot().activeRequestID)
    }

    func testGrantAlwaysThenRevokeUpdatesStoreAndCancelsWork() throws {
        let store = InMemoryPermissionStore()
        let harness = Harness(permissionStore: store)
        defer { harness.stop() }
        try harness.connect()
        harness.sendHello()
        XCTAssertTrue(waitUntil { harness.prompt.requestCount == 1 })

        harness.coordinator.grantAlwaysForCurrentDevice()
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().permission == .alwaysAllowed })
        XCTAssertTrue(store.contains(Harness.identity))
        harness.sendGET(requestID: 140)
        XCTAssertTrue(waitUntil { harness.http.executeCount == 1 })
        let baseline = harness.http.cancelCount

        harness.coordinator.revokeCurrentDevicePermission()
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().permission == .denied })
        XCTAssertTrue(waitUntil { harness.http.cancelCount > baseline })
        XCTAssertFalse(store.contains(Harness.identity))
        XCTAssertTrue(harness.transport.frames.contains {
            $0.messageType == .permissionStatus && $0.payload.first == BridgePermissionState.denied.rawValue
        })
    }

    func testUnnegotiatedPOSTAndHeadersAreRejectedBeforeHTTPClient() throws {
        let capabilities: FIBPCapabilities = [.httpsGET, .cancellation]
        let limitedHello = Harness.makeHello(capabilities: capabilities)
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection(hello: limitedHello)

        let post = FIBPRequestStart(
            method: .post,
            timeoutMilliseconds: 1_000,
            url: "https://example.com/post",
            declaredBodyLength: 0,
            declaredHeaderCount: 0
        )
        harness.inject(FIBPFrame(messageType: .requestStart, requestID: 201,
                                 sequence: 0, payload: post.encoded))
        let withHeader = FIBPRequestStart(
            method: .get,
            timeoutMilliseconds: 1_000,
            url: "https://example.com/get",
            declaredBodyLength: 0,
            declaredHeaderCount: 1
        )
        harness.inject(FIBPFrame(messageType: .requestStart, requestID: 202,
                                 sequence: 0, payload: withHeader.encoded))

        XCTAssertTrue(waitUntil {
            Set(harness.transport.frames.filter { $0.messageType == .error }.map(\.requestID))
                .isSuperset(of: [201, 202])
        })
        XCTAssertEqual(harness.http.executeCount, 0)
    }

    func testNegotiatedPOSTAssemblesHeaderAndBodyChunksInOrder() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        let start = FIBPRequestStart(
            method: .post,
            timeoutMilliseconds: 9_000,
            url: "https://example.com/post",
            declaredBodyLength: 5,
            declaredHeaderCount: 1
        )
        harness.inject(FIBPFrame(messageType: .requestStart, requestID: 205,
                                 sequence: 0, payload: start.encoded))
        harness.inject(FIBPFrame(
            messageType: .requestHeader,
            requestID: 205,
            sequence: 1,
            payload: BridgeHTTPHeader(name: "Content-Type", value: "text/plain").encoded
        ))
        harness.inject(FIBPFrame(messageType: .requestBodyChunk, requestID: 205,
                                 sequence: 2, payload: Data("he".utf8)))
        harness.inject(FIBPFrame(messageType: .requestBodyChunk, requestID: 205,
                                 sequence: 3, payload: Data("llo".utf8)))
        harness.inject(FIBPFrame(messageType: .requestEnd, flags: [.final],
                                 requestID: 205, sequence: 4))

        XCTAssertTrue(waitUntil { harness.http.request?.requestID == 205 })
        XCTAssertEqual(harness.http.request?.method, .post)
        XCTAssertEqual(harness.http.request?.body, Data("hello".utf8))
        XCTAssertEqual(
            harness.http.request?.headers,
            [BridgeHTTPHeader(name: "Content-Type", value: "text/plain")]
        )
        XCTAssertEqual(harness.http.request?.timeout ?? -1, 9, accuracy: 0.000_1)
    }

    func testResponseHeadersAreSuppressedWhenCapabilityWasNotNegotiated() throws {
        let limitedHello = Harness.makeHello(capabilities: [.httpsGET, .cancellation])
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection(hello: limitedHello)
        harness.sendGET(requestID: 203)
        XCTAssertTrue(waitUntil { harness.http.executeCount == 1 })
        harness.http.respond(
            status: 200,
            headers: [BridgeHTTPHeader(name: "Content-Type", value: "text/plain")]
        )
        harness.http.complete(.success(()))

        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { $0.messageType == .responseEnd && $0.requestID == 203 }
        })
        let frames = harness.transport.frames.filter { $0.requestID == 203 }
        XCTAssertFalse(frames.contains { $0.messageType == .responseHeader })
        let start = frames.first { $0.messageType == .responseStart }
        XCTAssertEqual(start?.payload.dropFirst(2).first, 0)
    }

    func testValidatedTimeoutIsPassedToHTTPClient() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        let start = FIBPRequestStart(
            method: .get,
            timeoutMilliseconds: 12_345,
            url: "https://example.com/",
            declaredBodyLength: 0,
            declaredHeaderCount: 0
        )
        harness.inject(FIBPFrame(messageType: .requestStart, requestID: 204,
                                 sequence: 0, payload: start.encoded))
        harness.inject(FIBPFrame(messageType: .requestEnd, flags: [.final],
                                 requestID: 204, sequence: 1))
        XCTAssertTrue(waitUntil { harness.http.request != nil })
        XCTAssertEqual(harness.http.request?.timeout ?? -1, 12.345, accuracy: 0.000_1)
    }

    func testRemoteErrorAdvancesControlSequenceWithoutErrorStorm() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        let errorsBefore = harness.transport.frames.filter { $0.messageType == .error }.count
        let payload = FIBPPayloadEncoder.error(
            code: .cancelled,
            scope: 1,
            offendingType: FIBPMessageType.cancel.rawValue,
            detail: "cancelled"
        )
        harness.inject(FIBPFrame(messageType: .error, requestID: 999,
                                 sequence: 1, payload: payload))
        let pingToken = Data([0, 1, 2, 3, 4, 5, 6, 7])
        harness.inject(FIBPFrame(messageType: .ping, requestID: 0,
                                 sequence: 2, payload: pingToken))

        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { $0.messageType == .pong && $0.payload == pingToken }
        })
        XCTAssertEqual(harness.transport.frames.filter { $0.messageType == .error }.count, errorsBefore)
    }

    func testMalformedERRORConsumesExpectedSequenceSilently() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        let errorsBefore = harness.transport.frames.filter { $0.messageType == .error }.count
        harness.inject(FIBPFrame(messageType: .error, requestID: 0,
                                 sequence: 1, payload: Data([0x00])))
        let token = Data([9, 8, 7, 6, 5, 4, 3, 2])
        harness.inject(FIBPFrame(messageType: .ping, requestID: 0,
                                 sequence: 2, payload: token))
        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { $0.messageType == .pong && $0.payload == token }
        })
        XCTAssertEqual(harness.transport.frames.filter { $0.messageType == .error }.count, errorsBefore)
    }

    func testCorruptERRORDoesNotReplyOrConsumeControlSequence() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        let errorsBefore = harness.transport.frames.filter { $0.messageType == .error }.count
        var corrupt = try FIBPCodec.encode(FIBPFrame(
            messageType: .error,
            requestID: 0,
            sequence: 1,
            payload: FIBPPayloadEncoder.error(
                code: .cancelled,
                scope: 0,
                offendingType: FIBPMessageType.cancel.rawValue,
                detail: "cancelled"
            )
        ))
        corrupt[corrupt.count - 1] ^= 0xFF
        harness.transport.inject(corrupt)
        let token = Data([2, 3, 4, 5, 6, 7, 8, 9])
        harness.inject(FIBPFrame(messageType: .ping, requestID: 0,
                                 sequence: 1, payload: token))
        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { $0.messageType == .pong && $0.payload == token }
        })
        XCTAssertEqual(harness.transport.frames.filter { $0.messageType == .error }.count, errorsBefore)
    }

    func testDuplicateHelloReplaysExactPermissionFramesNotLatestPingSequence() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        let token = Data([1, 2, 3, 4, 5, 6, 7, 8])
        harness.inject(FIBPFrame(messageType: .ping, requestID: 0,
                                 sequence: 1, payload: token))
        XCTAssertTrue(waitUntil { harness.transport.frames.contains { $0.messageType == .pong } })

        harness.sendHello()
        XCTAssertTrue(waitUntil {
            harness.transport.frames.filter { $0.messageType == .helloAck }.count == 2
        })
        let required = harness.transport.frames.filter { $0.messageType == .permissionRequired }
        let statuses = harness.transport.frames.filter { $0.messageType == .permissionStatus }
        XCTAssertEqual(required.map(\.sequence), [1, 1])
        XCTAssertEqual(statuses.map(\.sequence), [2, 2])
        XCTAssertEqual(harness.transport.frames.last { $0.messageType == .pong }?.sequence, 3)
    }

    func testSameNonceHELLOWithChangedFieldsClosesCandidate() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        harness.sendHello(Harness.makeHello(capabilities: [.httpsGET, .cancellation]))
        XCTAssertTrue(waitUntil { harness.transport.closeCount > 0 })
        XCTAssertFalse(harness.coordinator.snapshot().isConnected)
        XCTAssertEqual(harness.prompt.requestCount, 1)
    }

    func testMissingPONGClosesStaleConnection() throws {
        let harness = Harness(idleSessionTimeout: 0.01, pongTimeout: 0.05)
        defer { harness.stop() }
        try harness.readyConnection()
        XCTAssertTrue(waitUntil { harness.transport.frames.contains { $0.messageType == .ping } })
        XCTAssertTrue(waitUntil { harness.transport.closeCount > 0 })
        XCTAssertFalse(harness.coordinator.snapshot().isConnected)
    }

    func testRemoteCancelDuringAssemblyReleasesSingleRequestSlot() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        let start = FIBPRequestStart(
            method: .get,
            timeoutMilliseconds: 1_000,
            url: "https://example.com/first",
            declaredBodyLength: 0,
            declaredHeaderCount: 0
        )
        harness.inject(FIBPFrame(messageType: .requestStart, requestID: 301,
                                 sequence: 0, payload: start.encoded))
        harness.inject(FIBPFrame(messageType: .cancel, flags: [.final],
                                 requestID: 301, sequence: 1, payload: Data([0])))
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().activeRequestID == nil })

        harness.sendGET(requestID: 302)
        XCTAssertTrue(waitUntil { harness.http.request?.requestID == 302 })
    }

    func testInvalidCANCELSequenceHasNoCancellationSideEffect() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        harness.sendGET(requestID: 306)
        XCTAssertTrue(waitUntil { harness.http.request?.requestID == 306 })
        let cancelBaseline = harness.http.cancelCount

        harness.inject(FIBPFrame(messageType: .cancel, flags: [.final],
                                 requestID: 306, sequence: 1, payload: Data([0])))
        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { frame in
                guard frame.messageType == .error, frame.requestID == 306,
                      frame.payload.count >= 2 else { return false }
                return [UInt8](frame.payload).littleEndianUInt16ForTest
                    == FIBPErrorCode.duplicateSequence.rawValue
            }
        })
        XCTAssertEqual(harness.http.cancelCount, cancelBaseline)
        XCTAssertEqual(harness.coordinator.snapshot().activeRequestID, 306)

        harness.inject(FIBPFrame(messageType: .cancel, flags: [.final],
                                 requestID: 306, sequence: 3, payload: Data([0])))
        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { frame in
                guard frame.messageType == .error, frame.requestID == 306,
                      frame.payload.count >= 2 else { return false }
                return [UInt8](frame.payload).littleEndianUInt16ForTest
                    == FIBPErrorCode.sequenceGap.rawValue
            }
        })
        XCTAssertEqual(harness.http.cancelCount, cancelBaseline)
        XCTAssertEqual(harness.coordinator.snapshot().activeRequestID, 306)
    }

    func testInvalidCANCELReasonPreservesRequestAndDoesNotConsumeSequence() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        harness.sendGET(requestID: 307)
        XCTAssertTrue(waitUntil { harness.http.request?.requestID == 307 })
        let cancelBaseline = harness.http.cancelCount

        harness.inject(FIBPFrame(messageType: .cancel, flags: [.final],
                                 requestID: 307, sequence: 2, payload: Data([4])))
        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { frame in
                guard frame.messageType == .error, frame.requestID == 307,
                      frame.payload.count >= 2 else { return false }
                return [UInt8](frame.payload).littleEndianUInt16ForTest
                    == FIBPErrorCode.invalidRequest.rawValue
            }
        })
        XCTAssertEqual(harness.http.cancelCount, cancelBaseline)
        XCTAssertEqual(harness.coordinator.snapshot().activeRequestID, 307)

        harness.inject(FIBPFrame(messageType: .cancel, flags: [.final],
                                 requestID: 307, sequence: 2, payload: Data([0])))
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().activeRequestID == nil })
        XCTAssertTrue(waitUntil { harness.http.cancelCount > cancelBaseline })
    }

    func testInvalidDISCONNECTReasonPreservesConnectionAndControlSequence() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        harness.inject(FIBPFrame(messageType: .disconnect, requestID: 0,
                                 sequence: 1, payload: Data([3])))
        XCTAssertTrue(waitUntil {
            harness.transport.frames.contains { frame in
                guard frame.messageType == .error, frame.requestID == 0,
                      frame.payload.count >= 2 else { return false }
                return [UInt8](frame.payload).littleEndianUInt16ForTest
                    == FIBPErrorCode.invalidRequest.rawValue
            }
        })
        XCTAssertTrue(harness.coordinator.snapshot().isConnected)

        harness.inject(FIBPFrame(messageType: .disconnect, requestID: 0,
                                 sequence: 1, payload: Data([0])))
        XCTAssertTrue(waitUntil { !harness.coordinator.snapshot().isConnected })
    }

    func testParserOverflowResetsPartiallyAssembledRequest() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.readyConnection()
        let start = FIBPRequestStart(
            method: .get,
            timeoutMilliseconds: 1_000,
            url: "https://example.com/first",
            declaredBodyLength: 0,
            declaredHeaderCount: 0
        )
        harness.inject(FIBPFrame(messageType: .requestStart, requestID: 303,
                                 sequence: 0, payload: start.encoded))
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().activeRequestID == 303 })
        harness.transport.inject(Data(
            repeating: 0xAA,
            count: BridgeConfiguration.maximumBufferedSerialBytes + 1
        ))
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().activeRequestID == nil })

        harness.sendGET(requestID: 304)
        XCTAssertTrue(waitUntil { harness.http.request?.requestID == 304 })
    }

    func testMissingRequestEndTimesOutAndReleasesSlot() throws {
        let harness = Harness(requestAssemblyTimeout: 0.05)
        defer { harness.stop() }
        try harness.readyConnection()
        let start = FIBPRequestStart(
            method: .get,
            timeoutMilliseconds: 1_000,
            url: "https://example.com/incomplete",
            declaredBodyLength: 0,
            declaredHeaderCount: 0
        )
        harness.inject(FIBPFrame(messageType: .requestStart, requestID: 305,
                                 sequence: 0, payload: start.encoded))
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().activeRequestID == 305 })
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().activeRequestID == nil })
        XCTAssertTrue(harness.transport.frames.contains { frame in
            guard frame.messageType == .error, frame.requestID == 305, frame.payload.count >= 2 else {
                return false
            }
            return [UInt8](frame.payload).littleEndianUInt16ForTest == FIBPErrorCode.timeout.rawValue
        })
    }

    func testIdlePingIsSuppressedWhilePermissionIsPendingOrDenied() throws {
        let harness = Harness(idleSessionTimeout: 0.01, pongTimeout: 0.05)
        defer { harness.stop() }
        try harness.connect()
        harness.sendHello()
        XCTAssertTrue(waitUntil { harness.prompt.requestCount == 1 })
        Thread.sleep(forTimeInterval: 0.35)
        XCTAssertFalse(harness.transport.frames.contains { $0.messageType == .ping })

        harness.prompt.decide(.deny)
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().permission == .denied })
        Thread.sleep(forTimeInterval: 0.35)
        XCTAssertFalse(harness.transport.frames.contains { $0.messageType == .ping })
        XCTAssertTrue(harness.coordinator.snapshot().isConnected)
    }

    func testRejectedRequestIDFloodDoesNotPoisonBoundedReplayState() throws {
        let harness = Harness()
        defer { harness.stop() }
        try harness.connect()
        harness.sendHello()
        XCTAssertTrue(waitUntil { harness.prompt.requestCount == 1 })
        harness.prompt.decide(.deny)
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().permission == .denied })
        let start = FIBPRequestStart(
            method: .get,
            timeoutMilliseconds: 1_000,
            url: "https://example.com/rejected",
            declaredBodyLength: 0,
            declaredHeaderCount: 0
        )
        for requestID in UInt32(1)...UInt32(128) {
            harness.inject(FIBPFrame(messageType: .requestStart, requestID: requestID,
                                     sequence: 0, payload: start.encoded))
        }
        harness.coordinator.grantAlwaysForCurrentDevice()
        XCTAssertTrue(waitUntil { harness.coordinator.snapshot().permission == .alwaysAllowed })

        // Rejected IDs are not accepted replay state; the first valid request can
        // still establish the single monotonic watermark without growing a Set.
        harness.sendGET(requestID: 1)
        XCTAssertTrue(waitUntil { harness.http.request?.requestID == 1 })
    }
}

private final class Harness {
    static let device = SerialDevice(
        registryID: 123,
        calloutPath: "/dev/cu.mock-fibp",
        vendorID: BridgeConfiguration.flipperVendorID,
        productID: BridgeConfiguration.flipperProductID,
        interfaceNumber: 2
    )
    static func makeHello(
        capabilities: FIBPCapabilities = .helperSupported,
        maximumReceivePayload: UInt16 = UInt16(BridgeConfiguration.maximumWirePayload),
        maximumResponseBytes: UInt32 = UInt32(BridgeConfiguration.maximumResponseBytes)
    ) -> FIBPHello {
        FIBPHello(
        capabilities: capabilities,
        maximumReceivePayload: maximumReceivePayload,
        maximumResponseBytes: maximumResponseBytes,
        clientNonce: 0x0102_0304_0506_0708,
        model: "Flipper Zero",
        name: "Miço",
        idType: 1,
        deviceID: Data([1, 2, 3, 4, 5, 6, 7, 8]),
        appVersion: "0.1.0"
        )
    }
    static let hello = makeHello()
    static var identity: FlipperIdentity {
        FlipperIdentity(
            model: hello.model,
            name: hello.name,
            idType: hello.idType,
            deviceID: hello.deviceID,
            appVersion: hello.appVersion,
            protocolMajor: BridgeConfiguration.protocolMajor,
            protocolMinor: BridgeConfiguration.protocolMinor
        )
    }
    static var helloFrame: FIBPFrame {
        FIBPFrame(messageType: .hello, requestID: 0, sequence: 0,
                  payload: try! hello.encoded())
    }

    let monitor = ManualSerialDeviceMonitor()
    let transport = MockSerialTransport()
    let prompt = MockPermissionPrompt()
    let http = MockHTTPClient()
    let coordinator: BridgeCoordinator

    init(
        permissionStore: any PermissionStoring = InMemoryPermissionStore(),
        idleSessionTimeout: TimeInterval = BridgeConfiguration.idleSessionTimeout,
        pongTimeout: TimeInterval = BridgeConfiguration.pongTimeout,
        requestAssemblyTimeout: TimeInterval = BridgeConfiguration.requestAssemblyTimeout
    ) {
        coordinator = BridgeCoordinator(
            monitor: monitor,
            transport: transport,
            permissions: permissionStore,
            permissionPrompt: prompt,
            httpClient: http,
            idleSessionTimeout: idleSessionTimeout,
            pongTimeout: pongTimeout,
            requestAssemblyTimeout: requestAssemblyTimeout
        )
    }

    func connect() throws {
        try coordinator.start()
        monitor.add(Self.device)
        XCTAssertTrue(waitUntil { self.transport.isOpen })
    }

    func sendHello(_ hello: FIBPHello = Harness.hello) {
        inject(FIBPFrame(messageType: .hello, requestID: 0, sequence: 0,
                         payload: try! hello.encoded()))
    }

    func readyConnection(hello: FIBPHello = Harness.hello) throws {
        try connect()
        sendHello(hello)
        XCTAssertTrue(waitUntil { self.prompt.requestCount == 1 })
        prompt.decide(.allowOnce)
        XCTAssertTrue(waitUntil { self.coordinator.snapshot().state == .ready })
    }

    func sendGET(requestID: UInt32) {
        let start = FIBPRequestStart(
            method: .get,
            timeoutMilliseconds: 2_000,
            url: "https://example.com/demo",
            declaredBodyLength: 0,
            declaredHeaderCount: 0
        )
        inject(FIBPFrame(messageType: .requestStart, requestID: requestID,
                         sequence: 0, payload: start.encoded))
        inject(FIBPFrame(messageType: .requestEnd, flags: [.final],
                         requestID: requestID, sequence: 1))
    }

    func inject(_ frame: FIBPFrame) {
        transport.inject(try! FIBPCodec.encode(frame))
    }

    func stop() { coordinator.stop() }
}

private final class MockSerialTransport: SerialTransporting {
    var onReceive: ((Data) -> Void)?
    var onDisconnect: ((Error?) -> Void)?
    private let lock = NSLock()
    private var openState = false
    private var sent = [Data]()
    private(set) var closeCount = 0

    var isOpen: Bool { lock.withLock { openState } }
    var frames: [FIBPFrame] {
        lock.withLock {
            sent.flatMap { data -> [FIBPFrame] in
                FIBPStreamParser().feed(data).compactMap {
                    if case let .frame(frame) = $0 { return frame }
                    return nil
                }
            }
        }
    }

    func open(device: SerialDevice) throws { lock.withLock { openState = true } }
    func send(_ data: Data) throws {
        try lock.withLock {
            guard openState else { throw SerialTransportError.notOpen }
            sent.append(data)
        }
    }
    func close() {
        lock.withLock {
            if openState { closeCount += 1 }
            openState = false
        }
    }
    func inject(_ data: Data) { onReceive?(data) }
}

private final class MockPermissionPrompt: PermissionPrompting {
    private let lock = NSLock()
    private var completion: ((BridgePermissionDecision) -> Void)?
    private(set) var requestCount = 0

    func requestPermission(
        for identity: FlipperIdentity,
        completion: @escaping (BridgePermissionDecision) -> Void
    ) {
        lock.withLock {
            requestCount += 1
            self.completion = completion
        }
    }

    func cancelPendingPrompt() { lock.withLock { completion = nil } }

    func decide(_ decision: BridgePermissionDecision) {
        let callback = lock.withLock { () -> ((BridgePermissionDecision) -> Void)? in
            defer { completion = nil }
            return completion
        }
        callback?(decision)
    }
}

private final class MockHTTPClient: BridgeHTTPClient {
    private let lock = NSLock()
    private var onResponse: ((BridgeHTTPResponseMetadata) -> Void)?
    private var onData: ((Data) -> Void)?
    private var completion: ((Result<Void, BridgeNetworkError>) -> Void)?
    private(set) var request: BridgeHTTPRequest?
    private(set) var executeCount = 0
    private(set) var cancelCount = 0

    func execute(
        _ request: BridgeHTTPRequest,
        onResponse: @escaping (BridgeHTTPResponseMetadata) -> Void,
        onData: @escaping (Data) -> Void,
        completion: @escaping (Result<Void, BridgeNetworkError>) -> Void
    ) {
        lock.withLock {
            self.request = request
            self.executeCount += 1
            self.onResponse = onResponse
            self.onData = onData
            self.completion = completion
        }
    }

    func cancel() { lock.withLock { cancelCount += 1 } }
    func invalidate() { cancel() }

    func respond(status: Int, headers: [BridgeHTTPHeader] = []) {
        let callback = lock.withLock { onResponse }
        callback?(BridgeHTTPResponseMetadata(statusCode: status, headers: headers, expectedBodyLength: -1))
    }
    func send(_ data: Data) { lock.withLock { onData }?(data) }
    func complete(_ result: Result<Void, BridgeNetworkError>) {
        let callback = lock.withLock { () -> ((Result<Void, BridgeNetworkError>) -> Void)? in
            defer { completion = nil }
            return completion
        }
        callback?(result)
    }
}

private func waitUntil(
    timeout: TimeInterval = 2,
    condition: () -> Bool
) -> Bool {
    let deadline = Date().addingTimeInterval(timeout)
    repeat {
        if condition() { return true }
        Thread.sleep(forTimeInterval: 0.005)
    } while Date() < deadline
    return condition()
}

private extension NSLock {
    func withLock<T>(_ body: () throws -> T) rethrows -> T {
        lock(); defer { unlock() }
        return try body()
    }
}

private extension Array where Element == UInt8 {
    var littleEndianUInt16ForTest: UInt16 {
        UInt16(self[0]) | (UInt16(self[1]) << 8)
    }
}
