UFBT     := $(HOME)/.local/bin/ufbt
FAP_SRC  := $(HOME)/.ufbt/build/polk_magnifimini_remote.fap
FAP_DST  := build/polk_magnifimini_remote.fap

.PHONY: all build clean launch

all: build

build:
	$(UFBT) build
	mv $(FAP_SRC) $(FAP_DST)
	@echo "Build is saved in → $(CURDIR)/$(FAP_DST)"

launch:
	$(UFBT) launch

clean:
	$(UFBT) clean
	rm -f $(FAP_DST)
