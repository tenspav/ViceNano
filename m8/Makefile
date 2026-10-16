.DEFAULT_GOAL := all
NANOOS ?= /home/pavliukov/ipodwork/NanoRework/nanoos
NANOAPPS ?= $(abspath $(NANOOS)/../../NanoApps)
CHAIN := $(abspath $(NANOOS)/../probe/chainloader)
CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
GPU_BACKEND ?= mbx
ifeq ($(filter $(GPU_BACKEND),mbx stub),)
$(error GPU_BACKEND must be mbx or stub)
endif
BUILD := build-nanoos$(if $(filter stub,$(GPU_BACKEND)),-stub)

ARCH := -mcpu=cortex-a5 -marm -mfloat-abi=soft
FLAGS := $(ARCH) -Os -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-pic -fno-common -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -MMD -MP -I$(NANOOS)/src -I$(CURDIR) -I$(BUILD)
CPPFLAGS := -DM8_NANOOS
CXXFLAGS := $(FLAGS) -std=c++11 -fno-exceptions -fno-rtti -fno-threadsafe-statics
DRIVERS := lcd font input reset devmode mem timer
ifeq ($(GPU_BACKEND),mbx)
DRIVERS += mbx
endif
ifneq ($(wildcard $(NANOOS)/src/fmt.c),)
DRIVERS += fmt
endif
OBJECTS := $(addprefix $(BUILD)/,$(addsuffix .o,$(DRIVERS))) $(BUILD)/startup.o $(BUILD)/launcher.o $(BUILD)/nano.o $(BUILD)/nanoos_shim.o $(BUILD)/m8-input.o $(BUILD)/nanoos_runtime.o $(BUILD)/nanoos_gpu_$(GPU_BACKEND).o $(BUILD)/nanoos_world.o $(BUILD)/model.o
APPFLAGS := -mcpu=cortex-a8 -mthumb -mfpu=neon -Os -ffreestanding -fno-builtin -fno-pic -fno-common -ffunction-sections -fdata-sections -I$(NANOAPPS)/sdk -I$(NANOAPPS)/sdk/freestanding -I$(CHAIN)
.PHONY: all retail clean
all: $(BUILD)/m8-nanoos.bin $(BUILD)/m8-nanoos.hbapp
$(BUILD):
	mkdir -p $@
$(BUILD)/%.o: $(NANOOS)/src/%.c | $(BUILD)
	$(CC) $(FLAGS) -c $< -o $@
$(BUILD)/world_preview.h: generated/nbdecoshop01.h tools/make_world_preview.py | $(BUILD)
	python3 tools/make_world_preview.py $< $@
$(BUILD)/nanoos_world.o: $(BUILD)/world_preview.h
$(BUILD)/nanoos_runtime.o: nanoos_runtime.c | $(BUILD)
	$(CC) $(FLAGS) -c $< -o $@
$(BUILD)/startup.o: $(NANOOS)/src/startup.s | $(BUILD)
	$(CC) $(FLAGS) -c $< -o $@
$(BUILD)/launcher.c: $(NANOOS)/src/main.c tools/nanoos_launcher.py | $(BUILD)
	python3 tools/nanoos_launcher.py $< $@
$(BUILD)/launcher.o: $(BUILD)/launcher.c
	$(CC) $(FLAGS) -c $< -o $@
$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/m8-input.o: input.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/m8-nanoos.elf: $(OBJECTS) $(NANOOS)/kernel.ld
	$(CC) $(ARCH) -nostdlib -Wl,-T,$(NANOOS)/kernel.ld,--gc-sections,--build-id=none,--no-undefined,-Map,$(BUILD)/m8-nanoos.map $(OBJECTS) -lgcc -o $@
	$(CROSS_COMPILE)size $@
$(BUILD)/m8-nanoos.bin: $(BUILD)/m8-nanoos.elf
	$(CROSS_COMPILE)objcopy -O binary $< $@
$(BUILD)/chainloader-app.o: nanoos_chainloader.c | $(BUILD)
	$(CC) $(APPFLAGS) -c $< -o $@
$(BUILD)/shutdown-app.o: $(CHAIN)/shutdown.c | $(BUILD)
	$(CC) $(APPFLAGS) -c $< -o $@
$(BUILD)/payload.bin: $(BUILD)/m8-nanoos.bin tools/pack_kernel.py
	python3 tools/pack_kernel.py $< $@
$(BUILD)/handoff.o: $(CHAIN)/handoff.S $(BUILD)/payload.bin
	cd $(BUILD) && $(CC) $(APPFLAGS) -c $(CHAIN)/handoff.S -o handoff.o
$(BUILD)/chainloader.elf: $(addprefix $(BUILD)/,chainloader-app.o shutdown-app.o handoff.o)
	$(CC) $(APPFLAGS) -nostdlib -Wl,--gc-sections,--build-id=none,--no-undefined,-q,-e,payload_entry,-Ttext,0 $^ -lgcc -o $@
$(BUILD)/m8-nanoos.hbapp: $(BUILD)/chainloader.elf
	python3 $(NANOAPPS)/tools/mkrelocapp.py $< $@ payload_entry
retail:
	$(MAKE) -f Makefile.retail
clean:
	$(RM) -r $(BUILD)
-include $(wildcard $(BUILD)/*.d)

.PHONY: test-nanoemu
test-nanoemu: all
	NANOOS="$(NANOOS)" M8_BUILD="$(abspath $(BUILD))" python3 -u tools/test_nanoos.py
