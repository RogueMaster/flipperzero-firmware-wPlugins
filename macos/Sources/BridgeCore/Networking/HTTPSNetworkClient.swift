import Foundation

public struct BridgeHTTPRequest: Equatable, Sendable {
    public let requestID: UInt32
    public let method: FIBPHTTPMethod
    public let urlString: String
    public let headers: [BridgeHTTPHeader]
    public let body: Data
    public let timeout: TimeInterval

    public init(
        requestID: UInt32,
        method: FIBPHTTPMethod,
        urlString: String,
        headers: [BridgeHTTPHeader],
        body: Data,
        timeout: TimeInterval
    ) {
        self.requestID = requestID
        self.method = method
        self.urlString = urlString
        self.headers = headers
        self.body = body
        self.timeout = timeout
    }
}

public struct BridgeHTTPResponseMetadata: Equatable, Sendable {
    public let statusCode: Int
    public let headers: [BridgeHTTPHeader]
    public let expectedBodyLength: Int64

    public init(statusCode: Int, headers: [BridgeHTTPHeader], expectedBodyLength: Int64) {
        self.statusCode = statusCode
        self.headers = headers
        self.expectedBodyLength = expectedBodyLength
    }
}

public enum BridgeNetworkError: LocalizedError, Equatable {
    case securityBlocked(NetworkPolicyError)
    case timeout
    case cancelled
    case responseTooLarge
    case invalidResponse
    case transportFailure

    public var errorDescription: String? {
        switch self {
        case let .securityBlocked(error): return error.localizedDescription
        case .timeout: return "The internet request timed out."
        case .cancelled: return "The request was cancelled."
        case .responseTooLarge: return "The server response was truncated at the size limit."
        case .invalidResponse: return "The server did not return a valid HTTP response."
        case .transportFailure: return "The internet request could not be completed."
        }
    }
}

public protocol BridgeHTTPClient: AnyObject {
    func execute(
        _ request: BridgeHTTPRequest,
        onResponse: @escaping (BridgeHTTPResponseMetadata) -> Void,
        onData: @escaping (Data) -> Void,
        completion: @escaping (Result<Void, BridgeNetworkError>) -> Void
    )
    func cancel()
    func invalidate()
}

struct RedirectBudget: Equatable {
    private(set) var count = 0
    let limit: Int

    mutating func consume() -> Bool {
        guard count < limit else { return false }
        count += 1
        return true
    }
}

enum URLSessionSecurityPolicy {
    static func permitsDefaultHandling(authenticationMethod: String) -> Bool {
        authenticationMethod == NSURLAuthenticationMethodServerTrust
    }

    static func sanitizeRedirect(_ request: URLRequest) -> URLRequest {
        var cleanRequest = request
        cleanRequest.httpShouldHandleCookies = false
        cleanRequest.setValue(nil, forHTTPHeaderField: "Authorization")
        cleanRequest.setValue(nil, forHTTPHeaderField: "Cookie")
        cleanRequest.setValue(nil, forHTTPHeaderField: "Proxy-Authorization")
        return cleanRequest
    }
}

public final class HTTPSNetworkClient: NSObject, BridgeHTTPClient {
    private final class Context {
        let token = UUID()
        let request: BridgeHTTPRequest
        let onResponse: (BridgeHTTPResponseMetadata) -> Void
        let onData: (Data) -> Void
        let completion: (Result<Void, BridgeNetworkError>) -> Void
        let deadline: DispatchTime
        var task: URLSessionDataTask?
        var redirectBudget = RedirectBudget(limit: BridgeConfiguration.maximumRedirects)
        var bytesReceived = 0
        var terminalError: BridgeNetworkError?
        var finished = false
        var deadlineWorkItem: DispatchWorkItem?
        let transformsNationalToday: Bool
        let transformsRadioBrowser: Bool
        let expectsRadioAudio: Bool
        var bufferedHTML = Data()
        var delayedResponse: BridgeHTTPResponseMetadata?
        var streamsRadioAudio = false
        var radioBuffer = Data()
        var radioReadOffset = 0
        var radioTimer: DispatchSourceTimer?

