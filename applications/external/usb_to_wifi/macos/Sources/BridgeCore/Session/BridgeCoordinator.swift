import Foundation

/// Owns exactly one FIBP connection and one in-flight HTTPS request. All mutable
/// state is confined to `queue`; callbacks from IOKit, POSIX, AppKit, and
/// URLSession are marshalled onto it before they touch session state.
public final class BridgeCoordinator: @unchecked Sendable {
    private struct IncomingRequest {
        let requestID: UInt32
        let start: FIBPRequestStart
        var headers = [BridgeHTTPHeader]()
        var aggregateHeaderBytes = 0
        var body = Data()
        var bodyStarted = false
        var requestEnded = false
        var assemblyDeadline: Date
        var expectedSequence: UInt32 = 1
        var responseSequence: UInt32 = 0
        var responseStarted = false
        var responseBytes = 0
        var responseEnded = false
        var locallyCancelled = false
        var truncated = false
    }

    public var onStatusChange: ((BridgeStatusSnapshot) -> Void)?

    private let monitor: any SerialDeviceMonitoring
    private let transport: any SerialTransporting
    private let permissions: any PermissionStoring
    private let permissionPrompt: any PermissionPrompting
    private let httpClient: any BridgeHTTPClient
    private let idleSessionTimeout: TimeInterval
    private let pongTimeout: TimeInterval
    private let requestAssemblyTimeout: TimeInterval
    public let diagnostics: DiagnosticsLog

    private let queue = DispatchQueue(label: "fibp.bridge-session", qos: .userInitiated)
    private let queueKey = DispatchSpecificKey<UInt8>()
    private var maintenanceTimer: DispatchSourceTimer?
    private var running = false
    private var candidates = [UInt64: SerialDevice]()
    private var attemptedCandidateIDs = Set<UInt64>()
    private var currentDevice: SerialDevice?
    private var connectionToken = UUID()
    private var parser = FIBPStreamParser()
    private var helloDeadline: Date?
    private var retryScheduled = false
    private var lastActivity = Date()

    private var state: BridgeConnectionState = .disconnected
    private var statusText = "Flipper not connected"
    private var hello: FIBPHello?
    private var identity: FlipperIdentity?
    private var permission: BridgePermissionState?
    private var serverNonce: UInt64 = 0
    private var negotiatedCapabilities: FIBPCapabilities = []
    private var negotiatedPayload = BridgeConfiguration.maximumWirePayload
    private var negotiatedResponseBytes = BridgeConfiguration.maximumResponseBytes
    private var nextServerControlSequence: UInt32 = 1
    private var expectedClientControlSequence: UInt32 = 1
    private var permissionReplayFrames = [FIBPFrame]()
    // FIBP v1 requires monotonically increasing non-zero IDs and a fresh USB
    // session before wrap. One scalar therefore provides bounded replay defense.
    private var lastAcceptedRequestID: UInt32?
    private var activeRequest: IncomingRequest?
    private var lastErrorFrameDate = Date.distantPast
    private var pendingPingToken: Data?
    private var pingDeadline: Date?

    public init(
        monitor: any SerialDeviceMonitoring,
        transport: any SerialTransporting,
        permissions: any PermissionStoring,
        permissionPrompt: any PermissionPrompting,
        httpClient: any BridgeHTTPClient,
        diagnostics: DiagnosticsLog = DiagnosticsLog(),
        idleSessionTimeout: TimeInterval = BridgeConfiguration.idleSessionTimeout,
        pongTimeout: TimeInterval = BridgeConfiguration.pongTimeout,
        requestAssemblyTimeout: TimeInterval = BridgeConfiguration.requestAssemblyTimeout
    ) {
        self.monitor = monitor
        self.transport = transport
        self.permissions = permissions
        self.permissionPrompt = permissionPrompt
        self.httpClient = httpClient
        self.diagnostics = diagnostics
        self.idleSessionTimeout = idleSessionTimeout
        self.pongTimeout = pongTimeout
        self.requestAssemblyTimeout = requestAssemblyTimeout
        queue.setSpecific(key: queueKey, value: 1)
    }

    deinit { stop() }

    public func start() throws {
        var startError: Error?
        performSync {
            guard !running else { return }
            monitor.onDeviceAdded = { [weak self] device in
                self?.queue.async { self?.deviceAdded(device) }
            }
            monitor.onDeviceRemoved = { [weak self] device in
                self?.queue.async { self?.deviceRemoved(device) }
            }
            do {
                try monitor.start()
                running = true
                startMaintenanceTimer()
                diagnostics.append(.info, "Serial device monitoring started.")
            } catch {
                startError = error
                diagnostics.append(.error, "Serial device monitoring could not start.")
            }
        }
        if let startError { throw startError }
    }

    public func stop() {
        performSync {
            guard running || currentDevice != nil else { return }
            running = false
            bestEffortDisconnect(reason: 0)
            cancelCurrentConnection(closeTransport: true)
            monitor.stop()
            monitor.onDeviceAdded = nil
            monitor.onDeviceRemoved = nil
            maintenanceTimer?.cancel()
            maintenanceTimer = nil
            candidates.removeAll()
            attemptedCandidateIDs.removeAll()
            httpClient.invalidate()
            transition(.disconnected, "Flipper not connected")
            diagnostics.append(.info, "Bridge stopped.")
        }
    }

