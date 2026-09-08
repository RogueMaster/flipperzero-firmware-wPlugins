import Foundation
import CryptoKit

public protocol PermissionStoring: AnyObject {
    func contains(_ identity: FlipperIdentity) -> Bool
    func grant(_ identity: FlipperIdentity)
    func revoke(_ identity: FlipperIdentity)
}

public final class UserDefaultsPermissionStore: PermissionStoring {
    private let defaults: UserDefaults
    private let storageKey: String
    private let lock = NSLock()

    public init(
        defaults: UserDefaults = .standard,
        storageKey: String = "persistent-device-permissions"
    ) {
        self.defaults = defaults
        self.storageKey = storageKey
    }

    public func contains(_ identity: FlipperIdentity) -> Bool {
        lock.lock()
        defer { lock.unlock() }
        return storedKeys().contains(key(for: identity))
    }

    public func grant(_ identity: FlipperIdentity) {
        lock.lock()
        var keys = storedKeys()
        keys.insert(key(for: identity))
        defaults.set(Array(keys).sorted(), forKey: storageKey)
        lock.unlock()
    }

    public func revoke(_ identity: FlipperIdentity) {
        lock.lock()
        var keys = storedKeys()
        keys.remove(key(for: identity))
        defaults.set(Array(keys).sorted(), forKey: storageKey)
        lock.unlock()
    }

    private func storedKeys() -> Set<String> {
        Set(defaults.stringArray(forKey: storageKey) ?? [])
    }

    private func key(for identity: FlipperIdentity) -> String {
        // The hardware UID is useful for correlation but is not authentication and
        // should not be left verbatim in preferences. Versioned context prevents a
        // digest copied from another permission schema/protocol becoming a grant.
        var material = Data("fibp-permission-schema-\(BridgeConfiguration.permissionSchemaVersion)".utf8)
        material.append(identity.idType)
        material.append(identity.protocolMajor)
        material.append(identity.protocolMinor)
        material.append(identity.deviceID)
        let digest = SHA256.hash(data: material)
        return "sha256:" + digest.map { String(format: "%02x", $0) }.joined()
    }
}

public final class InMemoryPermissionStore: PermissionStoring {
    private var identities = [FlipperIdentity]()
    private let lock = NSLock()

    public init() {}

    public func contains(_ identity: FlipperIdentity) -> Bool {
        lock.lock(); defer { lock.unlock() }
        return identities.contains { Self.samePermissionIdentity($0, identity) }
    }

    public func grant(_ identity: FlipperIdentity) {
        lock.lock(); defer { lock.unlock() }
        if !identities.contains(where: { Self.samePermissionIdentity($0, identity) }) {
            identities.append(identity)
        }
    }

    public func revoke(_ identity: FlipperIdentity) {
        lock.lock(); defer { lock.unlock() }
        identities.removeAll { Self.samePermissionIdentity($0, identity) }
    }

    private static func samePermissionIdentity(_ lhs: FlipperIdentity, _ rhs: FlipperIdentity) -> Bool {
        lhs.idType == rhs.idType
            && lhs.deviceID == rhs.deviceID
            && lhs.protocolMajor == rhs.protocolMajor
            && lhs.protocolMinor == rhs.protocolMinor
    }
}
