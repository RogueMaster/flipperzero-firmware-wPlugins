import Foundation
import XCTest
@testable import BridgeCore

final class NetworkPolicyTests: XCTestCase {
    private struct Resolver: DNSResolving {
        let addresses: [ResolvedIPAddress]
        let shouldFail: Bool

        init(_ addresses: [ResolvedIPAddress], shouldFail: Bool = false) {
            self.addresses = addresses
            self.shouldFail = shouldFail
        }

        func resolve(host: String) throws -> [ResolvedIPAddress] {
            if shouldFail { throw DNSResolutionError.noAddresses }
            return addresses
        }
    }

    private let publicIPv4 = ResolvedIPAddress(family: .ipv4, bytes: [8, 8, 8, 8])

    func testAllowsHTTPSWithOnlyGlobalAddresses() throws {
        let policy = NetworkPolicy(resolver: Resolver([publicIPv4]))
        XCTAssertEqual(try policy.validate(urlString: "https://example.com/demo?q=1").scheme, "https")
    }

    func testBlocksHTTPAndEmbeddedCredentials() {
        let policy = NetworkPolicy(resolver: Resolver([publicIPv4]))
        XCTAssertThrowsError(try policy.validate(urlString: "http://example.com")) {
            XCTAssertEqual($0 as? NetworkPolicyError, .insecureScheme)
        }
        XCTAssertThrowsError(try policy.validate(urlString: "https://user:pass@example.com")) {
            XCTAssertEqual($0 as? NetworkPolicyError, .embeddedCredentials)
        }
    }

    func testBlocksLocalHostnamesWithoutDNS() {
        let policy = NetworkPolicy(resolver: Resolver([], shouldFail: true))
        for url in ["https://localhost/", "https://api.localhost/", "https://printer.local/"] {
            XCTAssertThrowsError(try policy.validate(urlString: url)) {
                XCTAssertEqual($0 as? NetworkPolicyError, .localHostname)
            }
        }
    }

    func testBlocksPrivateLoopbackLinkLocalCGNATAndMixedDNSAnswers() {
        let blocked: [[UInt8]] = [
            [0, 0, 0, 0], [10, 0, 0, 1], [127, 0, 0, 1], [100, 64, 0, 1],
            [169, 254, 1, 2], [172, 16, 0, 1], [192, 168, 1, 1], [224, 0, 0, 1],
        ]
        for bytes in blocked {
            let address = ResolvedIPAddress(family: .ipv4, bytes: bytes)
            XCTAssertFalse(address.isGloballyRoutable, "unexpected global address: \(bytes)")
            let policy = NetworkPolicy(resolver: Resolver([address]))
            XCTAssertThrowsError(try policy.validate(urlString: "https://example.com")) {
                XCTAssertEqual($0 as? NetworkPolicyError, .nonGlobalAddress)
            }
        }

        let mixed = NetworkPolicy(resolver: Resolver([
            publicIPv4,
            ResolvedIPAddress(family: .ipv4, bytes: [192, 168, 1, 20]),
        ]))
        XCTAssertThrowsError(try mixed.validate(urlString: "https://example.com")) {
            XCTAssertEqual($0 as? NetworkPolicyError, .nonGlobalAddress)
        }
    }

    func testRedirectTargetIsRevalidatedAgainstPrivateAddress() throws {
        let initialPolicy = NetworkPolicy(resolver: Resolver([publicIPv4]))
        _ = try initialPolicy.validate(urlString: "https://public.example/start")

        let redirectedPolicy = NetworkPolicy(resolver: Resolver([
            ResolvedIPAddress(family: .ipv4, bytes: [127, 0, 0, 1]),
        ]))
        XCTAssertThrowsError(try redirectedPolicy.validate(urlString: "https://redirect.example/admin")) {
            XCTAssertEqual($0 as? NetworkPolicyError, .nonGlobalAddress)
        }
    }

    func testHeaderAllowListDropsAuthenticationCredentialsAndCookies() {
        let policy = NetworkPolicy(resolver: Resolver([publicIPv4]))
        let filtered = policy.allowedRequestHeaders([
            BridgeHTTPHeader(name: "Accept", value: "application/json"),
            BridgeHTTPHeader(name: "Authorization", value: "Bearer secret"),
            BridgeHTTPHeader(name: "Cookie", value: "session=secret"),
            BridgeHTTPHeader(name: "User-Agent", value: "device-controlled"),
            BridgeHTTPHeader(name: "X-Test", value: "ignored"),
        ])
        XCTAssertEqual(filtered, [BridgeHTTPHeader(name: "Accept", value: "application/json")])
        XCTAssertEqual(BridgeConfiguration.fixedUserAgent, "FlipperUSBInternetBridge/0.3")
    }

    func testIPv6LoopbackLinkLocalUniqueLocalMappedPrivateAndMulticastAreBlocked() {
        let blocked: [[UInt8]] = [
            Array(repeating: 0, count: 15) + [1],
            [0xFE, 0x80] + Array(repeating: 0, count: 14),
            [0xFC, 0] + Array(repeating: 0, count: 14),
            Array(repeating: 0, count: 10) + [0xFF, 0xFF, 192, 168, 1, 2],
            [0xFF, 0x02] + Array(repeating: 0, count: 14),
        ]
        for bytes in blocked {
            XCTAssertFalse(ResolvedIPAddress(family: .ipv6, bytes: bytes).isGloballyRoutable)
        }
        XCTAssertTrue(ResolvedIPAddress(
            family: .ipv6,
            bytes: [0x26, 0x06, 0x47, 0x00] + Array(repeating: 0, count: 12)
        ).isGloballyRoutable)
    }

    func testIPv6SpecialTransitionBenchmarkAndDocumentationRangesAreBlocked() {
        let blocked: [[UInt8]] = [
            [0x20, 0x01, 0x00, 0x00] + Array(repeating: 0, count: 12), // Teredo
            [0x20, 0x01, 0x00, 0x02] + Array(repeating: 0, count: 12), // benchmark
            [0x20, 0x01, 0x00, 0x10] + Array(repeating: 0, count: 12), // ORCHID
            [0x20, 0x02, 0, 0] + Array(repeating: 0, count: 12),       // 6to4
            [0x3F, 0xFE, 0, 0] + Array(repeating: 0, count: 12),       // 6bone
            [0x3F, 0xFF, 0x00, 0] + Array(repeating: 0, count: 12),   // docs
        ]
        for bytes in blocked {
            XCTAssertFalse(
                ResolvedIPAddress(family: .ipv6, bytes: bytes).isGloballyRoutable,
                "unexpected globally routable special IPv6 address: \(bytes)"
            )
        }
    }
}
