APP_NAME := rwcube
SRCS :=
GL_SURFACE := 1
NANOAPPS ?= /home/pavliukov/ipodwork/nano7-untethered/nanoapps
override HB_ARCH_FLAGS := -mcpu=cortex-a5 -mthumb -mfloat-abi=soft -mfpu=neon
CHECK_NO_THUMB_FP := 1
.DEFAULT_GOAL := all
include $(NANOAPPS)/sdk/hb_app.mk
SDK_SRCS := $(filter-out $(SDK_DIR)/gl.c,$(SDK_SRCS)) ../sdk/gl.c
RELOC_LDFLAGS += -Wl,-T,../sdk/hbapp.ld -Wl,--target1-rel
CXX := arm-none-eabi-g++
CXXFLAGS := $(filter-out -I$(SDK_DIR)/freestanding,$(CFLAGS)) -I/usr/include/newlib -DRW_GLES1 -DRW_NANO -fno-rtti -fno-threadsafe-statics -mword-relocations -std=c++11 -MMD -MP
CPP_OBJS := build/rwcube.o build/runtime.o
LDLIBS := $(CPP_OBJS) arm-build/src/librw.a /usr/lib/arm-none-eabi/newlib/thumb/nofp/libm.a /usr/lib/arm-none-eabi/newlib/thumb/nofp/libc.a $(LIBGCC)
$(BUILD)/$(APP_NAME).reloc.elf: $(CPP_OBJS) arm-build/src/librw.a
build/rwcube.o: rwcube.cpp cube-data.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/runtime.o: ../sdk/runtime.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
arm-build/src/librw.a:
	cmake -S ../vendor/librw -B arm-build -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/arm-toolchain.cmake -DLIBRW_PLATFORM=GLES1 -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF
	cmake --build arm-build -j4
all: rwcube.hbapp
rwcube.hbapp: $(APP_OUT)
	cp $< $@
build/rwcube.o: ../vendor/librw/src/rwbase.h ../vendor/librw/src/gles1/rwgles1.h
$(BUILD)/$(APP_NAME).reloc.elf: ../sdk/hbapp.ld

# Ask CMake to refresh its own dependency graph before comparing archive mtime.
.PHONY: rwlib
arm-build/src/librw.a: | rwlib
rwlib:
	cmake -S ../vendor/librw -B arm-build -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/arm-toolchain.cmake -DLIBRW_PLATFORM=GLES1 -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF
	cmake --build arm-build -j4
$(BUILD)/$(APP_NAME).reloc.elf: ../sdk/gl.c
-include $(CPP_OBJS:.o=.d)
