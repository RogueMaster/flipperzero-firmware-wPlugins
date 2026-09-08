import Darwin
import Foundation

public enum IPAddressFamily: Equatable, Hashable, Sendable {
    case ipv4
    case ipv6
}

public struct ResolvedIPAddress: Equatable, Hashable, Sendable {
    public let family: IPAddressFamily
    public let bytes: [UInt8]

    public init(family: IPAddressFamily, bytes: [UInt8]) {
        self.family = family
        self.bytes = bytes
    }

    public var isGloballyRoutable: Bool {
        switch family {
        case .ipv4: return Self.isGlobalIPv4(bytes)
        case .ipv6: return Self.isGlobalIPv6(bytes)
        }
    }

    private static func isGlobalIPv4(_ b: [UInt8]) -> Bool {
        guard b.count == 4 else { return false }
        if b[0] == 0 || b[0] == 10 || b[0] == 127 { return false }
        if b[0] == 100 && (b[1] & 0xC0) == 0x40 { return false } // CGNAT /10
        if b[0] == 169 && b[1] == 254 { return false }
        if b[0] == 172 && (16...31).contains(b[1]) { return false }
        if b[0] == 192 && b[1] == 168 { return false }
        if b[0] == 192 && b[1] == 0 && b[2] == 0 { return false }
        if b[0] == 192 && b[1] == 0 && b[2] == 2 { return false }
        if b[0] == 192 && b[1] == 88 && b[2] == 99 { return false }
        if b[0] == 198 && (b[1] == 18 || b[1] == 19) { return false }
        if b[0] == 198 && b[1] == 51 && b[2] == 100 { return false }
        if b[0] == 203 && b[1] == 0 && b[2] == 113 { return false }
        if b[0] >= 224 { return false } // multicast and reserved
        return true
    }

    private static func isGlobalIPv6(_ b: [UInt8]) -> Bool {
        guard b.count == 16 else { return false }
        if b.allSatisfy({ $0 == 0 }) { return false }
        if b.dropLast().allSatisfy({ $0 == 0 }) && b.last == 1 { return false }

        // IPv4-mapped IPv6 must inherit the embedded address policy.
        if b[0..<10].allSatisfy({ $0 == 0 }), b[10] == 0xFF, b[11] == 0xFF {
            return isGlobalIPv4(Array(b[12..<16]))
        }
        if (b[0] & 0xFE) == 0xFC { return false } // unique-local /7
        if b[0] == 0xFE && (b[1] & 0xC0) == 0x80 { return false } // link-local /10
        if b[0] == 0xFF { return false } // multicast

        // Conservative global-unicast policy. It intentionally excludes NAT64,
        // unspecified legacy prefixes, and transition mechanisms for this MVP.
        guard (b[0] & 0xE0) == 0x20 else { return false } // 2000::/3
        // Fail closed for the IANA special-purpose 2001::/23 block. It includes
        // transition/benchmark/ORCHID ranges; a few narrow anycast exceptions
        // are deliberately not exposed by this application-layer proxy.
        if b[0] == 0x20 && b[1] == 0x01 && (b[2] & 0xFE) == 0 { return false }
        if b[0] == 0x20 && b[1] == 0x01 && b[2] == 0x0D && b[3] == 0xB8 {
            return false // documentation 2001:db8::/32
        }
        if b[0] == 0x20 && b[1] == 0x02 { return false } // 6to4 embeds IPv4
        if b[0] == 0x3F && b[1] == 0xFE { return false } // deprecated 6bone /16
        if b[0] == 0x3F && b[1] == 0xFF && (b[2] & 0xF0) == 0 {
            return false // documentation/reserved 3fff::/20
        }
        return true
    }
}

public protocol DNSResolving: Sendable {
    func resolve(host: String) throws -> [ResolvedIPAddress]
}

public enum DNSResolutionError: Error, Equatable {
    case failed(Int32)
    case noAddresses
}

public struct SystemDNSResolver: DNSResolving {
    public init() {}

