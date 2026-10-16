APP_NAME := vcperf
SRCS := hh_bt.c hid_parse.c
GL_SURFACE := 1
NANOAPPS ?= /home/pavliukov/ipodwork/nano7-untethered/nanoapps
HB_CODE_MODE := thumb
HB_FLOAT_ABI := softfp
HB_FPU := vfpv4-d16
CHECK_NO_THUMB_FP := 0
override HB_ARCH_FLAGS := -mcpu=cortex-a5 -mthumb -mfloat-abi=softfp -mfpu=vfpv4-d16
.DEFAULT_GOAL := all
include $(NANOAPPS)/sdk/hb_app.mk
SDK_SRCS := $(filter-out $(SDK_DIR)/gl.c,$(SDK_SRCS)) ../sdk/gl.c
RELOC_LDFLAGS += -Wl,-T,../sdk/hbapp.ld -Wl,--target1-rel
CXX := arm-none-eabi-g++
CXXFLAGS := $(filter-out -I$(SDK_DIR)/freestanding,$(CFLAGS)) -I/usr/include/newlib -DRW_GLES1 -DRW_NANO -fno-rtti -fno-threadsafe-statics -mword-relocations -std=c++11 -O3 -fomit-frame-pointer -ffast-math -MMD -MP -I. -Igenerated -I../vendor/librw -I../vendor/revc-viewer/fakerw
CPP_OBJS := build/viewer.o build/model.o build/stream.o build/fakerw.o build/helpers.o build/nano.o build/runtime.o build/input.o build/audio.o
CXXFLAGS += $(M8_CPPFLAGS)
NEWLIB_DIR := /usr/lib/arm-none-eabi/newlib/thumb/nofp
LDLIBS := $(CPP_OBJS) arm-build/src/librw.a $(NEWLIB_DIR)/libm.a $(NEWLIB_DIR)/libc.a $(LIBGCC)
$(BUILD)/$(APP_NAME).reloc.elf: $(CPP_OBJS) arm-build/src/librw.a ../sdk/hbapp.ld

build/viewer.o: viewer.cpp memory.h ../vendor/librw/src/gles1/rwgles1.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/model.o: model.cpp model.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/fakerw.o: ../m5/fakerw.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/runtime.o: ../sdk/runtime.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/helpers.o: ../m5/rw_helpers.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/nano.o: nano.cpp nano.h generated/world.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/stream.o: stream.cpp stream.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/input.o: input.cpp input.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/audio.o: audio.cpp audio.h generated/audio.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

arm-build/src/librw.a:
	cmake -S ../vendor/librw -B arm-build -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/arm-toolchain.cmake -DLIBRW_PLATFORM=GLES1 -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF
	cmake --build arm-build -j4

all: vcperf.hbapp
vcperf.hbapp: $(APP_OUT)
	cp $< $@

-include $(CPP_OBJS:.o=.d)
