import AppKit
import BridgeCore
import Foundation
import SwiftUI

@MainActor
final class BridgeMenuModel: ObservableObject {
    @Published private(set) var snapshot = BridgeStatusSnapshot(
        state: .disconnected,
        statusText: "Flipper not connected",
        device: nil,
        identity: nil,
        permission: nil,
        activeRequestID: nil
    )
    @Published private(set) var diagnosticsEntries = [DiagnosticEntry]()
    @Published private(set) var startupError: String?

    let diagnostics: DiagnosticsLog
    private let coordinator: BridgeCoordinator
    private var terminationObserver: NSObjectProtocol?
    #if DEBUG
    private let manualMonitor: ManualSerialDeviceMonitor?
    #endif

    init() {
        let diagnostics = DiagnosticsLog()
        let permissionPrompt = MacPermissionPrompter()
        let transport = POSIXSerialTransport()
        let permissionStore = UserDefaultsPermissionStore()
        let networkClient = HTTPSNetworkClient(policy: NetworkPolicy())

        let selectedMonitor: any SerialDeviceMonitoring
        #if DEBUG
        let debugPath = ProcessInfo.processInfo.environment["FIB_SERIAL_PORT"]
            .flatMap { $0.isEmpty ? nil : $0 }
        let manual: ManualSerialDeviceMonitor?
        if debugPath != nil {
            let created = ManualSerialDeviceMonitor()
            selectedMonitor = created
            manual = created
        } else {
            selectedMonitor = IOKitSerialDeviceMonitor()
            manual = nil
        }
        manualMonitor = manual
        #else
        selectedMonitor = IOKitSerialDeviceMonitor()
        #endif

        let coordinator = BridgeCoordinator(
            monitor: selectedMonitor,
            transport: transport,
            permissions: permissionStore,
            permissionPrompt: permissionPrompt,
            httpClient: networkClient,
            diagnostics: diagnostics
        )
        self.diagnostics = diagnostics
        self.coordinator = coordinator
        terminationObserver = NotificationCenter.default.addObserver(
            forName: NSApplication.willTerminateNotification,
            object: nil,
            queue: nil
        ) { [weak coordinator] _ in
            coordinator?.stop()
        }

        coordinator.onStatusChange = { [weak self] snapshot in
            DispatchQueue.main.async { self?.snapshot = snapshot }
        }
        diagnostics.onChange = { [weak self] entries in
            DispatchQueue.main.async { self?.diagnosticsEntries = entries }
        }
        do {
            try coordinator.start()
            snapshot = coordinator.snapshot()
            #if DEBUG
            if let path = ProcessInfo.processInfo.environment["FIB_SERIAL_PORT"],
               !path.isEmpty {
                diagnostics.append(.warning, "DEBUG PTY override is enabled.")
                manualMonitor?.add(SerialDevice(
                    registryID: 1,
                    calloutPath: path,
                    vendorID: BridgeConfiguration.flipperVendorID,
                    productID: BridgeConfiguration.flipperProductID,
                    interfaceNumber: 2,
                    productName: "FIBP PTY Simulator"
                ))
            }
            #endif
        } catch {
            startupError = error.localizedDescription
            diagnostics.append(.error, "Startup error: \(error.localizedDescription)")
        }
    }

    deinit {
        if let terminationObserver {
            NotificationCenter.default.removeObserver(terminationObserver)
        }
        coordinator.stop()
    }

    var statusSymbol: String {
        if snapshot.internetAccessEnabled { return "network.badge.shield.half.filled" }
        if snapshot.isConnected { return "cable.connector" }
        return "cable.connector.slash"
    }

    var connectionText: String {
        snapshot.isConnected ? "Flipper connected" : "Flipper not connected"
    }

    var accessText: String {
        snapshot.internetAccessEnabled ? "Internet access enabled" : "Internet access disabled"
    }

    var identityText: String {
        guard let identity = snapshot.identity else { return "Device identity: —" }
        return "Device: \(identity.displayName) (\(identity.redactedID))"
    }

    var canGrant: Bool {
        guard snapshot.identity != nil else { return false }
        return snapshot.permission != .allowedOnce && snapshot.permission != .alwaysAllowed
    }

    var canRevoke: Bool { snapshot.identity != nil }
    var canCancel: Bool { snapshot.activeRequestID != nil }

    func grantAlways() { coordinator.grantAlwaysForCurrentDevice() }
    func revoke() { coordinator.revokeCurrentDevicePermission() }
    func cancelRequest() { coordinator.cancelActiveRequest() }
    func stop() { coordinator.stop() }
}