        init(
            request: BridgeHTTPRequest,
            onResponse: @escaping (BridgeHTTPResponseMetadata) -> Void,
            onData: @escaping (Data) -> Void,
            completion: @escaping (Result<Void, BridgeNetworkError>) -> Void
        ) {
            self.request = request
            self.onResponse = onResponse
            self.onData = onData
            self.completion = completion
            transformsNationalToday = NationalTodayExtractor.handles(request)
            transformsRadioBrowser = RadioBrowserExtractor.handles(request)
            expectsRadioAudio = request.headers.contains { header in
                header.name.caseInsensitiveCompare("accept") == .orderedSame
                    && header.value.lowercased().contains("audio/mpeg")
            }
            deadline = .now() + max(0.001, min(request.timeout, 30))
        }
    }

    private let policy: NetworkPolicy
    private let resolverQueue = DispatchQueue(label: "fibp.network-policy", qos: .userInitiated)
    private let delegateQueue: OperationQueue
    private var active: Context?
    private var contextsByTaskID = [Int: Context]()
    private var invalidated = false

    private lazy var session: URLSession = {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.httpCookieStorage = nil
        configuration.httpShouldSetCookies = false
        configuration.urlCredentialStorage = nil
        configuration.urlCache = nil
        configuration.requestCachePolicy = .reloadIgnoringLocalCacheData
        configuration.connectionProxyDictionary = [:]
        configuration.timeoutIntervalForRequest = 30
        // Ordinary requests still have our explicit 30 second deadline. Live
        // radio is identified from audio/mpeg response headers and may remain
        // open much longer without weakening the normal request policy.
        configuration.timeoutIntervalForResource = 3_600
        configuration.waitsForConnectivity = false
        return URLSession(configuration: configuration, delegate: self, delegateQueue: delegateQueue)
    }()

    public init(policy: NetworkPolicy) {
        self.policy = policy
        let queue = OperationQueue()
        queue.name = "fibp.url-session-delegate"
        queue.maxConcurrentOperationCount = 1
        delegateQueue = queue
        super.init()
    }

    public func execute(
        _ request: BridgeHTTPRequest,
        onResponse: @escaping (BridgeHTTPResponseMetadata) -> Void,
        onData: @escaping (Data) -> Void,
        completion: @escaping (Result<Void, BridgeNetworkError>) -> Void
    ) {
        let context = Context(
            request: request,
            onResponse: onResponse,
            onData: onData,
            completion: completion
        )
        delegateQueue.addOperation { [weak self] in
            guard let self, !self.invalidated else {
                context.deadlineWorkItem?.cancel()
                completion(.failure(.cancelled))
                return
            }
            if let previous = self.active { self.cancel(previous) }
            self.active = context
            self.scheduleDeadline(for: context)
            self.resolveAndStart(context)
        }
    }

    public func cancel() {
        delegateQueue.addOperation { [weak self] in
            guard let self, let active = self.active else { return }
            self.cancel(active)
        }
    }

    public func invalidate() {
        delegateQueue.addOperation { [weak self] in
            guard let self, !self.invalidated else { return }
            self.invalidated = true
            if let active = self.active { self.cancel(active) }
            self.session.invalidateAndCancel()
        }
    }

    private func resolveAndStart(_ context: Context) {
        resolverQueue.async { [weak self, weak context] in
            guard let self, let context else { return }
            let result: Result<URL, BridgeNetworkError>
            do {
                result = .success(try self.policy.validate(urlString: context.request.urlString))
            } catch let error as NetworkPolicyError {
                result = .failure(.securityBlocked(error))
            } catch {
                result = .failure(.transportFailure)
            }
            self.delegateQueue.addOperation { [weak self, weak context] in
                guard let self, let context,
                      self.active?.token == context.token,
                      !context.finished else { return }
                switch result {
                case let .success(url): self.startTask(context, url: url)
                case let .failure(error): self.finish(context, result: .failure(error))
                }
            }
        }
    }

