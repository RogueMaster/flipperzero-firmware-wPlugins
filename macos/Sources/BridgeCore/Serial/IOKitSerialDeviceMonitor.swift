import Foundation
import IOKit
import IOKit.serial

public enum SerialMonitorError: LocalizedError {
    case notificationPortUnavailable
    case registrationFailed(kern_return_t)

    public var errorDescription: String? {
        switch self {
        case .notificationPortUnavailable:
            return "The IOKit notification port could not be created."
        case let .registrationFailed(code):
            return "The IOKit serial-device notification could not be registered (\(code))."
        }
    }
}

public final class IOKitSerialDeviceMonitor: SerialDeviceMonitoring {
    private let queue: DispatchQueue
    private let handlerLock = NSLock()
    private var deviceAddedHandler: ((SerialDevice) -> Void)?
    private var deviceRemovedHandler: ((SerialDevice) -> Void)?
    private let queueKey = DispatchSpecificKey<UInt8>()
    private var notificationPort: IONotificationPortRef?
    private var arrivalIterator: io_iterator_t = 0
    private var terminationIterator: io_iterator_t = 0
    private var devicesByRegistryID = [UInt64: SerialDevice]()
    private var started = false

    public init(queue: DispatchQueue = DispatchQueue(label: "fibp.serial-monitor")) {
        self.queue = queue
        queue.setSpecific(key: queueKey, value: 1)
    }

    deinit { stop() }

    public var onDeviceAdded: ((SerialDevice) -> Void)? {
        get { handlerLock.withLock { deviceAddedHandler } }
        set { handlerLock.withLock { deviceAddedHandler = newValue } }
    }

    public var onDeviceRemoved: ((SerialDevice) -> Void)? {
        get { handlerLock.withLock { deviceRemovedHandler } }
        set { handlerLock.withLock { deviceRemovedHandler = newValue } }
    }

    public func start() throws {
        var startError: Error?
        performSync {
            guard !started else { return }
            guard let port = IONotificationPortCreate(kIOMainPortDefault) else {
                startError = SerialMonitorError.notificationPortUnavailable
                return
            }
            notificationPort = port
            IONotificationPortSetDispatchQueue(port, queue)
            let context = Unmanaged.passUnretained(self).toOpaque()

            guard let arrivalMatch = IOServiceMatching(kIOSerialBSDServiceValue) else {
                startError = SerialMonitorError.notificationPortUnavailable
                tearDown()
                return
            }
            var result = IOServiceAddMatchingNotification(
                port,
                kIOFirstMatchNotification,
                arrivalMatch,
                fibpSerialArrivalCallback,
                context,
                &arrivalIterator
            )
            guard result == KERN_SUCCESS else {
                startError = SerialMonitorError.registrationFailed(result)
                tearDown()
                return
            }

            guard let terminationMatch = IOServiceMatching(kIOSerialBSDServiceValue) else {
                startError = SerialMonitorError.notificationPortUnavailable
                tearDown()
                return
            }
            result = IOServiceAddMatchingNotification(
                port,
                kIOTerminatedNotification,
                terminationMatch,
                fibpSerialTerminationCallback,
                context,
                &terminationIterator
            )
            guard result == KERN_SUCCESS else {
                startError = SerialMonitorError.registrationFailed(result)
                tearDown()
                return
            }

            started = true
            consumeArrival(iterator: arrivalIterator) // Draining arms the notification.
            consumeTermination(iterator: terminationIterator)
        }
        if let startError { throw startError }
    }

    public func stop() {
        performSync { tearDown() }
    }

    fileprivate func consumeArrival(iterator: io_iterator_t) {
        while true {
            let service = IOIteratorNext(iterator)
            guard service != IO_OBJECT_NULL else { break }
            defer { IOObjectRelease(service) }
            guard let device = makeDevice(from: service) else { continue }
            devicesByRegistryID[device.registryID] = device
            let callback = handlerLock.withLock { deviceAddedHandler }
            callback?(device)
        }
    }

