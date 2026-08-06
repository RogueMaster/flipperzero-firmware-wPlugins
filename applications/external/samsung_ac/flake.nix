{
  description = "Samsung AC Remote — Flipper Zero FAP, built against the f6/f7 fbt SDK";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    # reuse wlipurk-appkit's pinned fbt toolchain (single source of truth for the version)
    f6-appkit.url = "github:dappermint/wlipurk-appkit";
    f6-appkit.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, nixpkgs, f6-appkit }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAll = nixpkgs.lib.genAttrs systems;
    in {
      devShells = forAll (system:
        let
          pkgs = import nixpkgs { inherit system; };
          # storage.py (from the firmware tree) needs these to talk over USB CDC
          py = pkgs.python3.withPackages (ps: [ ps.pyserial ps.colorlog ]);
          toolchain = f6-appkit.packages.${system}.flipper-toolchain;
        in {
          default = pkgs.mkShell {
            packages = [ pkgs.just py pkgs.git pkgs.dfu-util pkgs.unzip pkgs.rsync ];
            # Point fbt at the nix-packaged toolchain so it never hits the network.
            FBT_TOOLCHAIN_PATH = "${toolchain}";
            shellHook = ''
              export FBT_NO_SYNC=1
              echo "samsung-ir — toolchain pinned at $FBT_TOOLCHAIN_PATH"
              echo "run 'just' for recipes"
            '';
          };
        });
    };
}
