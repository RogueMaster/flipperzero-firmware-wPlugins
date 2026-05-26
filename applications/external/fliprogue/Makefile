.PHONY: all build tests package clean ufbt-build

PYTHON ?= python3

all: package

tests:
	$(PYTHON) build.py test

build: ufbt-build

ufbt-build:
	$(PYTHON) -m ufbt build

package:
	$(PYTHON) build.py package

clean:
	$(PYTHON) build.py clean
