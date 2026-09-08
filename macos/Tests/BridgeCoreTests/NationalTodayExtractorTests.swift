import Foundation
import XCTest
@testable import BridgeCore

final class NationalTodayExtractorTests: XCTestCase {
    func testOnlyExactNationalTodayPageIsTransformed() {
        func request(_ url: String) -> BridgeHTTPRequest {
            BridgeHTTPRequest(
                requestID: 1,
                method: .get,
                urlString: url,
                headers: [],
                body: Data(),
                timeout: 5
            )
        }
        XCTAssertTrue(NationalTodayExtractor.handles(request(NationalTodayExtractor.sourceURL)))
        XCTAssertFalse(NationalTodayExtractor.handles(request("https://nationaltoday.com/other/")))
        XCTAssertFalse(NationalTodayExtractor.handles(request("https://example.com/today/")))
    }

    func testExtractsDailyParagraphAndRemovesMarkup() throws {
        let html = """
        <html><div class="single-date-header-content"><p><b>Example Day</b> celebrates
        testing &amp; safety &#8212; it&#039;s useful.</p></div><p>Unrelated text</p></html>
        """
        let data = try XCTUnwrap(NationalTodayExtractor.extract(from: Data(html.utf8)))
        XCTAssertEqual(
            String(decoding: data, as: UTF8.self),
            "[[B]]Example Day[[/B]]celebrates testing & safety - it's useful."
        )
    }

    func testRejectsPageWithoutDailyParagraph() {
        XCTAssertNil(NationalTodayExtractor.extract(from: Data("<html></html>".utf8)))
    }
}
