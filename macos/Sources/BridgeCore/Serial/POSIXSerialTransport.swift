import Darwin
import Dispatch
import Foundation

public final class POSIXSerialTransport: SerialTransporting {
    private let ioQueue: DispatchQueue
    private let stateLock = NSLock()
    private var receiveHandler: ((Data) -> Void)?
    private var disconnectHandler: ((Error?) -> Void)?
    private var fileDescriptor: Int32 = -1
    private var source: DispatchSourceRead?
    private var originalSettings: termios?
    private var connected = false

    public init(queue: DispatchQueue = DispatchQueue(label: "fibp.serial-io")) {
        ioQueue = queue
    }

    deinit { close() }

    public var isOpen: Bool {
        stateLock.lock(); defer { stateLock.unlock() }
        return connected
    }

    public var onReceive: ((Data) -> Void)? {
        get { stateLock.withLock { receiveHandler } }
        set { stateLock.withLock { receiveHandler = newValue } }
    }

    public var onDisconnect: ((Error?) -> Void)? {
        get { stateLock.withLock { disconnectHandler } }
        set { stateLock.withLock { disconnectHandler = newValue } }
    }

    public func open(device: SerialDevice) throws {
        stateLock.lock()
        let wasOpen = connected
        stateLock.unlock()
        guard !wasOpen else { throw SerialTransportError.alreadyOpen }

        let fd = device.calloutPath.withCString {
            Darwin.open($0, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC)
        }
        guard fd >= 0 else { throw SerialTransportError.openFailed(errno) }

        var original = termios()
        guard tcgetattr(fd, &original) == 0 else {
            let code = errno
            Darwin.close(fd)
            throw SerialTransportError.configurationFailed(code)
        }
        var settings = original
        cfmakeraw(&settings)
        settings.c_cflag |= tcflag_t(CLOCAL)
        settings.c_cflag |= tcflag_t(CREAD)
        settings.c_cflag &= ~tcflag_t(PARENB)
        settings.c_cflag &= ~tcflag_t(CSTOPB)
        settings.c_cflag &= ~tcflag_t(CSIZE)
        settings.c_cflag &= ~tcflag_t(CRTSCTS)
        settings.c_cflag |= tcflag_t(CS8)
        guard cfsetspeed(&settings, speed_t(B115200)) == 0,
              tcsetattr(fd, TCSANOW, &settings) == 0 else {
            let code = errno
            Darwin.close(fd)
            throw SerialTransportError.configurationFailed(code)
        }

        let readSource = DispatchSource.makeReadSource(fileDescriptor: fd, queue: ioQueue)
        readSource.setEventHandler { [weak self] in self?.drain(fd: fd) }
        readSource.setCancelHandler { Darwin.close(fd) }

        stateLock.lock()
        fileDescriptor = fd
        source = readSource
        originalSettings = original
        connected = true
        stateLock.unlock()
        readSource.resume()
    }

    public func send(_ data: Data) throws {
        stateLock.lock()
        let fd = fileDescriptor
        let canWrite = connected && fd >= 0
        stateLock.unlock()
        guard canWrite else { throw SerialTransportError.notOpen }
        guard !data.isEmpty else { return }

        ioQueue.async { [weak self] in
            guard let self, self.descriptorIsCurrent(fd) else { return }
            var failure: Int32?
            data.withUnsafeBytes { rawBuffer in
                guard let base = rawBuffer.baseAddress else { return }
                var written = 0
                while written < rawBuffer.count {
                    let writeLength = min(
                        BridgeConfiguration.serialWriteChunkSize,
                        rawBuffer.count - written
                    )
                    let count = Darwin.write(
                        fd,
                        base.advanced(by: written),
                        writeLength
                    )
                    if count > 0 {
                        written += count
                        if written < rawBuffer.count {
                            Darwin.usleep(BridgeConfiguration.serialWritePacingMicroseconds)
                        }
                    } else if count < 0 && errno == EINTR {
                        continue
                    } else if count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) {
                        var descriptor = pollfd(fd: fd, events: Int16(POLLOUT), revents: 0)
                        let pollResult = Darwin.poll(&descriptor, 1, 250)
                        if pollResult == 0 {
                            failure = ETIMEDOUT
                            break
                        } else if pollResult < 0 && errno != EINTR {
                            failure = errno
                            break
                        }
                    } else {
                        failure = errno
                        break
                    }
                }
            }
            if let failure {
                self.fail(fd: fd, error: SerialTransportError.writeFailed(failure))
            }
        }
    }

    public func close() {
        stateLock.lock()
        guard connected else {
            stateLock.unlock()
            return
        }
        connected = false
        let fd = fileDescriptor
        let source = self.source
        let original = originalSettings
        fileDescriptor = -1
        self.source = nil
        originalSettings = nil
        stateLock.unlock()

        ioQueue.async {
            if var original, fd >= 0 {
                _ = tcsetattr(fd, TCSANOW, &original)
            }
            source?.cancel()
        }
    }

    private func drain(fd: Int32) {
        guard descriptorIsCurrent(fd) else { return }
        var bytes = [UInt8](repeating: 0, count: 2_048)
        while true {
            let count = bytes.withUnsafeMutableBytes { rawBuffer in
                Darwin.read(fd, rawBuffer.baseAddress, rawBuffer.count)
            }
            if count > 0 {
                let callback = stateLock.withLock { receiveHandler }
                callback?(Data(bytes.prefix(count)))
                continue
            }
            if count == 0 {
                fail(fd: fd, error: nil)
                return
            }
            if errno == EINTR { continue }
            if errno == EAGAIN || errno == EWOULDBLOCK { return }
            fail(fd: fd, error: SerialTransportError.readFailed(errno))
            return
        }
    }

    private func descriptorIsCurrent(_ fd: Int32) -> Bool {
        stateLock.lock(); defer { stateLock.unlock() }
        return connected && fileDescriptor == fd
    }

    private func fail(fd: Int32, error: Error?) {
        stateLock.lock()
        guard connected, fileDescriptor == fd else {
            stateLock.unlock()
            return
        }
        connected = false
        fileDescriptor = -1
        let source = self.source
        let original = originalSettings
        self.source = nil
        originalSettings = nil
        let callback = disconnectHandler
        stateLock.unlock()
        if var original { _ = tcsetattr(fd, TCSANOW, &original) }
        source?.cancel()
        callback?(error)
    }
}

private extension NSLock {
    func withLock<T>(_ body: () -> T) -> T {
        lock(); defer { unlock() }
        return body()
    }
}
