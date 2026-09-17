import AppKit
import BridgeCore
import Foundation

final class MacPermissionPrompter: PermissionPrompting {
    private var activeAlert: NSAlert?
    private var promptToken: UUID?

    func requestPermission(
        for identity: FlipperIdentity,
        completion: @escaping (BridgePermissionDecision) -> Void
    ) {
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.cancelOnMain()

            let alert = NSAlert()
            alert.alertStyle = .informational
            alert.messageText = "Flipper Zero is requesting internet access"
            alert.informativeText = "The Flipper Zero named \(identity.displayName) wants to send HTTPS requests through this Mac's internet connection. Your Wi-Fi password is never shared. This application performs only the internet requests sent by the Flipper."
            alert.addButton(withTitle: "Deny")
            alert.addButton(withTitle: "Allow Once")
            alert.addButton(withTitle: "Always Allow")
            alert.buttons[0].keyEquivalent = "\r"
            alert.buttons[1].keyEquivalent = ""
            alert.buttons[2].keyEquivalent = ""
            let token = UUID()
            self.activeAlert = alert
            self.promptToken = token

            NSApp.activate(ignoringOtherApps: true)
            let response = alert.runModal()
            guard self.promptToken == token else { return }
            self.activeAlert = nil
            self.promptToken = nil
            switch response {
            case .alertSecondButtonReturn: completion(.allowOnce)
            case .alertThirdButtonReturn: completion(.alwaysAllow)
            default: completion(.deny)
            }
        }
    }

    func cancelPendingPrompt() {
        DispatchQueue.main.async { [weak self] in self?.cancelOnMain() }
    }

    private func cancelOnMain() {
        guard let activeAlert else { return }
        promptToken = nil
        self.activeAlert = nil
        NSApp.abortModal()
        activeAlert.window.orderOut(nil)
    }
}