    public func snapshot() -> BridgeStatusSnapshot {
        performSync { makeSnapshot() }
    }

    public func grantAlwaysForCurrentDevice() {
        queue.async { [weak self] in
            guard let self, let identity = self.identity, self.currentDevice != nil else { return }
            self.permissionPrompt.cancelPendingPrompt()
            self.permissions.grant(identity)
            self.permission = .alwaysAllowed
            self.sendPermissionStatus(.alwaysAllowed, reason: 0)
            self.transition(.ready, "Internet access ready")
            self.diagnostics.append(.info, "Persistent device permission granted (\(identity.redactedID)).")
        }
    }

    public func revokeCurrentDevicePermission() {
        queue.async { [weak self] in
            guard let self, let identity = self.identity else { return }
            self.permissions.revoke(identity)
            self.permissionPrompt.cancelPendingPrompt()
            self.cancelRequest(sendCancel: true, reason: 3)
            self.permission = .denied
            self.pendingPingToken = nil
            self.pingDeadline = nil
            self.sendPermissionStatus(.denied, reason: 2)
            self.transition(.permissionDenied, "Permission revoked for this device")
            self.diagnostics.append(.info, "Device permission revoked (\(identity.redactedID)).")
        }
    }

    public func cancelActiveRequest() {
        queue.async { [weak self] in self?.cancelRequest(sendCancel: true, reason: 0) }
    }

    // MARK: - Device selection

    private func deviceAdded(_ device: SerialDevice) {
        guard running else { return }
        candidates[device.registryID] = device
        diagnostics.append(
            .info,
            "Flipper serial candidate found: \(device.calloutPath), interface \(device.interfaceNumber.map(String.init) ?? "unknown")."
        )
        attemptNextCandidateIfNeeded()
    }

    private func deviceRemoved(_ device: SerialDevice) {
        candidates.removeValue(forKey: device.registryID)
        attemptedCandidateIDs.remove(device.registryID)
        guard currentDevice?.registryID == device.registryID else { return }
        diagnostics.append(.warning, "USB serial connection was removed.")
        cancelCurrentConnection(closeTransport: true)
        transition(.disconnected, "Disconnected")
        attemptNextCandidateIfNeeded()
    }

    private func attemptNextCandidateIfNeeded() {
        guard running, currentDevice == nil else { return }
        let available = candidates.values
            .filter { !attemptedCandidateIDs.contains($0.registryID) }
            .sorted { lhs, rhs in
                let lhsPriority = lhs.interfaceNumber.map {
                    BridgeConfiguration.bridgeUSBInterfaceNumbers.contains($0) ? 0 : 2
                } ?? 1
                let rhsPriority = rhs.interfaceNumber.map {
                    BridgeConfiguration.bridgeUSBInterfaceNumbers.contains($0) ? 0 : 2
                } ?? 1
                if lhsPriority != rhsPriority { return lhsPriority < rhsPriority }
                return lhs.calloutPath < rhs.calloutPath
            }
        guard let candidate = available.first else {
            scheduleSlowCandidateRetryIfNeeded()
            return
        }

        attemptedCandidateIDs.insert(candidate.registryID)
        currentDevice = candidate
        connectionToken = UUID()
        let token = connectionToken
        parser.reset()
        resetSessionState()
        transport.onReceive = { [weak self] data in
            self?.queue.async {
                guard self?.connectionToken == token else { return }
                self?.received(data)
            }
        }
        transport.onDisconnect = { [weak self] error in
            self?.queue.async {
                guard let self, self.connectionToken == token else { return }
                if error != nil { self.diagnostics.append(.warning, "Serial transport closed unexpectedly.") }
                self.cancelCurrentConnection(closeTransport: false)
                self.transition(.disconnected, "Disconnected")
                self.attemptNextCandidateIfNeeded()
            }
        }
        transition(.openingSerial, "Opening USB serial connection")
        do {
            try transport.open(device: candidate)
            lastActivity = Date()
            helloDeadline = Date().addingTimeInterval(BridgeConfiguration.handshakeTimeout)
            transition(.waitingForHello, "Waiting for Flipper HELLO")
            diagnostics.append(.info, "Serial connection opened: \(candidate.calloutPath).")
        } catch {
            diagnostics.append(.warning, "Serial candidate could not be opened: \(candidate.calloutPath).")
            currentDevice = nil
            transport.onReceive = nil
            transport.onDisconnect = nil
            attemptNextCandidateIfNeeded()
        }
    }

    private func failCandidateBeforeHandshake(_ reason: String) {
        diagnostics.append(.warning, reason)
        cancelCurrentConnection(closeTransport: true)
        transition(.disconnected, "Helper protocol channel not found")
        attemptNextCandidateIfNeeded()
    }

    private func scheduleSlowCandidateRetryIfNeeded() {
        guard !candidates.isEmpty, !retryScheduled, running else { return }
        retryScheduled = true
        queue.asyncAfter(deadline: .now() + 3) { [weak self] in
            guard let self else { return }
            self.retryScheduled = false
            guard self.running, self.currentDevice == nil else { return }
            self.attemptedCandidateIDs.removeAll()
            self.attemptNextCandidateIfNeeded()
        }
    }

