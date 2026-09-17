// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "FlipperInternetBridge",
    platforms: [.macOS(.v13)],
    products: [
        .library(name: "BridgeCore", targets: ["BridgeCore"]),
        .executable(name: "FlipperInternetBridge", targets: ["FlipperInternetBridge"]),
    ],
    targets: [
        .target(
            name: "BridgeCore",
            linkerSettings: [
                .linkedFramework("IOKit"),
            ]
        ),
        .executableTarget(
            name: "FlipperInternetBridge",
            dependencies: ["BridgeCore"],
            linkerSettings: [
                .linkedFramework("AppKit"),
                .linkedFramework("SwiftUI"),
            ]
        ),
        .testTarget(
            name: "BridgeCoreTests",
            dependencies: ["BridgeCore"]
        ),
    ]
)
