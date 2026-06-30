.PHONY: update
update:
	git submodule update --remote lib/micropython && git add lib/micropython

.PHONY: build-fap
build-fap: update
	ufbt build

.PHONY: launch
launch: build-fap
	ufbt launch

.PHONY: clean
clean:
	ufbt -c

.PHONY: build-pages
build-pages:
	rm -rf ./dist/pages ./flipperzero/__init__.py
	cat ./flipperzero/_*.py > ./flipperzero/__init__.py
	source venv/bin/activate && sphinx-build docs/pages dist/pages

.PHONY: publish-pages
publish-pages: build-pages
	./publish.sh pages

.PHONY: build-python
build-python:
	source venv/bin/activate && hatch build

.PHONY: publish-python
publish-python: build-python
	source venv/bin/activate && hatch publish dist/python/*