    // MARK: - Framing and handshake

    private func received(_ data: Data) {
        guard currentDevice != nil else { return }
        lastActivity = Date()
        for event in parser.feed(data) {
            switch event {
            case let .frame(frame): handle(frame)
            case let .error(error):
                diagnostics.append(.warning, "Corrupt FIBP packet ignored: \(String(describing: error)).")
                resetActiveRequestAfterParserFailure(error.wireErrorCode)
                sendRateLimitedError(error.wireErrorCode, scope: 0, requestID: 0, offendingType: 0)
            case let .rejectedErrorFrame(error):
                diagnostics.append(.warning, "Corrupt remote ERROR silently ignored: \(String(describing: error)).")
                resetActiveRequestAfterParserFailure(error.wireErrorCode)
            }
        }
    }

    private func handle(_ frame: FIBPFrame) {
        lastActivity = Date()
        let isErrorFrame = frame.rawMessageType == FIBPMessageType.error.rawValue
        guard frame.major == BridgeConfiguration.protocolMajor,
              frame.minor == BridgeConfiguration.protocolMinor else {
            if isErrorFrame {
                diagnostics.append(.warning, "Remote ERROR with an incompatible version silently ignored.")
                if hello == nil {
                    failCandidateBeforeHandshake("Serial candidate sent an incompatible ERROR and was closed.")
                }
                return
            }
            sendError(.unsupportedVersion, scope: frame.requestID == 0 ? 0 : 1,
                      requestID: frame.requestID, offendingType: frame.rawMessageType,
                      detail: "unsupported frame version")
            if hello == nil { failCandidateBeforeHandshake("Serial candidate has an incompatible FIBP version.") }
            return
        }
        guard let type = frame.messageType else {
            sendError(.unsupportedMessage, scope: frame.requestID == 0 ? 0 : 1,
                      requestID: frame.requestID, offendingType: frame.rawMessageType,
                      detail: "unsupported message")
            return
        }

        if type == .hello {
            handleHello(frame)
            return
        }
        guard hello != nil else {
            if isErrorFrame {
                diagnostics.append(.warning, "Remote ERROR before HELLO silently ignored.")
                return
            }
            sendError(.invalidState, scope: 0, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "HELLO required")
            return
        }

        switch type {
        case .ping: handlePing(frame)
        case .pong: handlePong(frame)
        case .requestStart: handleRequestStart(frame)
        case .requestHeader: handleRequestHeader(frame)
        case .requestBodyChunk: handleRequestBody(frame)
        case .requestEnd: handleRequestEnd(frame)
        case .cancel: handleRemoteCancel(frame)
        case .disconnect: handleRemoteDisconnect(frame)
        case .error: handleRemoteError(frame)
        default:
            sendError(.unsupportedMessage, scope: frame.requestID == 0 ? 0 : 1,
                      requestID: frame.requestID, offendingType: frame.rawMessageType,
                      detail: "invalid message direction")
        }
    }

    private func handleHello(_ frame: FIBPFrame) {
        guard frame.requestID == 0, frame.sequence == 0 else {
            failCandidateBeforeHandshake("Serial candidate sent an invalid HELLO.")
            return
        }
        let decoded: FIBPHello
        do { decoded = try FIBPHello(payload: frame.payload) }
        catch {
            failCandidateBeforeHandshake("Serial candidate has an invalid HELLO payload.")
            return
        }
        let supported = versionRangeContainsV1(decoded)
        guard supported, decoded.model == "Flipper Zero",
              decoded.capabilities.contains(.httpsGET),
              decoded.maximumReceivePayload >= UInt16(BridgeConfiguration.minimumAdvertisedReceivePayload),
              decoded.maximumResponseBytes > 0 else {
            if !supported {
                sendError(.unsupportedVersion, scope: 0, requestID: 0,
                          offendingType: frame.rawMessageType, detail: "no common protocol version")
            }
            failCandidateBeforeHandshake("HELLO failed device or protocol validation.")
            return
        }

        if let previous = hello, previous.clientNonce == decoded.clientNonce {
            guard previous == decoded else {
                failCandidateBeforeHandshake("Serial candidate changed HELLO with the same nonce and was closed.")
                return
            }
            sendHelloAcknowledgment(decoded)
            replayPermissionFrames()
            return
        }

        cancelRequest(sendCancel: false, reason: 2)
        hello = decoded
        helloDeadline = nil
        identity = FlipperIdentity(
            model: decoded.model,
            name: decoded.name,
            idType: decoded.idType,
            deviceID: decoded.deviceID,
            appVersion: decoded.appVersion,
            protocolMajor: BridgeConfiguration.protocolMajor,
            protocolMinor: BridgeConfiguration.protocolMinor
        )
        permission = nil
        lastAcceptedRequestID = nil
        expectedClientControlSequence = 1
        nextServerControlSequence = 1
        permissionReplayFrames.removeAll(keepingCapacity: true)
        negotiatedPayload = min(Int(decoded.maximumReceivePayload), BridgeConfiguration.maximumWirePayload)
        negotiatedResponseBytes = min(Int(decoded.maximumResponseBytes), BridgeConfiguration.maximumResponseBytes)
        negotiatedCapabilities = decoded.capabilities.intersection(.helperSupported)
        serverNonce = UInt64.random(in: 1...UInt64.max)
        sendHelloAcknowledgment(decoded)

        guard let identity else { return }
        diagnostics.append(.info, "FIBP HELLO validated: \(identity.displayName), \(identity.redactedID).")
        if permissions.contains(identity) {
            permission = .alwaysAllowed
            sendPermissionStatus(.alwaysAllowed, reason: 1)
            transition(.ready, "Internet access ready")
        } else {
            sendPermissionControl(
                .permissionRequired,
                payload: FIBPPayloadEncoder.permissionRequired(reason: 0)
            )
            transition(.permissionPending, "Waiting for user permission")
            let token = connectionToken
            let nonce = decoded.clientNonce
            permissionPrompt.requestPermission(for: identity) { [weak self] decision in
                self?.queue.async {
                    guard let self, self.connectionToken == token,
                          self.hello?.clientNonce == nonce,
                          self.permission == nil else { return }
                    self.applyPermissionDecision(decision, identity: identity)
                }
            }
        }
    }

