import Foundation
import XCTest
@testable import BridgeCore

final class RadioBrowserExtractorTests: XCTestCase {
    func testCompactsHTTPSMP3StationsAndDropsUnsupportedStreams() throws {
        let json = """
        [
          {"name":"Radio One","country":"Turkey","state":"Istanbul","codec":"MP3","bitrate":64,"url_resolved":"https://radio.example/live.mp3"},
          {"name":"Heavy Radio","country":"Turkey","state":"","codec":"MP3","bitrate":128,"url_resolved":"https://heavy.example/live.mp3"},
          {"name":"AAC Radio","country":"UK","state":"","codec":"AAC","bitrate":64,"url_resolved":"https://aac.example/live"},
          {"name":"HTTP Radio","country":"US","state":"","codec":"MP3","bitrate":64,"url_resolved":"http://radio.example/live.mp3"}
        ]
        """
        let output = try XCTUnwrap(RadioBrowserExtractor.extract(from: Data(json.utf8)))
        let text = try XCTUnwrap(String(data: output, encoding: .utf8))
        XCTAssertEqual(
            text,
            "FIBRADIO1\nRadio One\tTurkey\tIstanbul\t64\thttps://radio.example/live.mp3\n"
        )
    }

    func testOnlyHandlesExactRadioBrowserSearchEndpoint() {
        let request = BridgeHTTPRequest(
            requestID: 1,
            method: .get,
            urlString: "https://all.api.radio-browser.info/json/stations/search?limit=5",
            headers: [],
            body: Data(),
            timeout: 10
        )
        XCTAssertTrue(RadioBrowserExtractor.handles(request))
    }
}
