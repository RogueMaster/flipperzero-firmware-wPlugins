.PHONY: build install standard clean

# Build for Momentum firmware (mntm-012, API 87.1) using ufbt
build:
	ufbt fap_co2_app
	@mkdir -p dist
	@cp /Users/ivatikhonov/.ufbt/build/co2_app.fap dist/co2_app_mntm_87.1.fap
	@echo "Built: dist/co2_app_mntm_87.1.fap"

# Build for Unleashed firmware (legacy)
unleashed:
	@bash tools/build.sh

# Build for standard Flipper firmware
standard:
	@bash tools/build.sh -f standard

clean:
	@rm -f dist/*.fap