    private func scheduleDeadline(for context: Context) {
        let deadline = DispatchWorkItem { [weak self, weak context] in
            guard let self, let context else { return }
            self.delegateQueue.addOperation { [weak self, weak context] in
                guard let self, let context,
                      self.active?.token == context.token,
                      !context.finished else { return }
                context.terminalError = .timeout
                context.task?.cancel()
                self.finish(context, result: .failure(.timeout))
            }
        }
        context.deadlineWorkItem = deadline
        DispatchQueue.global(qos: .utility).asyncAfter(
            deadline: context.deadline,
            execute: deadline
        )
    }

    private func startTask(_ context: Context, url: URL) {
        var request = URLRequest(url: url)
        request.httpMethod = context.request.method == .get ? "GET" : "POST"
        request.timeoutInterval = min(context.request.timeout, 30)
        request.httpShouldHandleCookies = false
        request.cachePolicy = .reloadIgnoringLocalCacheData
        for header in policy.allowedRequestHeaders(context.request.headers) {
            request.setValue(header.value, forHTTPHeaderField: header.name)
        }
        request.setValue(BridgeConfiguration.fixedUserAgent, forHTTPHeaderField: "User-Agent")
        if context.request.method == .post { request.httpBody = context.request.body }
        let task = session.dataTask(with: request)
        context.task = task
        contextsByTaskID[task.taskIdentifier] = context
        task.resume()
    }

    private func cancel(_ context: Context) {
        guard !context.finished else { return }
        context.terminalError = .cancelled
        if let task = context.task {
            task.cancel()
        } else {
            finish(context, result: .failure(.cancelled))
        }
    }

    private func fail(_ context: Context, with error: BridgeNetworkError) {
        guard !context.finished else { return }
        context.terminalError = error
        if let task = context.task {
            task.cancel()
        } else {
            finish(context, result: .failure(error))
        }
    }

    private func finish(_ context: Context, result: Result<Void, BridgeNetworkError>) {
        guard !context.finished else { return }
        context.finished = true
        context.radioTimer?.cancel()
        context.radioTimer = nil
        context.deadlineWorkItem?.cancel()
        context.deadlineWorkItem = nil
        if let task = context.task { contextsByTaskID.removeValue(forKey: task.taskIdentifier) }
        if active?.token == context.token { active = nil }
        context.completion(result)
    }

    private func startRadioDrainIfReady(_ context: Context) {
        let targetBytes = 80 * 1_024 // About ten seconds at 64 kbit/s MP3.
        guard context.streamsRadioAudio, context.radioTimer == nil,
              context.radioBuffer.count - context.radioReadOffset >= targetBytes else { return }
        let timer = DispatchSource.makeTimerSource(queue: .global(qos: .userInitiated))
        // Smaller, more frequent deliveries keep CDC traffic even instead of
        // producing a six-frame burst every 128 ms. The average remains the
        // exact 8 kB/s required by a 64 kbit/s MP3 stream.
        timer.schedule(deadline: .now(), repeating: .milliseconds(64), leeway: .milliseconds(4))
        timer.setEventHandler { [weak self, weak context] in
            guard let self, let context else { return }
            self.delegateQueue.addOperation { [weak self, weak context] in
                guard let self, let context, !context.finished,
                      self.active?.token == context.token else { return }
                let available = context.radioBuffer.count - context.radioReadOffset
                guard available >= 512 else { return }
                let start = context.radioBuffer.index(
                    context.radioBuffer.startIndex,
                    offsetBy: context.radioReadOffset
                )
                let end = context.radioBuffer.index(start, offsetBy: 512)
                context.onData(context.radioBuffer.subdata(in: start..<end))
                context.radioReadOffset += 512
                if context.radioReadOffset >= 32 * 1_024 {
                    context.radioBuffer.removeFirst(context.radioReadOffset)
                    context.radioReadOffset = 0
                }
            }
        }
        context.radioTimer = timer
        timer.resume()
    }
}