    public func resolve(host: String) throws -> [ResolvedIPAddress] {
        var hints = addrinfo(
            ai_flags: 0,
            ai_family: AF_UNSPEC,
            ai_socktype: SOCK_STREAM,
            ai_protocol: IPPROTO_TCP,
            ai_addrlen: 0,
            ai_canonname: nil,
            ai_addr: nil,
            ai_next: nil
        )
        var result: UnsafeMutablePointer<addrinfo>?
        let status = getaddrinfo(host, nil, &hints, &result)
        guard status == 0 else { throw DNSResolutionError.failed(status) }
        defer { if let result { freeaddrinfo(result) } }

        var addresses = Set<ResolvedIPAddress>()
        var cursor = result
        while let current = cursor {
            let info = current.pointee
            if info.ai_family == AF_INET, let address = info.ai_addr {
                var ipv4 = UnsafeRawPointer(address)
                    .assumingMemoryBound(to: sockaddr_in.self)
                    .pointee
                    .sin_addr
                let bytes = withUnsafeBytes(of: &ipv4) { Array($0.prefix(4)) }
                addresses.insert(ResolvedIPAddress(family: .ipv4, bytes: bytes))
            } else if info.ai_family == AF_INET6, let address = info.ai_addr {
                var ipv6 = UnsafeRawPointer(address)
                    .assumingMemoryBound(to: sockaddr_in6.self)
                    .pointee
                    .sin6_addr
                let bytes = withUnsafeBytes(of: &ipv6) { Array($0.prefix(16)) }
                addresses.insert(ResolvedIPAddress(family: .ipv6, bytes: bytes))
            }
            cursor = info.ai_next
        }
        guard !addresses.isEmpty else { throw DNSResolutionError.noAddresses }
        return Array(addresses)
    }
}

public enum NetworkPolicyError: LocalizedError, Equatable {
    case invalidURL
    case urlTooLong
    case insecureScheme
    case missingHost
    case embeddedCredentials
    case localHostname
    case resolutionFailed
    case nonGlobalAddress
    case tooManyRedirects

    public var errorDescription: String? {
        switch self {
        case .invalidURL: return "The URL is invalid."
        case .urlTooLong: return "The URL exceeds the allowed length."
        case .insecureScheme: return "Only HTTPS requests are allowed."
        case .missingHost: return "The URL does not contain a destination host."
        case .embeddedCredentials: return "Credentials are not allowed in URLs."
        case .localHostname: return "Local network targets are blocked for security."
        case .resolutionFailed: return "The destination could not be resolved by DNS."
        case .nonGlobalAddress: return "Private, local, or reserved IP addresses are blocked."
        case .tooManyRedirects: return "Too many redirects."
        }
    }
}

public struct NetworkPolicy: Sendable {
    private let resolver: any DNSResolving

    public init(resolver: any DNSResolving = SystemDNSResolver()) {
        self.resolver = resolver
    }

    public func validate(urlString: String) throws -> URL {
        guard urlString.utf8.count <= BridgeConfiguration.maximumURLBytes else {
            throw NetworkPolicyError.urlTooLong
        }
        guard let components = URLComponents(string: urlString),
              let scheme = components.scheme,
              let url = components.url else {
            throw NetworkPolicyError.invalidURL
        }
        guard scheme.lowercased() == "https" else {
            throw NetworkPolicyError.insecureScheme
        }
        guard components.user == nil, components.password == nil else {
            throw NetworkPolicyError.embeddedCredentials
        }
        guard let rawHost = components.host, !rawHost.isEmpty else {
            throw NetworkPolicyError.missingHost
        }
        let host = rawHost.lowercased().trimmingCharacters(in: CharacterSet(charactersIn: "."))
        guard host != "localhost",
              !host.hasSuffix(".localhost"),
              !host.hasSuffix(".local") else {
            throw NetworkPolicyError.localHostname
        }

        let addresses: [ResolvedIPAddress]
        do {
            addresses = try resolver.resolve(host: host)
        } catch {
            throw NetworkPolicyError.resolutionFailed
        }
        guard !addresses.isEmpty, addresses.allSatisfy(\.isGloballyRoutable) else {
            throw NetworkPolicyError.nonGlobalAddress
        }
        return url
    }

    public func allowedRequestHeaders(_ headers: [BridgeHTTPHeader]) -> [BridgeHTTPHeader] {
        headers.filter { header in
            BridgeConfiguration.requestHeaderAllowList.contains(header.name.lowercased())
                && BridgeHTTPHeader.isValidName(header.name)
                && BridgeHTTPHeader.isValidValue(header.value)
        }
    }

    public func allowedResponseHeaders(_ fields: [AnyHashable: Any]) -> [BridgeHTTPHeader] {
        fields.compactMap { key, value -> BridgeHTTPHeader? in
            guard let name = key as? String,
                  let value = value as? String,
                  BridgeConfiguration.responseHeaderAllowList.contains(name.lowercased()),
                  !name.caseInsensitiveCompare("set-cookie").isOrderedSame,
                  BridgeHTTPHeader.isValidName(name),
                  BridgeHTTPHeader.isValidValue(value),
                  name.utf8.count <= BridgeConfiguration.maximumHeaderNameBytes,
                  value.utf8.count <= BridgeConfiguration.maximumHeaderValueBytes,
                  !value.contains("\r"), !value.contains("\n") else {
                return nil
            }
            return BridgeHTTPHeader(name: name, value: value)
        }
        .prefix(BridgeConfiguration.maximumHeaderCount)
        .map { $0 }
    }
}

private extension ComparisonResult {
    var isOrderedSame: Bool { self == .orderedSame }
}