    fileprivate func consumeTermination(iterator: io_iterator_t) {
        while true {
            let service = IOIteratorNext(iterator)
            guard service != IO_OBJECT_NULL else { break }
            defer { IOObjectRelease(service) }
            var registryID: UInt64 = 0
            guard IORegistryEntryGetRegistryEntryID(service, &registryID) == KERN_SUCCESS,
                  let device = devicesByRegistryID.removeValue(forKey: registryID) else {
                continue
            }
            let callback = handlerLock.withLock { deviceRemovedHandler }
            callback?(device)
        }
    }

    private func makeDevice(from service: io_service_t) -> SerialDevice? {
        guard let path = directStringProperty(service, key: "IOCalloutDevice"),
              path.hasPrefix("/dev/cu.") else {
            return nil
        }

        let vendor = parentNumberProperty(service, key: "idVendor").map { UInt16(truncating: $0) }
        let product = parentNumberProperty(service, key: "idProduct").map { UInt16(truncating: $0) }
        guard vendor == BridgeConfiguration.flipperVendorID,
              product == BridgeConfiguration.flipperProductID else {
            return nil
        }

        let interfaceNumber = parentNumberProperty(service, key: "bInterfaceNumber")?.intValue
        if let interfaceNumber,
           !BridgeConfiguration.bridgeUSBInterfaceNumbers.contains(interfaceNumber) {
            return nil
        }

        var registryID: UInt64 = 0
        guard IORegistryEntryGetRegistryEntryID(service, &registryID) == KERN_SUCCESS else {
            return nil
        }

        return SerialDevice(
            registryID: registryID,
            calloutPath: path,
            vendorID: vendor,
            productID: product,
            interfaceNumber: interfaceNumber,
            usbSerialNumber: parentStringProperty(service, key: "USB Serial Number"),
            productName: parentStringProperty(service, key: "USB Product Name")
        )
    }

    private func directStringProperty(_ service: io_service_t, key: String) -> String? {
        IORegistryEntryCreateCFProperty(
            service,
            key as CFString,
            kCFAllocatorDefault,
            0
        )?.takeRetainedValue() as? String
    }

    private func parentStringProperty(_ service: io_service_t, key: String) -> String? {
        searchParentProperty(service, key: key) as? String
    }

    private func parentNumberProperty(_ service: io_service_t, key: String) -> NSNumber? {
        searchParentProperty(service, key: key) as? NSNumber
    }

    private func searchParentProperty(_ service: io_service_t, key: String) -> Any? {
        let options = IOOptionBits(kIORegistryIterateParents | kIORegistryIterateRecursively)
        return IORegistryEntrySearchCFProperty(
            service,
            kIOServicePlane,
            key as CFString,
            kCFAllocatorDefault,
            options
        )
    }

    private func tearDown() {
        guard notificationPort != nil || arrivalIterator != 0 || terminationIterator != 0 else {
            started = false
            return
        }
        if arrivalIterator != 0 {
            IOObjectRelease(arrivalIterator)
            arrivalIterator = 0
        }
        if terminationIterator != 0 {
            IOObjectRelease(terminationIterator)
            terminationIterator = 0
        }
        if let notificationPort {
            IONotificationPortDestroy(notificationPort)
            self.notificationPort = nil
        }
        devicesByRegistryID.removeAll()
        started = false
    }

    private func performSync(_ action: () -> Void) {
        if DispatchQueue.getSpecific(key: queueKey) != nil {
            action()
        } else {
            queue.sync(execute: action)
        }
    }
}

private extension NSLock {
    func withLock<T>(_ body: () -> T) -> T {
        lock(); defer { unlock() }
        return body()
    }
}

private func fibpSerialArrivalCallback(
    _ context: UnsafeMutableRawPointer?,
    _ iterator: io_iterator_t
) {
    guard let context else { return }
    Unmanaged<IOKitSerialDeviceMonitor>
        .fromOpaque(context)
        .takeUnretainedValue()
        .consumeArrival(iterator: iterator)
}

private func fibpSerialTerminationCallback(
    _ context: UnsafeMutableRawPointer?,
    _ iterator: io_iterator_t
) {
    guard let context else { return }
    Unmanaged<IOKitSerialDeviceMonitor>
        .fromOpaque(context)
        .takeUnretainedValue()
        .consumeTermination(iterator: iterator)
}
