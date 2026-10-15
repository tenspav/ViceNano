APP_NAME := vcmodel
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
CXXFLAGS := $(filter-out -I$(SDK_DIR)/freestanding,$(CFLAGS)) -I/usr/include/newlib -DRW_GLES1 -DRW_NANO -fno-rtti -fno-threadsafe-statics -mword-relocations -std=c++11 -MMD -MP -I. -I../vendor/librw -I../vendor/revc-viewer/fakerw
CPP_OBJS := build/viewer.o build/model.o build/fakerw.o build/runtime.o
LDLIBS := $(CPP_OBJS) arm-build/src/librw.a /usr/lib/arm-none-eabi/newlib/thumb/nofp/libm.a /usr/lib/arm-none-eabi/newlib/thumb/nofp/libc.a $(LIBGCC)
$(BUILD)/$(APP_NAME).reloc.elf: $(CPP_OBJS) arm-build/src/librw.a ../sdk/hbapp.ld
build/viewer.o: viewer.cpp memory.h ../vendor/librw/src/gles1/rwgles1.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/model.o: model.cpp model.h generated/asset.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/fakerw.o: ../vendor/revc-viewer/fakerw_subset.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/runtime.o: ../sdk/runtime.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
arm-build/src/librw.a:
	cmake -S ../vendor/librw -B arm-build -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/arm-toolchain.cmake -DLIBRW_PLATFORM=GLES1 -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF
	cmake --build arm-build -j4
all: vcmodel.hbapp
vcmodel.hbapp: $(APP_OUT)
	cp $< $@

# Ask CMake to refresh its own dependency graph before comparing archive mtime.
.PHONY: rwlib
arm-build/src/librw.a: | rwlib
rwlib:
	cmake -S ../vendor/librw -B arm-build -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/arm-toolchain.cmake -DLIBRW_PLATFORM=GLES1 -DLIBRW_TOOLS=OFF -DLIBRW_INSTALL=OFF
	cmake --build arm-build -j4
$(BUILD)/$(APP_NAME).reloc.elf: ../sdk/gl.c
-include $(CPP_OBJS:.o=.d)
