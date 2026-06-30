all: gitsub asn1 build

gitsub:
	git submodule update --init --recursive

asn1:
	asn1c -S ./lib/asn1_skeletons -D ./lib/asn1 -no-gen-example -no-gen-OER -no-gen-PER -pdu=all seader.asn1

build:
	ufbt

HOST_TEST_CFLAGS = -std=c11 -Wall -Wextra -Werror -DSEADER_HOST_TEST -Ilib/host_tests/vendor/munit -Ilib/host_tests -Ilib/asn1 -I.
ASN1_TEST_CFLAGS = -std=c11 -Wall -Wextra -Werror -Wno-error=unused-function -Wno-error=unused-parameter -DASN_DISABLE_PER_SUPPORT -DASN_DISABLE_OER_SUPPORT -DASN_DISABLE_XER_SUPPORT -DASN_DISABLE_RANDOM_FILL -Ilib/host_tests/vendor/munit -Ilib/host_tests -Ilib/asn1 -Ilib/asn1_skeletons -I.

test-host:
	mkdir -p build/host_tests
	cc $(HOST_TEST_CFLAGS) \
		lib/host_tests/vendor/munit/munit.c \
		lib/host_tests/test_main.c \
			lib/host_tests/test_lrc.c \
			lib/host_tests/test_board_power_lifecycle.c \
			lib/host_tests/test_hf_read_lifecycle.c \
			lib/host_tests/test_sam_startup_ui.c \
			lib/host_tests/test_sam_key_label.c \
		lib/host_tests/test_ccid_logic.c \
		lib/host_tests/test_t1_existing.c \
		lib/host_tests/test_t1_protocol.c \
		lib/host_tests/test_snmp.c \
			lib/host_tests/test_uhf_status_label.c \
			lib/host_tests/test_credential_sio_label.c \
			lib/host_tests/test_hf_read_plan.c \
			lib/host_tests/test_wiegand_plugin.c \
			lib/host_tests/test_runtime_policy.c \
		lib/host_tests/t1_test_stubs.c \
		lib/host_tests/bit_buffer_mock.c \
			lrc.c \
			board_power_lifecycle.c \
			sam_startup_ui.c \
			ccid_logic.c \
		credential_sio_label.c \
		t_1_logic.c \
		t_1.c \
		sam_key_label.c \
		snmp_ber_view.c \
		snmp_codec.c \
		snmp_response_view.c \
			uhf_status_label.c \
			uhf_tag_config_view.c \
			uhf_snmp_probe.c \
			hf_read_lifecycle.c \
			seader_hf_read_plan.c \
			wiegand_interface_fal/wiegand.c \
		runtime_policy.c \
		-o build/host_tests/seader_tests
	./build/host_tests/seader_tests

test-asn1-integration:
	mkdir -p build/host_tests
	cc $(ASN1_TEST_CFLAGS) \
		lib/host_tests/vendor/munit/munit.c \
		lib/host_tests/test_card_details_main.c \
		lib/host_tests/test_card_details_builder.c \
		card_details_builder.c \
		lib/asn1/CardDetails.c \
		lib/asn1/Protocol.c \
		lib/asn1/FrameProtocol.c \
		lib/asn1/RunTimerValue.c \
		lib/asn1_skeletons/OCTET_STRING.c \
		lib/asn1_skeletons/BOOLEAN.c \
		lib/asn1_skeletons/NativeInteger.c \
		lib/asn1_skeletons/NativeEnumerated.c \
		lib/asn1_skeletons/INTEGER.c \
		lib/asn1_skeletons/OPEN_TYPE.c \
		lib/asn1_skeletons/constr_CHOICE.c \
		lib/asn1_skeletons/constr_SEQUENCE.c \
		lib/asn1_skeletons/constr_TYPE.c \
		lib/asn1_skeletons/asn_application.c \
		lib/asn1_skeletons/asn_codecs_prim.c \
		lib/asn1_skeletons/ber_tlv_tag.c \
		lib/asn1_skeletons/ber_tlv_length.c \
		lib/asn1_skeletons/ber_decoder.c \
		lib/asn1_skeletons/der_encoder.c \
		lib/asn1_skeletons/constraints.c \
		lib/asn1_skeletons/asn_internal.c \
		-o build/host_tests/seader_card_details_tests
	./build/host_tests/seader_card_details_tests

test-runtime-integration:
	mkdir -p build/host_tests
	cc $(HOST_TEST_CFLAGS) \
		lib/host_tests/vendor/munit/munit.c \
		lib/host_tests/test_runtime_integration_main.c \
		lib/host_tests/test_hf_release_sequence.c \
		hf_release_sequence.c \
		-o build/host_tests/seader_runtime_integration_tests
	./build/host_tests/seader_runtime_integration_tests

launch:
	ufbt launch

format:
	ufbt format

clean:
	rm -rf dist
	rm -rf build/host_tests
