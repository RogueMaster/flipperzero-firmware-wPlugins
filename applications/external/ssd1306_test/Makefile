.PHONY: build launch clean setup update vscode lint format

# Build the .fap file
build:
	ufbt

# Build and deploy to a connected Flipper Zero via USB
launch:
	ufbt launch

# Clean build artifacts
clean:
	ufbt -c

# First-time setup: install ufbt and pull Momentum SDK
setup:
	pip3 install ufbt
	ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json

# Update the Momentum SDK to latest
update:
	ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json

# Regenerate VSCode intellisense config
vscode:
	ufbt vscode_dist

# Lint the source
lint:
	ufbt lint

# Auto-format the source
format:
	ufbt format
