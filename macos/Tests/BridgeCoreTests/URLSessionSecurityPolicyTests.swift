import Foundation
import XCTest
@testable import BridgeCore

final class URLSessionSecurityPolicyTests: XCTestCase {
    private final class BlockingResolver: DNSResolving, @unchecked Sendable {
        private let gate = DispatchSemaphore(value: 0)

        func resolve(host: String) throws -> [ResolvedIPAddress] {
            _ = gate.wait(timeout: .now() + 2)
            return [ResolvedIPAddress(family: .ipv4, bytes: [8, 8, 8, 8])]
        }

        func unblock() { gate.signal() }
    }

    func testOnlySystemServerTrustUsesDefaultAuthenticationHandling() {
        XCTAssertTrue(URLSessionSecurityPolicy.permitsDefaultHandling(
            authenticationMethod: NSURLAuthenticationMethodServerTrust
        ))
        for method in [
            NSURLAuthenticationMethodDefault,
            NSURLAuthenticationMethodHTTPBasic,
            NSURLAuthenticationMethodHTTPDigest,
            NSURLAuthenticationMethodClientCertificate,
        ] {
            XCTAssertFalse(URLSessionSecurityPolicy.permitsDefaultHandling(authenticationMethod: method))
        }
    }

    func testRedirectDropsCookieAndCredentialHeaders() throws {
        var request = URLRequest(url: try XCTUnwrap(URL(string: "https://example.com/next")))
        request.httpShouldHandleCookies = true
        request.setValue("Bearer secret", forHTTPHeaderField: "Authorization")
        request.setValue("session=secret", forHTTPHeaderField: "Cookie")
        request.setValue("Basic secret", forHTTPHeaderField: "Proxy-Authorization")
        request.setValue("application/json", forHTTPHeaderField: "Accept")

        let clean = URLSessionSecurityPolicy.sanitizeRedirect(request)
        XCTAssertFalse(clean.httpShouldHandleCookies)
        XCTAssertNil(clean.value(forHTTPHeaderField: "Authorization"))
        XCTAssertNil(clean.value(forHTTPHeaderField: "Cookie"))
        XCTAssertNil(clean.value(forHTTPHeaderField: "Proxy-Authorization"))
        XCTAssertEqual(clean.value(forHTTPHeaderField: "Accept"), "application/json")
    }

    func testRedirectBudgetAllowsExactlyConfiguredMaximum() {
        var budget = RedirectBudget(limit: 3)
        XCTAssertTrue(budget.consume())
        XCTAssertTrue(budget.consume())
        XCTAssertTrue(budget.consume())
        XCTAssertFalse(budget.consume())
        XCTAssertEqual(budget.count, 3)
    }

    func testRequestDeadlineIncludesDNSPreflight() {
        let resolver = BlockingResolver()
        let client = HTTPSNetworkClient(policy: NetworkPolicy(resolver: resolver))
        let completed = expectation(description: "DNS-inclusive deadline")
        var result: Result<Void, BridgeNetworkError>?
        client.execute(
            BridgeHTTPRequest(
                requestID: 1,
                method: .get,
                urlString: "https://example.com/",
                headers: [],
                body: Data(),
                timeout: 0.05
            ),
            onResponse: { _ in XCTFail("DNS preflight should still be blocked") },
            onData: { _ in XCTFail("No response body expected") },
            completion: {
                result = $0
                completed.fulfill()
            }
        )

        wait(for: [completed], timeout: 1)
        if case .failure(.timeout)? = result {
            // Expected.
        } else {
            XCTFail("Expected DNS-inclusive timeout, got \(String(describing: result))")
        }
        resolver.unblock()
        client.invalidate()
    }
}
