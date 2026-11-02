# Build environment for the Ducobox Flipper app.
#
# ufbt downloads the official Flipper SDK and ARM toolchain, which are generic
# Linux binaries (python3, arm-none-eabi-gcc) that link against /lib64. Stock
# NixOS installs the stub dynamic loader there, so those binaries cannot run
# outside an FHS environment. This shell is an FHS environment for that reason:
# it provides a real glibc loader and the usual libraries, so ufbt and the
# downloaded toolchain work without enabling programs.nix-ld system-wide.

let
  pkgs = import <nixpkgs> { };

  ufbt = pkgs.python3Packages.buildPythonApplication rec {
    pname = "ufbt";
    version = "0.2.6";
    format = "wheel";

    src = pkgs.fetchurl {
      url = "https://files.pythonhosted.org/packages/b0/d3/54c1427c9f4473f33e4b4b54ce0c0854ac96de57b6e059a23a9ba3a7dff6/ufbt-0.2.6-py3-none-any.whl";
      hash = "sha256-7jtXiLQDRpSupZT9RHd+q0BJlo2UxfOa2T34sF2MiEk=";
    };

    propagatedBuildInputs = [ pkgs.python3Packages.oslex ];

    doCheck = false;
  };

  # `nix-shell --run` writes the command into its temporary rcfile and starts a
  # shell. The FHS shellHook replaces that shell with bwrap, so the command
  # would otherwise be lost. Recover it from the rcfile and run it inside the
  # FHS; without a command, start an interactive shell as usual.
  runScript = pkgs.writeShellScript "ducobox-fhs-run" ''
    if [ -n "''${DUCOBOX_NIX_SHELL_RUN:-}" ]; then
      exec bash -c "$DUCOBOX_NIX_SHELL_RUN"
    fi
    exec bash
  '';
in
(pkgs.buildFHSEnv {
  name = "ducobox";

  targetPkgs = p: [
    ufbt
    p.bash
    p.cacert
    p.coreutils
    p.curl
    p.findutils
    p.git
    p.gnumake
    p.gnugrep
    p.gnused
    p.gnutar
    p.gzip
    p.python3
    p.unzip
    p.wget
    p.zip
    p.zlib
  ];

  runScript = "${runScript}";

  extraPreBwrapCmds = ''
    # nix-shell runs its rcfile as $0; --run appends the command after the
    # `shopt -s execfail;` setup. Strip that prefix and the trailing `exit`.
    if [ -f "$0" ]; then
      run_cmd=$( ${pkgs.gnused}/bin/sed -n '/shopt -s execfail;/,$p' "$0" )
      run_cmd=''${run_cmd#*shopt -s execfail;}
      run_cmd=''${run_cmd%$'\n'exit}
      if [ -n "$run_cmd" ]; then
        export DUCOBOX_NIX_SHELL_RUN="$run_cmd"
      fi
    fi
  '';
}).env
