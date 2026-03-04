all: gitsub asn1 build

gitsub:
	git submodule update --init --recursive

asn1:
	asn1c -S ./lib/asn1_skeletons -D ./lib/asn1 -no-gen-example -no-gen-OER -no-gen-PER -pdu=all seader.asn1

build:
	ufbt

launch:
	ufbt launch

format:
	ufbt format

clean:
	rm -rf dist