    private func versionRangeContainsV1(_ hello: FIBPHello) -> Bool {
        let selected = (UInt16(BridgeConfiguration.protocolMajor) << 8)
            | UInt16(BridgeConfiguration.protocolMinor)
        let minimum = (UInt16(hello.minimumMajor) << 8) | UInt16(hello.minimumMinor)
        let maximum = (UInt16(hello.maximumMajor) << 8) | UInt16(hello.maximumMinor)
        return minimum <= selected && selected <= maximum
    }

    private func sendHelloAcknowledgment(_ decoded: FIBPHello) {
        let acknowledgment = FIBPHelloAcknowledgment(
            selectedMajor: BridgeConfiguration.protocolMajor,
            selectedMinor: BridgeConfiguration.protocolMinor,
            capabilities: negotiatedCapabilities,
            maximumPayload: UInt16(negotiatedPayload),
            maximumResponseBytes: UInt32(negotiatedResponseBytes),
            echoedClientNonce: decoded.clientNonce,
            serverNonce: serverNonce
        )
        send(FIBPFrame(messageType: .helloAck, requestID: 0, sequence: 0,
                       payload: acknowledgment.encoded))
    }

    private func replayPermissionFrames() {
        for frame in permissionReplayFrames { send(frame) }
    }

    private func applyPermissionDecision(_ decision: BridgePermissionDecision, identity: FlipperIdentity) {
        switch decision {
        case .deny:
            permission = .denied
            pendingPingToken = nil
            pingDeadline = nil
            sendPermissionStatus(.denied, reason: 0)
            transition(.permissionDenied, "Internet permission denied")
        case .allowOnce:
            permission = .allowedOnce
            sendPermissionStatus(.allowedOnce, reason: 0)
            transition(.ready, "Internet access ready")
        case .alwaysAllow:
            permissions.grant(identity)
            permission = .alwaysAllowed
            sendPermissionStatus(.alwaysAllowed, reason: 0)
            transition(.ready, "Internet access ready")
        }
    }

    private func sendPermissionStatus(_ value: BridgePermissionState, reason: UInt8) {
        sendPermissionControl(
            .permissionStatus,
            payload: FIBPPayloadEncoder.permissionStatus(state: value, reason: reason)
        )
    }

    // MARK: - Control stream

    private func handlePing(_ frame: FIBPFrame) {
        guard frame.requestID == 0, frame.payload.count == 8,
              acceptControlSequence(frame) else { return }
        sendControl(.pong, payload: frame.payload)
    }

    private func handlePong(_ frame: FIBPFrame) {
        guard frame.requestID == 0, frame.payload.count == 8,
              acceptControlSequence(frame) else { return }
        guard frame.payload == pendingPingToken else {
            sendError(.invalidRequest, scope: 0, requestID: 0,
                      offendingType: frame.rawMessageType, detail: "PONG token mismatch")
            return
        }
        pendingPingToken = nil
        pingDeadline = nil
    }

    private func handleRemoteError(_ frame: FIBPFrame) {
        // ERROR belongs to the directional control sequence even when it scopes
        // a non-zero request. Never answer malformed/duplicate ERROR with ERROR.
        guard acceptControlSequence(frame, reportFailure: false) else {
            diagnostics.append(.warning, "Remote ERROR with an invalid sequence was ignored.")
            return
        }
        guard let payload = try? FIBPRemoteErrorPayload(payload: frame.payload),
              (payload.scope == 0 && frame.requestID == 0)
                || (payload.scope == 1 && frame.requestID != 0) else {
            diagnostics.append(.warning, "Malformed remote ERROR silently ignored.")
            return
        }
        diagnostics.append(.warning, "Flipper reported a FIBP error message.")
        if payload.scope == 1, activeRequest?.requestID == frame.requestID {
            httpClient.cancel()
            activeRequest = nil
            transition(canPerformNetworkRequest ? .ready : .permissionDenied,
                       "Flipper ended the request with an error")
        }
    }

