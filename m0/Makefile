APP_NAME := tri
SRCS := tri.c
GL_SURFACE := 1
NANOAPPS ?= /home/pavliukov/ipodwork/nano7-untethered/nanoapps
# The SDK assigns this with :=; override before inclusion also selects libgcc.
override HB_ARCH_FLAGS := -mcpu=cortex-a5 -mthumb -mfloat-abi=soft -mfpu=neon
CHECK_NO_THUMB_FP := 1
.DEFAULT_GOAL := all
include $(NANOAPPS)/sdk/hb_app.mk

.PHONY: package
all: tri.hbapp
tri.hbapp: $(APP_OUT)
	cp $< $@
