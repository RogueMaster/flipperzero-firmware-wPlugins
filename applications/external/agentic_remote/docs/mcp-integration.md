# MCP Integration Guide

Claupper + MCP (Model Context Protocol) turns your Flipper Zero into a universal AI remote. Press 1/2/3 on the Flipper, and Claude Code dispatches actions to any MCP server.

## Architecture

```
Flipper Zero (5 buttons)
  → USB/BLE HID →
    Claude Code (with /claupper skill)
      → MCP servers →
        [Smart Home, DevOps, Music, Messaging, etc.]
```

The `/claupper` skill formats every MCP action as 3 numbered choices. You press 1, 2, or 3. Claude handles the rest.

## Smart Home Control

### Home Assistant

Install the [Home Assistant MCP server](https://www.home-assistant.io/integrations/mcp_server/), then ask Claude to control your devices.

Example interaction:
```
You: "lights"
Claude:
  1. Turn on living room lights
  2. Set all lights to 50%
  3. More rooms...
```

### Philips Hue

Use the [Hue MCP server](https://github.com/rmrfslashbin/hue-mcp) for direct Hue bulb control — colors, brightness, effects.

### Multi-Brand

The [Smart Home Orchestrator MCP](https://github.com/modelcontextprotocol/servers) supports Hue, Nest, Ring, Sonos, SmartThings, Kasa, Arlo, Ecobee, LIFX, and August from one server.

## Developer Tools

### GitHub

With the [GitHub MCP server](https://github.com/modelcontextprotocol/servers):
```
Claude:
  1. Merge PR #42 (all checks pass)
  2. View PR diff first
  3. Request changes
```

### CI/CD & Deployment

Use [Desktop Commander MCP](https://github.com/wonderwhy-er/DesktopCommanderMCP) or [SSH MCP](https://github.com/tufantunc/ssh-mcp) to manage deployments:
```
Claude:
  1. Deploy to staging
  2. Check current deployment status
  3. Rollback last deploy
```

### Jira / Linear

Manage sprints and issues from your remote:
```
Claude:
  1. Move ticket to "In Progress"
  2. Create new bug ticket
  3. View sprint backlog
```

## Media & Communication

### Spotify

With [Spotify MCP](https://github.com/marcelmarais/spotify-mcp-server):
```
Claude:
  1. Play/Pause
  2. Skip track
  3. Change playlist
```

### Slack / Discord

Post status updates or read messages:
```
Claude:
  1. Send "deploying now" to #engineering
  2. Read latest messages
  3. Different channel
```

## Setting Up MCP Modes

To switch between different MCP contexts, use macros:

```
# macros.txt
/claupper code
/claupper home
/claupper music
/claupper devops
```

Each macro tells Claude which MCP servers to prioritize. The `/claupper` skill adapts its option format to match the active context.

## Adding a New MCP Server

1. Install the MCP server (see [MCP server directory](https://github.com/modelcontextprotocol/servers))
2. Configure it in Claude Code's MCP settings
3. The `/claupper` skill automatically formats its tools as 1/2/3 choices
4. Add a macro to quickly switch to that server's context

## Tips

- MCP servers run locally — no cloud dependency for smart home control
- Combine multiple MCP servers for compound actions ("deploy and notify Slack")
- Voice dictation (Down button) works great for MCP: "turn off the bedroom lights"
- The Claupper skill batches MCP calls when possible to reduce button presses
