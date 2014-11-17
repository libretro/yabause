DEBUG = 0
DYNAREC = 0
HAVE_THREADS = 1
HAVE_GRIFFIN = 1

ifeq ($(platform),)
	platform = unix
	ifeq ($(shell uname -a),)
		platform = win
	else ifneq ($(findstring Darwin,$(shell uname -a)),)
		platform = osx
	else ifneq ($(findstring MINGW,$(shell uname -a)),)
		platform = win
	endif
endif

# system platform
system_platform = unix
ifeq ($(shell uname -a),)
	EXE_EXT = .exe
	system_platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
	system_platform = osx
	arch = intel
	ifeq ($(shell uname -p),powerpc)
		arch = ppc
	endif
else ifneq ($(findstring MINGW,$(shell uname -a)),)
	system_platform = win
endif

YABAUSE_DIR  := ../yabause
SOURCE_DIR	:= $(YABAUSE_DIR)/src
LIBRETRO_DIR := ./

TARGET_NAME = yabause
CC_AS ?= $(CC)

# Unix
ifeq ($(platform), unix)
	TARGET := $(TARGET_NAME)_libretro.so
	fpic := -fPIC
	SHARED := -shared -Wl,--no-undefined -Wl,--version-script=link.T
	ENDIANNESS_DEFINES := -DLSB_FIRST
	#DYNAREC = 2
	LDFLAGS += -lpthread
	HAVE_PTHREADS = 1

# OS X
else ifeq ($(platform), osx)
	TARGET := $(TARGET_NAME)_libretro.dylib
	fpic := -fPIC -mmacosx-version-min=10.6
	SHARED := -dynamiclib
	ENDIANNESS_DEFINES := -DLSB_FIRST
	#DYNAREC = 2
	LDFLAGS += -lpthread
	HAVE_PTHREADS = 1

# iOS
else ifeq ($(platform), ios)
	TARGET := $(TARGET_NAME)_libretro_ios.dylib
	fpic := -fPIC
	SHARED := -dynamiclib
	ENDIANNESS_DEFINES := -DLSB_FIRST
	CC = clang -arch armv7 -isysroot $(IOSSDK)
	CC_AS =  perl ./libretro/gas-preprocessor.pl $(CC)
	CXX = clang++ -arch armv7 -isysroot $(IOSSDK)

# QNX
else ifeq ($(platform), qnx)
	TARGET := $(TARGET_NAME)_libretro_qnx.so
	fpic := -fPIC
	SHARED := -shared -Wl,--version-script=libretro/link.T
	CC = qcc -Vgcc_ntoarmv7le
	CC_AS = $(CC)
	CXX = QCC -Vgcc_ntoarmv7le
	AR = qcc -Vgcc_ntoarmv7le
	ENDIANNESS_DEFINES += -D__BLACKBERRY_QNX__ -marm -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=softfp -DLSB_FIRST

# PS3
else ifeq ($(platform), ps3)
	TARGET := $(TARGET_NAME)_libretro_ps3.a
	CC = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-gcc.exe
	CXX = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-g++.exe
	AR = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-ar.exe
	ENDIANNESS_DEFINES = -DWORDS_BIGENDIAN -DMSB_FIRST -D__ppc__
	STATIC_LINKING = 1

# sncps3
else ifeq ($(platform), sncps3)
	TARGET := $(TARGET_NAME)_libretro_ps3.a
	CC = $(CELL_SDK)/host-win32/sn/bin/ps3ppusnc.exe
	CXX = $(CELL_SDK)/host-win32/sn/bin/ps3ppusnc.exe
	AR = $(CELL_SDK)/host-win32/sn/bin/ps3snarl.exe
	ENDIANNESS_DEFINES = -DWORDS_BIGENDIAN -DMSB_FIRST -D__ppc__
	STATIC_LINKING = 1

# Lightweight PS3 Homebrew SDK
else ifeq ($(platform), psl1ght)
	TARGET := $(TARGET_NAME)_libretro_psl1ght.a
	CC = $(PS3DEV)/ppu/bin/ppu-gcc$(EXE_EXT)
	CC = $(PS3DEV)/ppu/bin/ppu-g++$(EXE_EXT)
	AR = $(PS3DEV)/ppu/bin/ppu-ar$(EXE_EXT)
	ENDIANNESS_DEFINES = -DWORDS_BIGENDIAN -DMSB_FIRST -D__ppc__
	STATIC_LINKING = 1

