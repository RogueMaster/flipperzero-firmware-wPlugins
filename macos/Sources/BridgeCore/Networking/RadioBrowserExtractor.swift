import Foundation

enum RadioBrowserExtractor {
    static let host = "all.api.radio-browser.info"
    static let maximumJSONBytes = 128 * 1_024
    static let maximumStations = 5

    static func handles(_ request: BridgeHTTPRequest) -> Bool {
        guard request.method == .get,
              let components = URLComponents(string: request.urlString) else { return false }
        return components.scheme?.lowercased() == "https" &&
            components.host?.lowercased() == host &&
            components.path == "/json/stations/search"
    }

    static func extract(from data: Data) -> Data? {
        guard let objects = try? JSONSerialization.jsonObject(with: data) as? [[String: Any]] else {
            return nil
        }

        var lines = ["FIBRADIO1"]
        for object in objects.prefix(maximumStations) {
            guard let rawURL = object["url_resolved"] as? String,
                  let url = URL(string: rawURL),
                  url.scheme?.lowercased() == "https",
                  let codec = object["codec"] as? String,
                  codec.caseInsensitiveCompare("MP3") == .orderedSame else { continue }

            let name = field(object["name"] as? String, fallback: "Unknown station", limit: 42)
            let country = field(object["country"] as? String, fallback: "Unknown", limit: 24)
            let state = field(object["state"] as? String, fallback: "", limit: 24)
            let bitrate = (object["bitrate"] as? NSNumber)?.intValue ?? 0
            guard (8...64).contains(bitrate) else { continue }
            lines.append([name, country, state, String(bitrate), rawURL].joined(separator: "\t"))
        }

        let output = lines.joined(separator: "\n") + "\n"
        return Data(output.utf8.prefix(BridgeConfiguration.maximumResponseBytes))
    }

    private static func field(_ value: String?, fallback: String, limit: Int) -> String {
        let source = (value?.isEmpty == false ? value! : fallback)
            .replacingOccurrences(of: "\t", with: " ")
            .replacingOccurrences(of: "\r", with: " ")
            .replacingOccurrences(of: "\n", with: " ")
        return String(source.prefix(limit))
    }
}
