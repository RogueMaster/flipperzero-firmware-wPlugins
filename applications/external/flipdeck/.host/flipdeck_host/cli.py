"""FlipDeck CLI – main entry point."""

import argparse
import os
import sys


def get_config_dir():
    """Get platform-appropriate config directory."""
    if sys.platform == "darwin":
        return os.path.expanduser("~/Library/Application Support/FlipDeck")
    elif sys.platform == "linux":
        return os.path.expanduser("~/.config/flipdeck")
    return os.path.expanduser("~/.flipdeck")


def ensure_config(config_dir):
    """Create config dir and default files if missing."""
    os.makedirs(config_dir, exist_ok=True)
    actions_file = os.path.join(config_dir, "actions.yaml")
    plugins_dir = os.path.join(config_dir, "plugins")
    os.makedirs(plugins_dir, exist_ok=True)

    if not os.path.exists(actions_file):
        from .actions import write_default_actions

        write_default_actions(actions_file)
        print(f"  Created default config: {actions_file}")

    return actions_file


def cmd_start(args):
    """Start the FlipDeck daemon (foreground)."""
    from .daemon import run_daemon

    config_dir = get_config_dir()
    actions_file = ensure_config(config_dir)
    run_daemon(
        actions_file=actions_file,
        plugins_dir=os.path.join(config_dir, "plugins"),
        web_port=args.port if args.web else None,
    )


def cmd_web(args):
    """Start only the web UI."""
    from .web import run_web

    config_dir = get_config_dir()
    actions_file = ensure_config(config_dir)
    run_web(
        actions_file=actions_file,
        plugins_dir=os.path.join(config_dir, "plugins"),
        port=args.port,
    )


def cmd_upload(args):
    """Upload config to Flipper."""
    from .flipper import upload_config

    upload_config(args.config, args.port)


def cmd_actions(args):
    """List available actions."""
    from .actions import list_all_actions

    config_dir = get_config_dir()
    ensure_config(config_dir)
    list_all_actions(
        os.path.join(config_dir, "actions.yaml"),
        os.path.join(config_dir, "plugins"),
    )


def cmd_install(args):
    from .service import install_service

    install_service()


def cmd_uninstall(args):
    from .service import uninstall_service

    uninstall_service()


def cmd_stop(args):
    from .service import stop_service

    stop_service()


def cmd_start_service(args):
    from .service import start_service

    start_service()


def cmd_status(args):
    from .service import service_status

    service_status()


def cmd_logs(args):
    from .service import show_logs

    show_logs()


def cmd_config_dir(args):
    print(get_config_dir())


def main():
    p = argparse.ArgumentParser(
        prog="flipdeck",
        description="FlipDeck Host - USB HID macro pad companion for Flipper Zero",
        epilog="Run 'flipdeck <command> --help' for details on each command.",
    )
    p.add_argument("--version", action="version", version="%(prog)s 3.0.0")
    sub = p.add_subparsers(dest="command")

    # Foreground
    s = sub.add_parser("start", help="Start daemon in foreground (Ctrl+C to stop)")
    s.add_argument("--web", action="store_true", help="Enable web UI")
    s.add_argument("--port", type=int, default=7433, help="Web UI port (default 7433)")
    s.set_defaults(func=cmd_start)

    w = sub.add_parser("web", help="Start web UI only (no key listener)")
    w.add_argument("--port", type=int, default=7433, help="Port (default 7433)")
    w.set_defaults(func=cmd_web)

    # Service management
    sub.add_parser(
        "install", help="Install as background service (autostart at login)"
    ).set_defaults(func=cmd_install)
    sub.add_parser(
        "uninstall", help="Remove background service completely"
    ).set_defaults(func=cmd_uninstall)
    sub.add_parser("stop", help="Stop background service").set_defaults(func=cmd_stop)
    sub.add_parser("restart", help="Restart background service").set_defaults(
        func=cmd_start_service
    )
    sub.add_parser(
        "status", help="Show service status, config paths, web UI URL"
    ).set_defaults(func=cmd_status)
    sub.add_parser("logs", help="Show service logs (last 50 lines)").set_defaults(
        func=cmd_logs
    )

    # Config & tools
    sub.add_parser("actions", help="List all configured actions").set_defaults(
        func=cmd_actions
    )
    u = sub.add_parser("upload", help="Upload config.txt to Flipper SD card via USB")
    u.add_argument("config", help="Path to config.txt file")
    u.add_argument("--port", default=None, help="Serial port (auto-detect if omitted)")
    u.set_defaults(func=cmd_upload)
    sub.add_parser("config-dir", help="Print config directory path").set_defaults(
        func=cmd_config_dir
    )

    args = p.parse_args()
    if not args.command:
        p.print_help()
        print("\nQuick start:")
        print("  flipdeck start --web     Start in foreground with web UI")
        print("  flipdeck install         Install as background service")
        print("  flipdeck status          Check if service is running")
        print("  flipdeck stop            Stop background service")
        print("  flipdeck uninstall       Remove background service")
    else:
        args.func(args)


if __name__ == "__main__":
    main()