    private func acceptControlSequence(_ frame: FIBPFrame, reportFailure: Bool = true) -> Bool {
        if frame.sequence < expectedClientControlSequence {
            if reportFailure {
                sendError(.duplicateSequence, scope: 0, requestID: 0,
                          offendingType: frame.rawMessageType, detail: "duplicate control sequence")
            }
            return false
        }
        if frame.sequence > expectedClientControlSequence {
            if reportFailure {
                sendError(.sequenceGap, scope: 0, requestID: 0,
                          offendingType: frame.rawMessageType, detail: "control sequence gap")
            }
            return false
        }
        expectedClientControlSequence += 1
        return true
    }

    // MARK: - Request assembly

    private func handleRequestStart(_ frame: FIBPFrame) {
        guard frame.requestID != 0, frame.sequence == 0 else {
            sendError(.invalidRequest, scope: 1, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "invalid REQUEST_START")
            return
        }
        guard lastAcceptedRequestID.map({ frame.requestID > $0 }) ?? true else {
            sendError(.duplicateRequestID, scope: 1, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "request ID already used")
            return
        }
        guard canPerformNetworkRequest else {
            sendError(.permissionDenied, scope: 1, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "permission required")
            return
        }
        guard activeRequest == nil else {
            sendError(.invalidState, scope: 1, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "one request is already active")
            return
        }
        do {
            let start = try FIBPRequestStart(payload: frame.payload)
            guard start.method != .post || negotiatedCapabilities.contains(.httpsPOST),
                  start.declaredHeaderCount == 0 || negotiatedCapabilities.contains(.requestHeaders) else {
                sendError(.invalidRequest, scope: 1, requestID: frame.requestID,
                          offendingType: frame.rawMessageType,
                          detail: "request uses an unnegotiated capability")
                return
            }
            activeRequest = IncomingRequest(
                requestID: frame.requestID,
                start: start,
                assemblyDeadline: Date().addingTimeInterval(requestAssemblyTimeout)
            )
            lastAcceptedRequestID = frame.requestID
            transition(.receivingRequest, "Receiving request")
        } catch {
            sendError(.invalidRequest, scope: 1, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "malformed request metadata")
        }
    }

    private func handleRequestHeader(_ frame: FIBPFrame) {
        guard validateAndAdvanceRequestSequence(frame) else { return }
        guard var request = activeRequest else { return }
        do {
            let header = try BridgeHTTPHeader(payload: frame.payload)
            let aggregate = request.aggregateHeaderBytes + header.name.utf8.count + header.value.utf8.count
            guard !request.bodyStarted,
                  request.headers.count < Int(request.start.declaredHeaderCount),
                  request.headers.count < BridgeConfiguration.maximumHeaderCount,
                  aggregate <= BridgeConfiguration.maximumAggregateRequestHeaderBytes else {
                abortRequest(frame, code: .invalidRequest, detail: "request headers exceed limit")
                return
            }
            request.headers.append(header)
            request.aggregateHeaderBytes = aggregate
            activeRequest = request
        } catch {
            abortRequest(frame, code: .invalidRequest, detail: "malformed request header")
        }
    }

    private func handleRequestBody(_ frame: FIBPFrame) {
        guard validateAndAdvanceRequestSequence(frame) else { return }
        guard var request = activeRequest else { return }
        guard request.start.method == .post,
              !frame.payload.isEmpty,
              request.headers.count == Int(request.start.declaredHeaderCount),
              request.body.count + frame.payload.count <= BridgeConfiguration.maximumRequestBodyBytes,
              request.body.count + frame.payload.count <= Int(request.start.declaredBodyLength) else {
            abortRequest(frame, code: .invalidRequest, detail: "request body exceeds declaration")
            return
        }
        request.bodyStarted = true
        request.body.append(frame.payload)
        activeRequest = request
    }

    private func handleRequestEnd(_ frame: FIBPFrame) {
        guard validateAndAdvanceRequestSequence(frame) else { return }
        guard var request = activeRequest else { return }
        guard frame.payload.isEmpty, frame.flags.contains(.final),
              request.headers.count == Int(request.start.declaredHeaderCount),
              request.body.count == Int(request.start.declaredBodyLength) else {
            abortRequest(frame, code: .invalidRequest, detail: "request declaration mismatch")
            return
        }
        request.requestEnded = true
        activeRequest = request

        // Permission is intentionally checked again at the last possible point,
        // after assembly and immediately before creating a URLSession task.
        guard canPerformNetworkRequest else {
            abortRequest(frame, code: .permissionDenied, detail: "permission revoked")
            return
        }
        transition(.performingRequest, "Performing HTTPS request")
        let networkRequest = BridgeHTTPRequest(
            requestID: request.requestID,
            method: request.start.method,
            urlString: request.start.url,
            headers: request.headers,
            body: request.body,
            timeout: TimeInterval(request.start.timeoutMilliseconds) / 1_000
        )
        let token = connectionToken
        httpClient.execute(
            networkRequest,
            onResponse: { [weak self] response in
                self?.queue.async {
                    guard self?.connectionToken == token else { return }
                    self?.networkResponseStarted(response, requestID: request.requestID)
                }
            },
            onData: { [weak self] data in
                self?.queue.async {
                    guard self?.connectionToken == token else { return }
                    self?.networkData(data, requestID: request.requestID)
                }
            },
            completion: { [weak self] result in
                self?.queue.async {
                    guard self?.connectionToken == token else { return }
                    self?.networkCompleted(result, requestID: request.requestID)
                }
            }
        )
    }

