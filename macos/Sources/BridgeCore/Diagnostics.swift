import Foundation

public enum DiagnosticLevel: String, Sendable {
    case debug
    case info
    case warning
    case error
}

public struct DiagnosticEntry: Identifiable, Equatable, Sendable {
    public let id: UUID
    public let timestamp: Date
    public let level: DiagnosticLevel
    public let message: String

    public init(
        id: UUID = UUID(),
        timestamp: Date = Date(),
        level: DiagnosticLevel,
        message: String
    ) {
        self.id = id
        self.timestamp = timestamp
        self.level = level
        self.message = message
    }
}

public final class DiagnosticsLog {
    private let lock = NSLock()
    private let capacity: Int
    private var entries = [DiagnosticEntry]()
    private var changeHandler: (([DiagnosticEntry]) -> Void)?

    public var onChange: (([DiagnosticEntry]) -> Void)? {
        get {
            lock.lock(); defer { lock.unlock() }
            return changeHandler
        }
        set {
            lock.lock(); defer { lock.unlock() }
            changeHandler = newValue
        }
    }

    public init(capacity: Int = 200) {
        self.capacity = max(1, capacity)
    }

    public func append(_ level: DiagnosticLevel, _ message: String) {
        let sanitized = Self.sanitize(message)
        lock.lock()
        entries.append(DiagnosticEntry(level: level, message: sanitized))
        if entries.count > capacity {
            entries.removeFirst(entries.count - capacity)
        }
        let snapshot = entries
        let callback = changeHandler
        lock.unlock()
        callback?(snapshot)
    }

    public func snapshot() -> [DiagnosticEntry] {
        lock.lock()
        defer { lock.unlock() }
        return entries
    }

    private static func sanitize(_ value: String) -> String {
        let scalars = value.unicodeScalars.filter { scalar in
            scalar == "\n" || (scalar.value >= 0x20 && scalar.value != 0x7F)
        }
        return String(String.UnicodeScalarView(scalars)).prefix(512).description
    }
}
