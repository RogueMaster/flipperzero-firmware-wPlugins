.SUFFIXES:

.DEFAULT_GOAL := app


.PHONY: app
app:
	ufbt

.PHONY: clean
clean:
	ufbt -c

.PHONY: switch-release
ch-release:
	ufbt update --channel=release

.PHONY: ch-dev
ch-dev:
	ufbt update --channel=dev


.PHONY: launch
launch:
	ufbt launch


ALL_RAW_IMAGES := \
  asset_fuji_page_num_25x7_1.png \
  asset_fuji_page_num_25x7_2.png \
  asset_fuji_page_num_25x7_3.png \
  asset_fuji_fan_Icon_15x15.png \
  asset_fuji_fan_power_bars_22x15_1.png \
  asset_fuji_fan_power_bars_22x15_2.png \
  asset_fuji_fan_power_bars_22x15_3.png \
  asset_fuji_fan_power_bars_22x15_4.png \
  asset_fuji_fan_power_bars_22x15_auto.png \
  asset_fuji_auto_swing_icon_15x15.png \
  asset_fuji_degree_icon_6x6.png \
  asset_fuji_clock_icon_15x15.png \
  asset_fuji_buttons_page2_20x18_01_fan.png \
  asset_fuji_buttons_page2_20x18_03_swing.png \
  asset_fuji_buttons_page2_20x18_05_set.png \
  asset_fuji_buttons_page2_20x18_07_eco.png \
  asset_fuji_buttons_page2_20x18_09_mod.png \
  asset_fuji_page3_blank_button_34x19.png \

ALL_IMAGES := 

define DEFINE_IMAGE_RULE =
ALL_IMAGES += images/$(1)
images/$(1): raw_images/$(1)
	convert $$^ -background white -alpha remove -alpha off $$@
endef

$(foreach img,$(ALL_RAW_IMAGES),$(eval $(call DEFINE_IMAGE_RULE,$(img))))

.PHONY: images
images: $(ALL_IMAGES)
