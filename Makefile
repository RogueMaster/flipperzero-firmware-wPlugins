.PHONY: build install standard clean

# Build for Unleashed firmware (default)
build:
	@bash tools/build.sh

# Build for standard Flipper firmware
standard:
	@bash tools/build.sh -f standard

# Build and flash to connected Flipper via USB
install:
	@bash tools/build.sh -i

clean:
	@rm -f dist/*.fap
