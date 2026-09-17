import Foundation

public struct SerialDevice: Equatable, Sendable {
    public let registryID: UInt64
    public let calloutPath: String
    public let vendorID: UInt16?
    public let productID: UInt16?
    public let interfaceNumber: Int?
    public let usbSerialNumber: String?
    public let productName: String?

    public init(
        registryID: UInt64,
        calloutPath: String,
        vendorID: UInt16? = nil,
        productID: UInt16? = nil,
        interfaceNumber: Int? = nil,
        usbSerialNumber: String? = nil,
        productName: String? = nil
    ) {
        self.registryID = registryID
        self.calloutPath = calloutPath
        self.vendorID = vendorID
        self.productID = productID
        self.interfaceNumber = interfaceNumber
        self.usbSerialNumber = usbSerialNumber
        self.productName = productName
    }
}

public protocol SerialDeviceMonitoring: AnyObject {
    var onDeviceAdded: ((SerialDevice) -> Void)? { get set }
    var onDeviceRemoved: ((SerialDevice) -> Void)? { get set }
    func start() throws
    func stop()
}

public protocol SerialTransporting: AnyObject {
    var onReceive: ((Data) -> Void)? { get set }
    var onDisconnect: ((Error?) -> Void)? { get set }
    var isOpen: Bool { get }
    func open(device: SerialDevice) throws
    func send(_ data: Data) throws
    func close()
}

public enum SerialTransportError: LocalizedError, Equatable {
    case alreadyOpen
    case openFailed(Int32)
    case configurationFailed(Int32)
    case notOpen
    case readFailed(Int32)
    case writeFailed(Int32)

    public var errorDescription: String? {
        switch self {
        case .alreadyOpen: return "The serial connection is already open."
        case let .openFailed(code): return "The serial port could not be opened (errno \(code))."
        case let .configurationFailed(code): return "The serial port could not be configured (errno \(code))."
        case .notOpen: return "The serial connection is not open."
        case let .readFailed(code): return "The serial port could not be read (errno \(code))."
        case let .writeFailed(code): return "The serial port could not be written (errno \(code))."
        }
    }
}

/// Useful for unit tests and the PTY simulator without weakening production IOKit matching.
public final class ManualSerialDeviceMonitor: SerialDeviceMonitoring {
    private let lock = NSLock()
    private var addedHandler: ((SerialDevice) -> Void)?
    private var removedHandler: ((SerialDevice) -> Void)?

    public var onDeviceAdded: ((SerialDevice) -> Void)? {
        get { lock.withLock { addedHandler } }
        set { lock.withLock { addedHandler = newValue } }
    }
    public var onDeviceRemoved: ((SerialDevice) -> Void)? {
        get { lock.withLock { removedHandler } }
        set { lock.withLock { removedHandler = newValue } }
    }

    public init() {}
    public func start() throws {}
    public func stop() {}
    public func add(_ device: SerialDevice) {
        let callback = lock.withLock { addedHandler }
        callback?(device)
    }
    public func remove(_ device: SerialDevice) {
        let callback = lock.withLock { removedHandler }
        callback?(device)
    }
}

private extension NSLock {
    func withLock<T>(_ body: () -> T) -> T {
        lock(); defer { unlock() }
        return body()
    }
}
