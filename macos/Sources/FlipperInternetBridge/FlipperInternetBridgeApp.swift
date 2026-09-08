import AppKit
import BridgeCore
import SwiftUI

final class BridgeAppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
    }
}

@main
struct FlipperInternetBridgeApp: App {
    @NSApplicationDelegateAdaptor(BridgeAppDelegate.self) private var appDelegate
    @StateObject private var model = BridgeMenuModel()

    var body: some Scene {
        MenuBarExtra {
            BridgeMenu(model: model)
        } label: {
            Label("Flipper USB Internet Bridge", systemImage: model.statusSymbol)
        }
        .menuBarExtraStyle(.menu)

        Window("Flipper USB Internet Bridge Diagnostics", id: "diagnostics") {
            DiagnosticsView(model: model)
        }
        .defaultSize(width: 680, height: 420)
    }
}

private struct BridgeMenu: View {
    @ObservedObject var model: BridgeMenuModel
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        Text(model.connectionText)
        Text(model.accessText)
        Text(model.identityText)
        Text(model.snapshot.statusText)
        if let startupError = model.startupError {
            Text("Error: \(startupError)")
        }

        Divider()

        Button("Allow This Device") { model.grantAlways() }
            .disabled(!model.canGrant)
        Button("Revoke Device Permission") { model.revoke() }
            .disabled(!model.canRevoke)
        Button("Cancel Active Request") { model.cancelRequest() }
            .disabled(!model.canCancel)

        Divider()

        Button("Show Diagnostics") {
            NSApp.activate(ignoringOtherApps: true)
            openWindow(id: "diagnostics")
        }
        Button("Quit") {
            model.stop()
            NSApp.terminate(nil)
        }
            .keyboardShortcut("q")
    }
}

private struct DiagnosticsView: View {
    @ObservedObject var model: BridgeMenuModel

    private static let dateFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateStyle = .none
        formatter.timeStyle = .medium
        return formatter
    }()

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Diagnostics")
                .font(.title2)
            Text("URL queries, bodies, header values, and full device IDs are not logged.")
                .font(.caption)
                .foregroundStyle(.secondary)
            List(model.diagnosticsEntries) { entry in
                HStack(alignment: .top, spacing: 10) {
                    Text(Self.dateFormatter.string(from: entry.timestamp))
                        .monospacedDigit()
                        .foregroundStyle(.secondary)
                    Text(entry.level.rawValue.uppercased())
                        .font(.caption.monospaced())
                        .frame(width: 62, alignment: .leading)
                    Text(entry.message)
                        .textSelection(.enabled)
                }
            }
        }
        .padding()
    }
}
