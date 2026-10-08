// Differential test: our C core against RPCS3's own Infinity code (extracted verbatim).
// Crypto is cross-checked against OpenSSL.
#include "ref_shim.hpp"

#include "ref_generated.inc"

#include <new>
#include <random>

extern "C" {
#include "inf_crypto.h"
#include "infinity_core.h"
#include "infinity_figures.h"
}

static std::mt19937_64 rng(0xC0FFEE);
static u8 rb() { return u8(rng()); }

#define CHECK(cond)                                                      \
	do                                                                   \
	{                                                                    \
		if (!(cond))                                                     \
		{                                                                \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
			std::exit(1);                                                \
		}                                                                \
	} while (0)

static void test_crypto()
{
	for (int len = 0; len <= 300; len++)
	{
		std::vector<u8> data(len);
		for (auto& b : data) b = rb();
		u8 expect[20];
		SHA1(data.data(), data.size(), expect);

		InfSha1 ctx;
		inf_sha1_init(&ctx);
		size_t pos = 0;
		while (pos < data.size())
		{
			size_t n = 1 + rng() % 70;
			if (n > data.size() - pos) n = data.size() - pos;
			inf_sha1_update(&ctx, data.data() + pos, n);
			pos += n;
		}
		u8 got[20];
		inf_sha1_final(&ctx, got);
		CHECK(std::memcmp(expect, got, 20) == 0);
	}

	for (int i = 0; i < 3000; i++)
	{
		u8 key[16], in[16], expect[16], got[16], back[16];
		for (auto& b : key) b = rb();
		for (auto& b : in) b = rb();

		AES_KEY enc, dec;
		AES_set_encrypt_key(key, 128, &enc);
		AES_set_decrypt_key(key, 128, &dec);
		AES_ecb_encrypt(in, expect, &enc, AES_ENCRYPT);

		InfAes128 ours;
		inf_aes128_set_key(&ours, key);
		inf_aes128_encrypt_block(&ours, in, got);
		CHECK(std::memcmp(expect, got, 16) == 0);
		inf_aes128_decrypt_block(&ours, expect, back);
		CHECK(std::memcmp(in, back, 16) == 0);

		AES_ecb_encrypt(expect, back, &dec, AES_DECRYPT);
		CHECK(std::memcmp(in, back, 16) == 0);
	}
	std::puts("crypto ok");
}

static void test_figures()
{
	std::printf("figure table: %zu entries\n", inf_figure_table_count);
	CHECK(inf_figure_table_count > 300);
	const InfFigureInfo* mr = inf_figure_lookup(0x0F4241);
	CHECK(mr && std::strcmp(mr->name, "Mr. Incredible") == 0);
	CHECK(inf_figure_lookup(0xFFFFFF) == nullptr);

	for (size_t i = 0; i < inf_figure_table_count; i++)
	{
		const InfFigureInfo& info = inf_figure_table[i];
		u8 uid[7];
		g_ref_rand_values.clear();
		for (int k = 0; k < 7; k++)
		{
			uid[k] = u8(rng() % 255);
			g_ref_rand_values.push_back(uid[k]);
		}
		g_ref_rand_pos = 0;

		std::array<u8, 0x14 * 0x10> expect{};
		CHECK(ref_create_blank(info.id, info.series, expect));

		u8 got[INF_FIGURE_SIZE];
		CHECK(inf_figure_create_blank(got, info.id, info.series, uid));
		CHECK(std::memcmp(expect.data(), got, INF_FIGURE_SIZE) == 0);
		CHECK(inf_figure_decode_number(got) == info.id);
	}

	// A figure number the format cannot hold is refused by both.
	u8 uid[7] = {1, 2, 3, 4, 5, 6, 7};
	g_ref_rand_values.assign({1, 2, 3, 4, 5, 6, 7});
	g_ref_rand_pos = 0;
	std::array<u8, 0x14 * 0x10> unused{};
	CHECK(!ref_create_blank(0x001234, 3, unused));
	u8 out[INF_FIGURE_SIZE];
	CHECK(!inf_figure_create_blank(out, 0x001234, 3, uid));
	std::puts("figures ok");
}

static void reset_ref()
{
	g_infinitybase.~infinity_base();
	new (&g_infinitybase) infinity_base();
}

static void ref_poll(std::queue<std::array<u8, 32>>& queries, u8* out)
{
	std::memset(out, 0, 32);
	std::optional<std::array<u8, 32>> ev = g_infinitybase.pop_added_removed_response();
	if (ev)
	{
		std::memcpy(out, ev->data(), 32);
	}
	else if (!queries.empty())
	{
		std::memcpy(out, queries.front().data(), 32);
		queries.pop();
	}
}

static const u8 COMMANDS[] = {0x80, 0x81, 0x83, 0x90, 0x92, 0x93, 0x95, 0x96, 0xA1, 0xA1, 0xA2, 0xA2,
	0xA2, 0xA3, 0xB4, 0xB4, 0xB5, 0x00, 0x42, 0xFF};

