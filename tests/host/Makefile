# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
# Builds every test_<x>.c against ../../main/<x>.c (plus EXTRA_<x>) and runs it. test_reference.cpp
# links the verbatim rc-switch 2.6.4 and sketch extract (ref/) against the firmware's pure RF modules.
#
#   make           public mode: tracked fixtures, main/rf_codes_example.h, device NOVY-RF
#   make local     your own mode, from ../../.local (git-ignored): local.mk (DEVICE, NOVY_CONFIG,
#                  NOVY_SKETCH, PUBLIC_MAP), fixtures/ (your captures) and rf_codes.h; also checks that
#                  the fixtures match your Arduino sources and that the public fixtures are up to date
CC       ?= cc
CXX      ?= c++
CFLAGS   ?= -std=c11 -Wall -Wextra -Werror -g
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -g
MAIN     := ../../main
LOCAL    := ../../.local
IDF_PATH ?= $(HOME)/.espressif/v5.5.3/esp-idf
CJSON    := $(IDF_PATH)/components/json/cJSON
PY       := /usr/bin/python3
MODE     ?= public

ifeq ($(MODE),local)
include $(LOCAL)/local.mk
FIXTURES := $(LOCAL)/fixtures
RF_DEF   := -DRF_CODES_H=\"$(abspath $(LOCAL)/rf_codes.h)\"
else
DEVICE   := NOVY-RF
FIXTURES := fixtures
RF_DEF   :=
endif
OUT      := out/$(MODE)
DEFS     := -DTEST_DEVICE=\"$(DEVICE)\" -DFIXTURES=\"$(FIXTURES)\"

CTESTS   := $(patsubst %.c,%,$(wildcard test_*.c))
REF_C    := rcswitch rf_wave rf_cmd devstate

EXTRA_rf_cmd          := $(MAIN)/rcswitch.c $(MAIN)/rf_wave.c
EXTRA_devstate        := $(MAIN)/rf_cmd.c $(MAIN)/rcswitch.c
EXTRA_discovery       := $(MAIN)/rf_cmd.c $(MAIN)/rcswitch.c $(CJSON)/cJSON.c
EXTRA_led_logic       := $(MAIN)/light.c
EXTRA_config_validate := $(MAIN)/net_config.c
FLAGS_discovery       := -I$(CJSON)

# When the CommandLineTools SDK is newer than its linker ("unknown architecture arm64e..."),
# link against the Xcode SDK instead.
XCODE_SDK := /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
ifneq ($(wildcard $(XCODE_SDK)),)
export SDKROOT ?= $(XCODE_SDK)
endif

all: test

$(OUT) $(OUT)/obj:
	mkdir -p $@

.SECONDEXPANSION:
$(OUT)/test_%: test_%.c $(MAIN)/%.c check.h $$(EXTRA_$$*) | $(OUT)
	$(CC) $(CFLAGS) $(DEFS) $(RF_DEF) $(FLAGS_$*) -I$(MAIN) -o $@ $< $(MAIN)/$*.c $(EXTRA_$*) -lm

$(OUT)/obj/%.o: $(MAIN)/%.c $(wildcard $(MAIN)/*.h) | $(OUT)/obj
	$(CC) $(CFLAGS) $(RF_DEF) -I$(MAIN) -c -o $@ $<

# Not obj/RCSwitch.o: on a case-insensitive file system that is obj/rcswitch.o (firmware module).
$(OUT)/obj/ref_rcswitch_lib.o: ref/RCSwitch.cpp ref/RCSwitch.h ref/Arduino.h | $(OUT)/obj
	$(CXX) -std=c++17 -g -w -DARDUINO=100 -DESP32 -Iref -c -o $@ $<

# config_rf.h and sketch_ref.inc come from $(FIXTURES) first, then ref/.
$(OUT)/test_reference: test_reference.cpp ref/Arduino.h check.h $(OUT)/obj/ref_rcswitch_lib.o $(REF_C:%=$(OUT)/obj/%.o)
	$(CXX) $(CXXFLAGS) $(DEFS) -DARDUINO=100 -DESP32 -I$(FIXTURES) -Iref -I$(MAIN) -o $@ $< \
	    $(OUT)/obj/ref_rcswitch_lib.o $(REF_C:%=$(OUT)/obj/%.o)

TESTS := $(CTESTS:%=$(OUT)/%) $(OUT)/test_reference

test: $(TESTS)
	@set -e; for t in $(TESTS); do ./$$t; done

local:
	@test -f $(LOCAL)/local.mk || { echo "make local: $(LOCAL)/local.mk not found"; exit 1; }
	@$(MAKE) --no-print-directory MODE=local test check-sources check-public

# Your fixtures still match your Arduino sources (NOVY_CONFIG, NOVY_SKETCH from local.mk).
check-sources:
	@$(PY) ../../tools/extract_ref.py config "$(NOVY_CONFIG)" | diff -u $(FIXTURES)/config_rf.h -
	@$(PY) ../../tools/extract_ref.py sketch "$(NOVY_SKETCH)" | diff -u $(FIXTURES)/sketch_ref.inc -
	@echo "check-sources: local fixtures match the Arduino sources"

# The tracked public fixtures are what tools/make_public_fixtures.py makes of your fixtures.
check-public:
	@rm -rf $(OUT)/public && $(PY) ../../tools/make_public_fixtures.py --src $(FIXTURES) \
	    --fixtures $(OUT)/public/fixtures --ref $(OUT)/public/ref $(PUBLIC_MAP) >/dev/null
	@for f in config_rf.h discovery.txt state_topics.txt http_live.txt; do \
	    diff -u fixtures/$$f $(OUT)/public/fixtures/$$f || exit 1; done
	@diff -u ref/sketch_ref.inc $(OUT)/public/ref/sketch_ref.inc
	@echo "check-public: tracked public fixtures are up to date"

clean:
	rm -rf out obj test_*.dSYM
	rm -f $(CTESTS)

.PHONY: all test local check-sources check-public clean
