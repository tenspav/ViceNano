APP_NAME := cube
SRCS := cube.c
GL_SURFACE := 1
NANOAPPS ?= /home/pavliukov/ipodwork/nano7-untethered/nanoapps
# The SDK assigns this with :=; override before inclusion also selects libgcc.
override HB_ARCH_FLAGS := -mcpu=cortex-a5 -mthumb -mfloat-abi=soft -mfpu=neon
CHECK_NO_THUMB_FP := 1
.DEFAULT_GOAL := all
include $(NANOAPPS)/sdk/hb_app.mk

SDK_SRCS := $(filter-out $(SDK_DIR)/gl.c,$(SDK_SRCS)) ../sdk/gl.c

.PHONY: package
all: cube.hbapp
cube.hbapp: $(APP_OUT)
	cp $< $@
# Default GNU script adds a 64 KiB text/data gap; HRL1 needs a compact image.
RELOC_LDFLAGS += -Wl,-n
RELOC_LDFLAGS += -Wl,-T,../sdk/hbapp.ld

$(BUILD)/$(APP_NAME).reloc.elf: ../sdk/gl.c ../sdk/hbapp.ld
