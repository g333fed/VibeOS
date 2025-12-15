# MicroPython port for VibeOS
# Builds a PIE binary for VibeOS userspace

include ../../py/mkenv.mk

# Cross-compilation for aarch64
CROSS_COMPILE ?= aarch64-elf-

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h

# Disable some features for minimal size
MICROPY_ROM_TEXT_COMPRESSION ?= 0

# include py core make definitions
include $(TOP)/py/py.mk
# extmod removed - we don't use extended modules

INC += -I.
INC += -I$(TOP)
INC += -I$(BUILD)
INC += -I../../../user/lib
INC += -I../../../kernel/libc

# Compiler flags for VibeOS userspace (PIE for ELF loading)
CFLAGS += $(INC) -ffreestanding -nostdlib -nostartfiles
CFLAGS += -mcpu=cortex-a72 -mstrict-align
CFLAGS += -fPIE
CFLAGS += -Wall -std=gnu99
CFLAGS += -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
CFLAGS += -Wno-implicit-function-declaration -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
CFLAGS += -Wno-int-conversion
CFLAGS += -Os -DNDEBUG
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -include stddef.h -include stdint.h -include string.h -include errno.h -include stdio.h -include setjmp.h

# Linker flags
LDFLAGS += -nostdlib -pie -T ../../../user/linker.ld
LDFLAGS += --gc-sections

# Source files
SRC_C = \
	main.c \
	mphalport.c \
	modvibe.c \
	stubs.c \
	shared/readline/readline.c \
	shared/runtime/pyexec.c \
	shared/runtime/stdout_helpers.c \
	shared/runtime/gchelper_generic.c \

SRC_QSTR += shared/readline/readline.c shared/runtime/pyexec.c modvibe.c

# Assembly sources
SRC_S = setjmp.S

# Use crt0 from VibeOS userspace
CRT0 = ../../../user/lib/crt0.S
CRT0_OBJ = $(BUILD)/crt0.o

OBJ += $(PY_CORE_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_S:.S=.o))

all: $(BUILD)/micropython.elf

$(CRT0_OBJ): $(CRT0) | $(BUILD)
	$(CC) -c $< -o $@

$(BUILD)/%.o: %.S | $(BUILD)
	$(ECHO) "AS $<"
	$(Q)$(CC) -c $< -o $@

$(BUILD)/micropython.elf: $(CRT0_OBJ) $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $^
	$(Q)$(SIZE) $@

include $(TOP)/py/mkrules.mk