# PSP
else ifeq ($(platform), psp1)
	TARGET := $(TARGET_NAME)_libretro_psp1.a
	CC = psp-gcc$(EXE_EXT)
	CC_AS = $(CC)
	CXX = psp-g++$(EXE_EXT)
	AR = psp-ar$(EXE_EXT)
	ENDIANNESS_DEFINES += -DPSP -G0 -DLSB_FIRST
	STATIC_LINKING = 1

# Nintendo Game Cube
else ifeq ($(platform), ngc)
	TARGET := $(TARGET_NAME)_libretro_ngc.a
	CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
	CXX = $(DEVKITPPC)/bin/powerpc-eabi-g++$(EXE_EXT)
	AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
	ENDIANNESS_DEFINES = -DWORDS_BIGENDIAN -DMSB_FIRST -DGEKKO -DHW_DOL -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__
	STATIC_LINKING = 1

# Nintendo Wii
else ifeq ($(platform), wii)
	TARGET := $(TARGET_NAME)_libretro_wii.a
	CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
	CXX = $(DEVKITPPC)/bin/powerpc-eabi-g++$(EXE_EXT)
	AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
	ENDIANNESS_DEFINES = -DWORDS_BIGENDIAN -DMSB_FIRST -DGEKKO -DHW_RVL -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__
	STATIC_LINKING = 1

# ARM
else ifeq ($(platform), armv)
	TARGET := $(TARGET_NAME)_libretro.so
	fpic := -fPIC
	SHARED := -shared -Wl,--no-undefined -Wl,--version-script=link.T
	ENDIANNESS_DEFINES := -DLSB_FIRST
	LDFLAGS += -lpthread
	HAVE_PTHREADS = 1
	ifneq (,$(findstring cortexa8,$(platform)))
		FLAGS += -marm -mcpu=cortex-a8
	else ifneq (,$(findstring cortexa9,$(platform)))
		FLAGS += -marm -mcpu=cortex-a9
	endif
	FLAGS += -marm
	ifneq (,$(findstring neon,$(platform)))
		FLAGS += -mfpu=neon
		HAVE_NEON = 1
	endif
	ifneq (,$(findstring softfloat,$(platform)))
		FLAGS += -mfloat-abi=softfp
	else ifneq (,$(findstring hardfloat,$(platform)))
		FLAGS += -mfloat-abi=hard
	endif
	FLAGS += -DARM

# Windows
else
	TARGET := $(TARGET_NAME)_libretro.dll
	CC = gcc
	CXX = g++
	SHARED := -shared -Wl,--no-undefined -Wl,--version-script=link.T
	LDFLAGS += -static-libgcc -static-libstdc++ -lwinmm
	ENDIANNESS_DEFINES := -DLSB_FIRST
	FLAGS += -DHAVE_WFOPEN
	#DYNAREC = 2

endif

include Makefile.common

SOURCES += $(LIBRETRO_SOURCES) $(YABAUSE_SOURCES)
OBJECTS += $(SOURCES:.c=.o) $(SOURCES_C:.c=.o) $(C68KEXEC_OBJECT)

all: $(TARGET)

ifeq ($(DEBUG), 0)
FLAGS += -O3
else
FLAGS += -O0 -g
endif

LDFLAGS += $(fpic) $(SHARED)
FLAGS += $(fpic)
INCFLAGS += -I. -I../yabause -I../yabause/src

WARNINGS :=
FLAGS += $(INCFLAGS) -D__LIBRETRO__ $(ENDIANNESS_DEFINES) -DSIZEOF_DOUBLE=8 $(WARNINGS) \
			-DUSE_SCSP2=1 -DNO_CLI -DHAVE_SYS_TIME_H -DHAVE_GETTIMEOFDAY -DVERSION=\"0.9.12\" -DHAVE_STRCASECMP -DINLINE="inline"

CXXFLAGS += $(FLAGS)

CFLAGS += $(FLAGS)
ifeq ($(platform),qnx)
CFLAGS += -Wc,-std=c99
else
CFLAGS += -std=gnu99
endif

$(TARGET): $(OBJECTS)
ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJECTS)
else
	$(CXX) -o $@ $^ $(LDFLAGS)
endif

%.o: %.S
	$(CC_AS) $(CFLAGS) -c $^ -o $@

%.o: %.s
	$(CC_AS) $(CFLAGS) -c $^ -o $@

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
	
$(C68KEXEC_OBJECT): $(C68KEXEC_SOURCE)
	$(CC) -c -o $@ $< $(CFLAGS) -O0

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean
