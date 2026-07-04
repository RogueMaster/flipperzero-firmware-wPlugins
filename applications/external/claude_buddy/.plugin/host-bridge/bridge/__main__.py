"""Entry point: python -m bridge  [--transport usb|ble]"""

import argparse
import asyncio
import logging
import os
import signal
import sys

from . import config
from .daemon import Daemon


def _make_transport(name: str):
    if name == "ble":
        from .transport_bt import BtTransport

        return BtTransport()
    if name == "usb":
        from .transport_usb import UsbTransport

        return UsbTransport()
    # "auto": try USB first, fall back to BLE
    from .transport_auto import AutoTransport

    return AutoTransport()


def main():
    if sys.version_info < (3, 10):
        sys.exit(
            f"flipper-claude-buddy requires Python 3.10 or higher. "
            f"You are running Python {sys.version.split()[0]}. "
            f"Please upgrade Python, then reinstall the plugin:\n"
            f"  claude plugin uninstall flipper-claude-buddy\n"
            f"  claude plugin install flipper-claude-buddy@flipper-claude-buddy"
        )

    parser = argparse.ArgumentParser(description="Flipper Claude Buddy bridge")
    parser.add_argument(
        "--transport",
        choices=["auto", "usb", "ble"],
        default=config.TRANSPORT,
        help="Communication transport (default: %(default)s)",
    )
    parser.add_argument(
        "--log-level",
        choices=["debug", "info", "warning", "error"],
        default=os.environ.get("FLIPPER_LOG_LEVEL", "info"),
        help="Log level (default: info, env: FLIPPER_LOG_LEVEL)",
    )
    args = parser.parse_args()

    logging.basicConfig(
        level=getattr(logging, args.log_level.upper()),
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )

    transport = _make_transport(args.transport)
    daemon = Daemon(transport)

    async def _run():
        loop = asyncio.get_running_loop()
        stop_event = asyncio.Event()
        for sig in (signal.SIGINT, signal.SIGTERM):
            loop.add_signal_handler(sig, stop_event.set)

        daemon_task = asyncio.create_task(daemon.run())
        stopper = asyncio.create_task(stop_event.wait())
        # Wait for either the daemon to exit on its own or a shutdown
        # signal.  On signal, cancel the daemon task so its `finally`
        # block can run a clean BLE disconnect — loop.stop() would skip
        # that cleanup entirely.
        done, _ = await asyncio.wait(
            {daemon_task, stopper}, return_when=asyncio.FIRST_COMPLETED
        )
        if daemon_task not in done:
            daemon_task.cancel()
            try:
                await daemon_task
            except asyncio.CancelledError:
                pass
        stopper.cancel()

    try:
        asyncio.run(_run())
    except KeyboardInterrupt:
        pass
    finally:
        logging.info("Bridge stopped.")


if __name__ == "__main__":
    main()