    private func validateAndAdvanceRequestSequence(_ frame: FIBPFrame) -> Bool {
        guard var request = activeRequest, request.requestID == frame.requestID else {
            sendError(.invalidState, scope: 1, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "no matching active request")
            return false
        }
        guard !request.requestEnded else {
            sendError(.invalidState, scope: 1, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "request stream already ended")
            return false
        }
        if frame.sequence < request.expectedSequence {
            sendError(.duplicateSequence, scope: 1, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "duplicate request sequence")
            return false
        }
        if frame.sequence > request.expectedSequence {
            abortRequest(frame, code: .sequenceGap, detail: "request sequence gap")
            return false
        }
        request.expectedSequence += 1
        request.assemblyDeadline = Date().addingTimeInterval(requestAssemblyTimeout)
        activeRequest = request
        return true
    }

    private func abortRequest(_ frame: FIBPFrame, code: FIBPErrorCode, detail: String) {
        httpClient.cancel()
        activeRequest = nil
        sendError(code, scope: 1, requestID: frame.requestID,
                  offendingType: frame.rawMessageType, detail: detail)
        transition(canPerformNetworkRequest ? .ready : .permissionDenied, detail)
    }

    private func handleRemoteCancel(_ frame: FIBPFrame) {
        guard frame.requestID != 0,
              let request = activeRequest, request.requestID == frame.requestID else { return }
        guard frame.payload.count == 1, let reason = frame.payload.first, reason <= 3 else {
            sendError(.invalidRequest, scope: 1, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "invalid CANCEL payload")
            return
        }
        guard frame.sequence == request.expectedSequence else {
            let code: FIBPErrorCode = frame.sequence < request.expectedSequence ? .duplicateSequence : .sequenceGap
            sendError(code, scope: 1, requestID: frame.requestID,
                      offendingType: frame.rawMessageType, detail: "invalid CANCEL sequence")
            return
        }
        httpClient.cancel()
        activeRequest = nil
        transition(canPerformNetworkRequest ? .ready : .permissionDenied, "Request cancelled by Flipper")
        diagnostics.append(.info, "Flipper cancelled the active request.")
    }

    // MARK: - Response stream

    private func networkResponseStarted(_ metadata: BridgeHTTPResponseMetadata, requestID: UInt32) {
        guard var request = activeRequest, request.requestID == requestID,
              !request.responseStarted, canPerformNetworkRequest else { return }
        let headers: ArraySlice<BridgeHTTPHeader>
        if negotiatedCapabilities.contains(.responseHeaders) {
            headers = metadata.headers.filter { $0.encoded.count <= negotiatedPayload }
                .prefix(BridgeConfiguration.maximumHeaderCount)
        } else {
            headers = []
        }
        let declaredLength: UInt32
        if metadata.expectedBodyLength >= 0,
           metadata.expectedBodyLength <= Int64(negotiatedResponseBytes) {
            declaredLength = UInt32(metadata.expectedBodyLength)
        } else {
            declaredLength = UInt32.max
        }
        request.responseStarted = true
        request.responseSequence = 0
        activeRequest = request
        sendResponse(.responseStart, request: &request,
                     payload: FIBPPayloadEncoder.responseStart(
                        status: UInt16(clamping: metadata.statusCode),
                        headerCount: UInt8(headers.count),
                        declaredBodyLength: declaredLength
                     ))
        for header in headers {
            sendResponse(.responseHeader, request: &request, payload: header.encoded)
        }
        activeRequest = request
        transition(.sendingResponse, "Sending response to Flipper")
    }

    private func networkData(_ data: Data, requestID: UInt32) {
        guard var request = activeRequest, request.requestID == requestID,
              request.responseStarted, !request.responseEnded, canPerformNetworkRequest else { return }
        let remaining = negotiatedResponseBytes - request.responseBytes
        if remaining > 0 {
            let accepted = data.prefix(remaining)
            let chunkSize = max(1, min(BridgeConfiguration.responseChunkSize, negotiatedPayload))
            var offset = 0
            while offset < accepted.count {
                let end = min(offset + chunkSize, accepted.count)
                sendResponse(.responseBodyChunk, request: &request,
                             payload: Data(accepted[offset..<end]))
                request.responseBytes += end - offset
                offset = end
            }
        }
        if data.count > remaining {
            request.truncated = true
            activeRequest = request
            httpClient.cancel()
        } else {
            activeRequest = request
        }
    }

