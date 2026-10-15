APP_NAME := vcaudio
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
CXXFLAGS := $(filter-out -I$(SDK_DIR)/freestanding,$(CFLAGS)) -I/usr/include/newlib -DRW_GLES1 -DRW_NANO -fno-rtti -fno-threadsafe-statics -mword-relocations -std=c++11 -MMD -MP -I. -I../m5 -I../vendor/librw -I../vendor/revc-viewer/fakerw
CPP_OBJS := build/viewer.o build/model.o build/stream.o build/fakerw.o build/helpers.o build/nano.o build/runtime.o build/audio.o
LDLIBS := $(CPP_OBJS) ../m5/arm-build/src/librw.a /usr/lib/arm-none-eabi/newlib/thumb/nofp/libm.a /usr/lib/arm-none-eabi/newlib/thumb/nofp/libc.a $(LIBGCC)
$(BUILD)/$(APP_NAME).reloc.elf: $(CPP_OBJS) ../m5/arm-build/src/librw.a ../sdk/hbapp.ld
build/viewer.o: viewer.cpp ../m5/memory.h ../vendor/librw/src/gles1/rwgles1.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/model.o: ../m5/model.cpp ../m5/model.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/fakerw.o: ../m5/fakerw.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/runtime.o: ../sdk/runtime.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
../m5/arm-build/src/librw.a:
	$(MAKE) -C ../m5 arm-build/src/librw.a
all: vcaudio.hbapp
vcaudio.hbapp: $(APP_OUT)
	cp $< $@

-include $(CPP_OBJS:.o=.d)
build/helpers.o: ../m5/rw_helpers.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/nano.o: ../m5/nano.cpp ../m5/nano.h ../m5/generated/world.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/stream.o: ../m5/stream.cpp ../m5/stream.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
build/audio.o: audio.cpp audio.h generated/audio.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@