extension HTTPSNetworkClient: URLSessionDataDelegate {
    public func urlSession(
        _ session: URLSession,
        dataTask: URLSessionDataTask,
        didReceive response: URLResponse,
        completionHandler: @escaping (URLSession.ResponseDisposition) -> Void
    ) {
        guard let context = contextsByTaskID[dataTask.taskIdentifier],
              let response = response as? HTTPURLResponse,
              (100...599).contains(response.statusCode) else {
            completionHandler(.cancel)
            if let context = contextsByTaskID[dataTask.taskIdentifier] {
                fail(context, with: .invalidResponse)
            }
            return
        }
        let expected = response.expectedContentLength <= Int64(BridgeConfiguration.maximumResponseBytes)
            ? response.expectedContentLength
            : -1
        let metadata = BridgeHTTPResponseMetadata(
            statusCode: response.statusCode,
            headers: policy.allowedResponseHeaders(response.allHeaderFields),
            expectedBodyLength: expected
        )
        let contentType = response.value(forHTTPHeaderField: "Content-Type")?.lowercased() ?? ""
        if context.expectsRadioAudio
            || contentType.hasPrefix("audio/mpeg")
            || contentType.hasPrefix("audio/mp3")
            || contentType.hasPrefix("audio/x-mpeg") {
            context.streamsRadioAudio = true
            context.deadlineWorkItem?.cancel()
            context.deadlineWorkItem = nil
        }
        if context.transformsNationalToday || context.transformsRadioBrowser {
            context.delayedResponse = metadata
        } else {
            context.onResponse(metadata)
        }
        completionHandler(.allow)
    }

    public func urlSession(
        _ session: URLSession,
        dataTask: URLSessionDataTask,
        didReceive data: Data
    ) {
        guard let context = contextsByTaskID[dataTask.taskIdentifier], !context.finished else { return }
        if context.streamsRadioAudio {
            let remaining = BridgeConfiguration.maximumResponseBytes - context.bytesReceived
            guard remaining > 0 else {
                fail(context, with: .responseTooLarge)
                return
            }
            let accepted = data.prefix(remaining)
            context.bytesReceived += accepted.count
            context.radioBuffer.append(accepted)
            startRadioDrainIfReady(context)
            if data.count > remaining { fail(context, with: .responseTooLarge) }
            return
        }
        if context.transformsNationalToday || context.transformsRadioBrowser {
            let maximumBytes = context.transformsNationalToday
                ? NationalTodayExtractor.maximumHTMLBytes
                : RadioBrowserExtractor.maximumJSONBytes
            guard context.bufferedHTML.count + data.count <= maximumBytes else {
                fail(context, with: .responseTooLarge)
                return
            }
            context.bufferedHTML.append(data)
            return
        }
        let remaining = BridgeConfiguration.maximumResponseBytes - context.bytesReceived
        if remaining > 0 {
            let accepted = data.prefix(remaining)
            if !accepted.isEmpty {
                context.bytesReceived += accepted.count
                context.onData(Data(accepted))
            }
        }
        if data.count > remaining {
            fail(context, with: .responseTooLarge)
        }
    }

    public func urlSession(
        _ session: URLSession,
        dataTask: URLSessionDataTask,
        willCacheResponse proposedResponse: CachedURLResponse,
        completionHandler: @escaping (CachedURLResponse?) -> Void
    ) {
        completionHandler(nil)
    }
}

extension HTTPSNetworkClient {
    public func urlSession(
        _ session: URLSession,
        task: URLSessionTask,
        willPerformHTTPRedirection response: HTTPURLResponse,
        newRequest request: URLRequest,
        completionHandler: @escaping (URLRequest?) -> Void
    ) {
        guard let context = contextsByTaskID[task.taskIdentifier],
              let target = request.url else {
            completionHandler(nil)
            return
        }
        guard context.redirectBudget.consume() else {
            completionHandler(nil)
            fail(context, with: .securityBlocked(.tooManyRedirects))
            return
        }

        resolverQueue.async { [weak self, weak context] in
            guard let self, let context else {
                completionHandler(nil)
                return
            }
            let allowed: Bool
            do {
                _ = try self.policy.validate(urlString: target.absoluteString)
                allowed = true
            } catch {
                allowed = false
            }
            self.delegateQueue.addOperation { [weak self, weak context] in
                guard let self, let context,
                      self.active?.token == context.token,
                      context.terminalError == nil,
                      !context.finished else {
                    completionHandler(nil)
                    return
                }
                if allowed {
                    completionHandler(URLSessionSecurityPolicy.sanitizeRedirect(request))
                } else {
                    completionHandler(nil)
                    self.fail(context, with: .securityBlocked(.nonGlobalAddress))
                }
            }
        }
    }