    private func networkCompleted(
        _ result: Result<Void, BridgeNetworkError>,
        requestID: UInt32
    ) {
        guard var request = activeRequest, request.requestID == requestID,
              !request.responseEnded else { return }
        if request.responseStarted {
            switch result {
            case .success:
                finishResponse(&request, result: 0, flags: [])
            case let .failure(error):
                if request.truncated || error == .responseTooLarge {
                    finishResponse(&request, result: 1, flags: [.truncated])
                } else if request.locallyCancelled || error == .cancelled {
                    finishResponse(&request, result: 2, flags: [])
                } else {
                    sendNetworkError(error, requestID: requestID)
                    activeRequest = nil
                }
            }
        } else {
            switch result {
            case .success:
                sendError(.networkFailure, scope: 1, requestID: requestID,
                          offendingType: FIBPMessageType.requestEnd.rawValue,
                          detail: "missing HTTP response")
            case let .failure(error): sendNetworkError(error, requestID: requestID)
            }
            activeRequest = nil
        }
        if activeRequest == nil {
            transition(canPerformNetworkRequest ? .ready : .permissionDenied,
                       canPerformNetworkRequest ? "Internet access ready" : "No internet permission")
        }
    }

    private func finishResponse(_ request: inout IncomingRequest, result: UInt8, flags: FIBPFlags) {
        sendResponse(.responseEnd, request: &request, flags: flags.union(.final),
                     payload: FIBPPayloadEncoder.responseEnd(
                        result: result,
                        bytesSent: UInt32(request.responseBytes)
                     ))
        request.responseEnded = true
        activeRequest = nil
    }

    private func sendNetworkError(_ error: BridgeNetworkError, requestID: UInt32) {
        let code: FIBPErrorCode
        switch error {
        case .securityBlocked: code = .securityBlocked
        case .timeout: code = .timeout
        case .cancelled: code = .cancelled
        case .responseTooLarge: code = .responseTooLarge
        case .invalidResponse, .transportFailure: code = .networkFailure
        }
        sendError(code, scope: 1, requestID: requestID,
                  offendingType: FIBPMessageType.requestEnd.rawValue,
                  detail: error.localizedDescription)
        diagnostics.append(.warning, "HTTPS request failed: \(error.localizedDescription)")
    }

    private func sendResponse(
        _ type: FIBPMessageType,
        request: inout IncomingRequest,
        flags: FIBPFlags = [],
        payload: Data
    ) {
        send(FIBPFrame(messageType: type, flags: flags, requestID: request.requestID,
                       sequence: request.responseSequence, payload: payload))
        request.responseSequence += 1
    }

    // MARK: - Cancellation, timeout, and output

    private var canPerformNetworkRequest: Bool {
        currentDevice != nil && (permission == .allowedOnce || permission == .alwaysAllowed)
    }

    private func cancelRequest(sendCancel: Bool, reason: UInt8) {
        guard var request = activeRequest else { return }
        request.locallyCancelled = true
        activeRequest = request
        httpClient.cancel()
        if sendCancel {
            send(FIBPFrame(messageType: .cancel, flags: [.final], requestID: request.requestID,
                           sequence: request.responseSequence, payload: Data([reason])))
        }
        activeRequest = nil
        transition(canPerformNetworkRequest ? .ready : .permissionDenied,
                   reason == 0 ? "Request cancelled" : "Internet access disabled")
    }

    private func handleRemoteDisconnect(_ frame: FIBPFrame) {
        guard frame.requestID == 0, frame.payload.count == 1,
              let reason = frame.payload.first, reason <= 2 else {
            sendError(.invalidRequest, scope: 0, requestID: 0,
                      offendingType: frame.rawMessageType, detail: "invalid DISCONNECT payload")
            return
        }
        guard acceptControlSequence(frame) else { return }
        diagnostics.append(.info, "Flipper closed the FIBP session.")
        cancelCurrentConnection(closeTransport: true)
        transition(.disconnected, "Flipper closed the connection")
        attemptNextCandidateIfNeeded()
    }

    private func bestEffortDisconnect(reason: UInt8) {
        guard hello != nil, transport.isOpen else { return }
        sendControl(.disconnect, payload: Data([reason]))
    }

    private func cancelCurrentConnection(closeTransport: Bool) {
        connectionToken = UUID()
        permissionPrompt.cancelPendingPrompt()
        httpClient.cancel()
        activeRequest = nil
        if closeTransport { transport.close() }
        transport.onReceive = nil
        transport.onDisconnect = nil
        currentDevice = nil
        parser.reset()
        resetSessionState()
    }

    private func resetSessionState() {
        helloDeadline = nil
        hello = nil
        identity = nil
        permission = nil
        serverNonce = 0
        negotiatedPayload = BridgeConfiguration.maximumWirePayload
        negotiatedResponseBytes = BridgeConfiguration.maximumResponseBytes
        negotiatedCapabilities = []
        nextServerControlSequence = 1
        expectedClientControlSequence = 1
        permissionReplayFrames.removeAll(keepingCapacity: true)
        lastAcceptedRequestID = nil
        activeRequest = nil
        pendingPingToken = nil
        pingDeadline = nil
    }

    private func sendControl(_ type: FIBPMessageType, payload: Data, flags: FIBPFlags = []) {
        send(FIBPFrame(messageType: type, flags: flags, requestID: 0,
                       sequence: nextServerControlSequence, payload: payload))
        nextServerControlSequence += 1
    }

    private func sendPermissionControl(_ type: FIBPMessageType, payload: Data) {
        let frame = FIBPFrame(
            messageType: type,
            requestID: 0,
            sequence: nextServerControlSequence,
            payload: payload
        )
        send(frame)
        nextServerControlSequence += 1
        permissionReplayFrames.append(frame)
        if permissionReplayFrames.count > 2 { permissionReplayFrames.removeFirst() }
    }