static void test_protocol(int scenarios, int steps)
{
	for (int s = 0; s < scenarios; s++)
	{
		reset_ref();
		InfBase mine;
		inf_base_init(&mine);
		std::queue<std::array<u8, 32>> queries;
		int outstanding = 0;

		for (int step = 0; step < steps; step++)
		{
			const unsigned r = unsigned(rng() % 100);
			if (r < 55)
			{
				u8 buf[32];
				for (auto& b : buf) b = rb();
				buf[0] = 0xFF;
				buf[2] = COMMANDS[rng() % sizeof(COMMANDS)];
				const u8 orders[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 255};
				buf[4] = (buf[2] == 0xA2 || buf[2] == 0xA3 || buf[2] == 0xB4) ? orders[rng() % sizeof(orders)] : buf[4];
				if (buf[2] == 0xA2 || buf[2] == 0xA3) buf[5] = u8(rng() % 8);

				u8 ref_buf[32];
				std::memcpy(ref_buf, buf, 32);
				queries.push(ref_process(ref_buf));
				inf_base_handle_out(&mine, buf);
				outstanding++;
			}
			else if (r < 75)
			{
				std::array<u8, 0x14 * 0x10> data{};
				for (auto& b : data) b = rb();
				const u8 slot = u8(rng() % 9);
				g_infinitybase.load_figure(data, fs::file(true), slot);
				CHECK(inf_base_load_figure(&mine, slot, data.data()));
				outstanding++;
			}
			else if (r < 85)
			{
				const u8 slot = u8(rng() % 9);
				const bool a = g_infinitybase.remove_figure(slot);
				const bool b = inf_base_remove_figure(&mine, slot);
				CHECK(a == b);
				if (a) outstanding++;
			}

			int polls = int(rng() % 3);
			if (outstanding > 10) polls = 4;
			for (int p = 0; p < polls; p++)
			{
				u8 expect[32], got[32] = {0};
				ref_poll(queries, expect);
				inf_base_next_in(&mine, got);
				if (std::memcmp(expect, got, 32) != 0)
				{
					std::printf("MISMATCH scenario %d step %d poll %d\n", s, step, p);
					for (int i = 0; i < 32; i++) std::printf("%02X", expect[i]);
					std::printf("\n");
					for (int i = 0; i < 32; i++) std::printf("%02X", got[i]);
					std::printf("\n");
					std::exit(1);
				}
				if (outstanding > 0) outstanding--;
			}
		}

		// Drain whatever is left and compare.
		while (outstanding > 0)
		{
			u8 expect[32], got[32] = {0};
			ref_poll(queries, expect);
			inf_base_next_in(&mine, got);
			CHECK(std::memcmp(expect, got, 32) == 0);
			outstanding--;
		}
		CHECK(mine.dropped_packets == 0);
	}
	std::printf("protocol: %d scenarios x %d steps\n", scenarios, steps);
}

// A realistic conversation: handshake, then place a figure and read it back.
static void test_scripted()
{
	reset_ref();
	InfBase mine;
	inf_base_init(&mine);

	u8 uid[7] = {9, 8, 7, 6, 5, 4, 3};
	u8 fig[INF_FIGURE_SIZE];
	CHECK(inf_figure_create_blank(fig, 0x3D0900, 3, uid));
	std::array<u8, 0x14 * 0x10> ref_fig{};
	std::memcpy(ref_fig.data(), fig, INF_FIGURE_SIZE);
	g_infinitybase.load_figure(ref_fig, fs::file(true), 3);
	CHECK(inf_base_load_figure(&mine, 3, fig));

	std::queue<std::array<u8, 32>> queries;
	auto exchange = [&](u8 cmd, u8 seq, u8 a, u8 b)
	{
		u8 buf[32] = {0xFF, 0x00, cmd, seq, a, b};
		u8 ref_buf[32];
		std::memcpy(ref_buf, buf, 32);
		queries.push(ref_process(ref_buf));
		inf_base_handle_out(&mine, buf);
		for (int i = 0; i < 2; i++)
		{
			u8 expect[32], got[32] = {0};
			ref_poll(queries, expect);
			inf_base_next_in(&mine, got);
			CHECK(std::memcmp(expect, got, 32) == 0);
		}
	};
	exchange(0x80, 1, 0, 0);
	exchange(0x81, 2, 0x12, 0x34);
	exchange(0x83, 3, 0, 0);
	exchange(0xA1, 4, 0, 0);
	exchange(0xB4, 5, 0, 0);
	exchange(0xA2, 6, 0, 0);
	exchange(0xA2, 7, 0, 1);
	CHECK(mine.handshake_seen && mine.commands_seen == 7);
	std::puts("scripted ok");
}

int main()
{
	test_crypto();
	test_figures();
	test_scripted();
	test_protocol(3000, 300);
	std::puts("ALL TESTS PASSED");
	return 0;
}
