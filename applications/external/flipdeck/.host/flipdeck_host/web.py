"""FlipDeck Web UI – config editor, action manager, Flipper status."""

import json
import os
import threading
import yaml
from flask import Flask, request, jsonify, send_from_directory

from .actions import ActionExecutor, BUILTIN_HANDLERS, write_default_actions
from .flipper import find_flipper_port, upload_config_data, is_flipper_connected


def create_app(executor, actions_file, plugins_dir=None):
    """Create Flask app with all API routes."""

    static_dir = os.path.join(os.path.dirname(__file__), "static")
    app = Flask(__name__, static_folder=static_dir)
    app.config["EXECUTOR"] = executor
    app.config["ACTIONS_FILE"] = actions_file
    app.config["PLUGINS_DIR"] = plugins_dir

    # ── Static files ──────────────────────────────────────

    @app.route("/")
    def index():
        return send_from_directory(static_dir, "index.html")

    @app.route("/static/<path:filename>")
    def static_files(filename):
        return send_from_directory(static_dir, filename)

    # ── Actions API ───────────────────────────────────────

    @app.route("/api/actions", methods=["GET"])
    def get_actions():
        executor.reload()
        return jsonify(executor.get_all())

    @app.route("/api/actions/<key>", methods=["PUT"])
    def set_action(key):
        data = request.get_json()
        if not data:
            return jsonify({"error": "No data"}), 400
        executor.set_action(key.lower(), data)
        return jsonify({"ok": True, "key": key.lower()})

    @app.route("/api/actions/<key>", methods=["DELETE"])
    def delete_action(key):
        executor.delete_action(key.lower())
        return jsonify({"ok": True})

    @app.route("/api/actions/<key>/toggle", methods=["POST"])
    def toggle_action(key):
        actions = executor.get_all()
        if key.lower() in actions:
            act = actions[key.lower()]
            act["enabled"] = not act.get("enabled", True)
            executor.set_action(key.lower(), act)
            return jsonify({"ok": True, "enabled": act["enabled"]})
        return jsonify({"error": "Not found"}), 404

    @app.route("/api/actions/<key>/test", methods=["POST"])
    def test_action(key):
        result = executor.execute(key.lower())
        return jsonify({"ok": True, "result": result})

    # ── Action types ──────────────────────────────────────

    @app.route("/api/types", methods=["GET"])
    def get_types():
        types = {}
        for name in sorted(executor.handlers.keys()):
            builtin = name in BUILTIN_HANDLERS
            types[name] = {"builtin": builtin}
        return jsonify(types)

    # ── Flipper config ────────────────────────────────────

    @app.route("/api/flipper/status", methods=["GET"])
    def flipper_status():
        port = find_flipper_port()
        return jsonify(
            {
                "connected": port is not None,
                "port": port,
            }
        )

    @app.route("/api/flipper/config", methods=["GET"])
    def get_flipper_config():
        """Read current Flipper config from actions file to generate config.txt."""
        executor.reload()
        return jsonify(
            {
                "actions": executor.get_all(),
                "types": list(executor.handlers.keys()),
            }
        )

    @app.route("/api/flipper/upload", methods=["POST"])
    def upload_to_flipper():
        data = request.get_json()
        config_text = data.get("config", "")
        if not config_text:
            return jsonify({"error": "No config data"}), 400
        try:
            port = find_flipper_port()
            if not port:
                return jsonify({"error": "Flipper not connected"}), 404
            upload_config_data(config_text, port)
            return jsonify({"ok": True})
        except Exception as e:
            return jsonify({"error": str(e)}), 500

    # ── Reload ────────────────────────────────────────────

    @app.route("/api/reload", methods=["POST"])
    def reload_config():
        executor.reload()
        return jsonify({"ok": True, "count": len(executor.get_all())})

    return app


def run_web(actions_file, plugins_dir=None, port=7433):
    """Run the web UI as the main process."""
    executor = ActionExecutor(actions_file, plugins_dir)
    app = create_app(executor, actions_file, plugins_dir)
    print(f"  FlipDeck Web UI: http://localhost:{port}")
    app.run(host="127.0.0.1", port=port, debug=False)


def run_web_thread(
    executor, actions_file, plugins_dir=None, port=7433, stop_event=None
):
    """Run the web UI in a background thread."""
    app = create_app(executor, actions_file, plugins_dir)

    def _run():
        from werkzeug.serving import make_server

        srv = make_server("127.0.0.1", port, app)
        srv.timeout = 1
        while not (stop_event and stop_event.is_set()):
            srv.handle_request()

    t = threading.Thread(target=_run, daemon=True, name="flipdeck-web")
    t.start()
    return t