    private func sendError(
        _ code: FIBPErrorCode,
        scope: UInt8,
        requestID: UInt32,
        offendingType: UInt8,
        detail: String
    ) {
        let payload = FIBPPayloadEncoder.error(
            code: code,
            scope: scope,
            offendingType: offendingType,
            detail: detail,
            maximumPayload: hello == nil
                ? BridgeConfiguration.maximumWirePayload
                : negotiatedPayload
        )
        sendControlFrame(.error, requestID: requestID, payload: payload)
    }

    private func sendRateLimitedError(
        _ code: FIBPErrorCode,
        scope: UInt8,
        requestID: UInt32,
        offendingType: UInt8
    ) {
        let now = Date()
        guard now.timeIntervalSince(lastErrorFrameDate) >= 0.25 else { return }
        lastErrorFrameDate = now
        sendError(code, scope: scope, requestID: requestID,
                  offendingType: offendingType, detail: "malformed frame")
    }

    private func sendControlFrame(_ type: FIBPMessageType, requestID: UInt32, payload: Data) {
        send(FIBPFrame(messageType: type, requestID: requestID,
                       sequence: nextServerControlSequence, payload: payload))
        nextServerControlSequence += 1
    }

    private func send(_ frame: FIBPFrame) {
        guard transport.isOpen else { return }
        do {
            try transport.send(FIBPCodec.encode(frame))
            lastActivity = Date()
        } catch {
            diagnostics.append(.error, "FIBP packet could not be written to the serial port.")
        }
    }

    private func startMaintenanceTimer() {
        let timer = DispatchSource.makeTimerSource(queue: queue)
        timer.schedule(deadline: .now() + 0.25, repeating: 0.25)
        timer.setEventHandler { [weak self] in self?.maintenanceTick() }
        maintenanceTimer = timer
        timer.resume()
    }

    private func maintenanceTick() {
        let now = Date()
        if let event = parser.resetIfStalled(now: now) {
            switch event {
            case let .error(error):
                diagnostics.append(.warning, "Incomplete FIBP packet timed out.")
                resetActiveRequestAfterParserFailure(error.wireErrorCode)
                sendRateLimitedError(error.wireErrorCode, scope: 0, requestID: 0, offendingType: 0)
            case let .rejectedErrorFrame(error):
                diagnostics.append(.warning, "Incomplete remote ERROR silently ignored.")
                resetActiveRequestAfterParserFailure(error.wireErrorCode)
            case .frame:
                break
            }
        }
        if hello == nil, let deadline = helloDeadline, now >= deadline {
            failCandidateBeforeHandshake("Serial candidate did not send HELLO within five seconds.")
            return
        }
        if let request = activeRequest, !request.requestEnded,
           now >= request.assemblyDeadline {
            activeRequest = nil
            sendError(.timeout, scope: 1, requestID: request.requestID,
                      offendingType: FIBPMessageType.requestEnd.rawValue,
                      detail: "request assembly timeout")
            transition(canPerformNetworkRequest ? .ready : .permissionDenied,
                       "Incomplete request timed out")
        }
        if let deadline = pingDeadline, now >= deadline {
            diagnostics.append(.warning, "Flipper PONG timed out; closing the connection.")
            cancelRequest(sendCancel: false, reason: 1)
            cancelCurrentConnection(closeTransport: true)
            transition(.disconnected, "Connection timed out")
            attemptNextCandidateIfNeeded()
            return
        }
        if hello != nil, canPerformNetworkRequest, pendingPingToken == nil,
           now.timeIntervalSince(lastActivity) >= idleSessionTimeout {
            var token = [UInt8](repeating: 0, count: 8)
            var random = UInt64.random(in: 1...UInt64.max)
            withUnsafeBytes(of: &random) { token.replaceSubrange(0..<8, with: $0) }
            let payload = Data(token)
            pendingPingToken = payload
            pingDeadline = now.addingTimeInterval(pongTimeout)
            sendControl(.ping, payload: payload)
            lastActivity = now
        }
    }

    private func transition(_ newState: BridgeConnectionState, _ text: String) {
        state = newState
        statusText = text
        let snapshot = makeSnapshot()
        onStatusChange?(snapshot)
    }

    private func resetActiveRequestAfterParserFailure(_ code: FIBPErrorCode) {
        guard activeRequest != nil else { return }
        httpClient.cancel()
        activeRequest = nil
        transition(canPerformNetworkRequest ? .ready : .permissionDenied,
                   code == .receiveOverflow ? "USB receive buffer reset" : "Incomplete request frame cancelled")
    }

    private func makeSnapshot() -> BridgeStatusSnapshot {
        BridgeStatusSnapshot(
            state: state,
            statusText: statusText,
            device: currentDevice,
            identity: identity,
            permission: permission,
            activeRequestID: activeRequest?.requestID
        )
    }

    private func performSync<T>(_ action: () -> T) -> T {
        if DispatchQueue.getSpecific(key: queueKey) != nil { return action() }
        return queue.sync(execute: action)
    }
}