    public func urlSession(
        _ session: URLSession,
        task: URLSessionTask,
        didReceive challenge: URLAuthenticationChallenge,
        completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void
    ) {
        if URLSessionSecurityPolicy.permitsDefaultHandling(
            authenticationMethod: challenge.protectionSpace.authenticationMethod
        ) {
            completionHandler(.performDefaultHandling, nil)
        } else {
            completionHandler(.cancelAuthenticationChallenge, nil)
            if let context = contextsByTaskID[task.taskIdentifier] {
                fail(context, with: .securityBlocked(.embeddedCredentials))
            }
        }
    }

    public func urlSession(
        _ session: URLSession,
        task: URLSessionTask,
        didCompleteWithError error: Error?
    ) {
        guard let context = contextsByTaskID[task.taskIdentifier], !context.finished else { return }
        if let terminal = context.terminalError {
            finish(context, result: .failure(terminal))
        } else if let error = error as? URLError, error.code == .timedOut {
            finish(context, result: .failure(.timeout))
        } else if error != nil {
            finish(context, result: .failure(.transportFailure))
        } else if context.transformsNationalToday {
            completeNationalToday(context)
        } else if context.transformsRadioBrowser {
            completeRadioBrowser(context)
        } else {
            finish(context, result: .success(()))
        }
    }

    private func completeNationalToday(_ context: Context) {
        guard let response = context.delayedResponse else {
            finish(context, result: .failure(.invalidResponse))
            return
        }
        guard response.statusCode == 200 else {
            context.onResponse(BridgeHTTPResponseMetadata(
                statusCode: response.statusCode,
                headers: [],
                expectedBodyLength: 0
            ))
            finish(context, result: .success(()))
            return
        }
        guard let text = NationalTodayExtractor.extract(from: context.bufferedHTML) else {
            finish(context, result: .failure(.invalidResponse))
            return
        }
        context.onResponse(BridgeHTTPResponseMetadata(
            statusCode: 200,
            headers: [BridgeHTTPHeader(name: "content-type", value: "text/plain; charset=utf-8")],
            expectedBodyLength: Int64(text.count)
        ))
        context.onData(text)
        finish(context, result: .success(()))
    }

    private func completeRadioBrowser(_ context: Context) {
        guard let response = context.delayedResponse else {
            finish(context, result: .failure(.invalidResponse))
            return
        }
        guard response.statusCode == 200 else {
            context.onResponse(BridgeHTTPResponseMetadata(
                statusCode: response.statusCode,
                headers: [],
                expectedBodyLength: 0
            ))
            finish(context, result: .success(()))
            return
        }
        guard let compact = RadioBrowserExtractor.extract(from: context.bufferedHTML) else {
            finish(context, result: .failure(.invalidResponse))
            return
        }
        context.onResponse(BridgeHTTPResponseMetadata(
            statusCode: 200,
            headers: [BridgeHTTPHeader(name: "content-type", value: "text/plain; charset=utf-8")],
            expectedBodyLength: Int64(compact.count)
        ))
        context.onData(compact)
        finish(context, result: .success(()))
    }
}

extension HTTPSNetworkClient {
    public func urlSession(
        _ session: URLSession,
        didReceive challenge: URLAuthenticationChallenge,
        completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void
    ) {
        if URLSessionSecurityPolicy.permitsDefaultHandling(
            authenticationMethod: challenge.protectionSpace.authenticationMethod
        ) {
            completionHandler(.performDefaultHandling, nil)
        } else {
            completionHandler(.cancelAuthenticationChallenge, nil)
        }
    }
}
