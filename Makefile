#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# GRAPHICS is a list of directories containing graphics files
# GFXBUILD is the directory where converted graphics files will be placed
#   If set to $(BUILD), it will statically link in the converted
#   files as if they were data files.
#
# NO_SMDH: if set to anything, no SMDH file is generated.
# ROMFS is the directory which contains the RomFS, relative to the Makefile (Optional)
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
# APP_AUTHOR is the author of the app stored in the SMDH file (Optional)
# ICON is the filename of the icon (.png), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.png
#     - icon.png
#     - <libctru folder>/default_icon.png
#---------------------------------------------------------------------------------
3DS_LIBS     := source/3ds-libs
3DS_LIBS_SRC := $(3DS_LIBS)/source
3DS_LIBS_INC := $(3DS_LIBS)/include

APP_TITLE := Snake Engine
APP_DESCRIPTION := A FNF engine for the Nintendo 3DS
APP_AUTHOR := Snakyjoel
ifeq ($(LITE),1)
TARGET := $(notdir $(CURDIR))-lite
ROMFS  := romfs-lite
RSF    := template-lite.rsf
ICON   := homemenu/iconLite.png
else
TARGET := $(notdir $(CURDIR))
ROMFS  := romfs
RSF    := template.rsf
ICON   := homemenu/icon.png
endif
BUILD		:=	build
SOURCES		:=	source source/backend source/backend/codecs source/backend/savedata source/backend/parsers source/shaders source/objects source/states source/substates source/options source/debug $(3DS_LIBS_SRC) $(3DS_LIBS_SRC)/renderable $(3DS_LIBS_SRC)/audio
DATA		:=	data
INCLUDES	:=	include source source/backend source/backend/codecs source/backend/savedata source/backend/parsers source/shaders source/objects source/states source/substates source/options source/debug $(3DS_LIBS_INC) $(3DS_LIBS_INC)/renderable $(3DS_LIBS_INC)/audio
GRAPHICS	:=	gfx gfx/stages gfx/characters
#GFXBUILD	:=	$(BUILD)
GFXBUILD	:=	$(ROMFS)/gfx

# --- BANNER CUSTOMIZATION ---
# Automatically checks if banner.cgfx exists to compile a 3D animated banner.
# Otherwise, it falls back to compile the static 2D banner (banner.png).
ifneq ($(wildcard $(CURDIR)/homemenu/banner.cgfx),)
    BANNER_CMD := bannertool makebanner -ci "$(CURDIR)/homemenu/banner.cgfx" -a "$(CURDIR)/homemenu/menu.wav" -o "$(CURDIR)/build/banner.bin"
    BANNER_MSG := "[3D BANNER] Compilando banner animado 3D desde banner.cgfx..."
else
    BANNER_CMD := bannertool makebanner -i "$(CURDIR)/homemenu/banner.png" -a "$(CURDIR)/homemenu/menu.wav" -o "$(CURDIR)/build/banner.bin"
    BANNER_MSG := "[2D BANNER] banner.cgfx no encontrado. Compilando banner estatico 2D..."
endif

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -D__3DS__

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11 -include $(TOPDIR)/source/Import.hpp

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -llua5.1 -lcitro2d -lcitro3d -lctru -lm -lz -lvorbisidec -logg -ljansson




#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CTRULIB) $(PORTLIBS)


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/export/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
# --- ASSET MANAGEMENT ---
# Everything in 'assets/' will be mirrored to RomFS.
# .t3s files will be converted to .t3x at the same relative path.
# Others will be copied.
ROMFS_SOURCE := $(TOPDIR)/assets
ROMFS_TARGET := $(TOPDIR)/$(ROMFS)

# Function to recursive find files
# On Windows/PowerShell we use wildcard in a loop or shell find
ifeq ($(LITE),1)
ALL_ASSETS := $(shell find $(ROMFS_SOURCE) -type f | grep -v "/videos/")
else
ALL_ASSETS := $(shell find $(ROMFS_SOURCE) -type f)
endif

# Filter assets
T3S_FILES  := $(filter %.t3s, $(ALL_ASSETS))
XML_FILES  := $(filter %.xml, $(ALL_ASSETS))
JSON_FILES := $(filter %.json, $(ALL_ASSETS))
OGG_FILES  := $(filter %.ogg, $(ALL_ASSETS))
ADP_FILES  := $(filter %.adp, $(ALL_ASSETS))
TTF_FILES  := $(filter %.ttf, $(ALL_ASSETS))
PNG_FILES  := $(filter %.png, $(ALL_ASSETS))
LUA_FILES  := $(filter %.lua, $(ALL_ASSETS))
TXT_FILES  := $(filter %.txt, $(ALL_ASSETS))
SNAKY_FILES := $(filter %.snaky, $(ALL_ASSETS))
RAWTEX_FILES := $(filter %.rawtex, $(ALL_ASSETS))

# Target mappings
ROMFS_T3XFILES  := $(patsubst $(ROMFS_SOURCE)/%.t3s, $(ROMFS_TARGET)/%.t3x, $(T3S_FILES))
ROMFS_XMLFILES  := $(patsubst $(ROMFS_SOURCE)/%, $(ROMFS_TARGET)/%, $(XML_FILES))
ROMFS_JSONFILES := $(patsubst $(ROMFS_SOURCE)/%, $(ROMFS_TARGET)/%, $(JSON_FILES))
ROMFS_OGGFILES  := $(patsubst $(ROMFS_SOURCE)/%, $(ROMFS_TARGET)/%, $(OGG_FILES))
ROMFS_ADPFILES  := $(patsubst $(ROMFS_SOURCE)/%, $(ROMFS_TARGET)/%, $(ADP_FILES))
ROMFS_TTFFILES  := $(patsubst $(ROMFS_SOURCE)/%.ttf, $(ROMFS_TARGET)/%.bcfnt, $(TTF_FILES))
ROMFS_LUAFILES  := $(patsubst $(ROMFS_SOURCE)/%, $(ROMFS_TARGET)/%, $(LUA_FILES))
ROMFS_TXTFILES  := $(patsubst $(ROMFS_SOURCE)/%, $(ROMFS_TARGET)/%, $(TXT_FILES))
ROMFS_SNAKYFILES:= $(patsubst $(ROMFS_SOURCE)/%, $(ROMFS_TARGET)/%, $(SNAKY_FILES))
ROMFS_RAWTEXFILES:= $(patsubst $(ROMFS_SOURCE)/%, $(ROMFS_TARGET)/%, $(RAWTEX_FILES))

# Static target names for header flattening (legacy support)
T3XHFILES := $(patsubst %.t3s, $(BUILD)/%.h, $(notdir $(T3S_FILES)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_SOURCES)

export HFILES	:=	$(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
			$(notdir $(T3S_FILES:.t3s=.h))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS	:=	$(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(OUTPUT).smdh
endif

ifneq ($(ROMFS_TARGET),)
	export _3DSXFLAGS += --romfs=$(ROMFS_TARGET)
endif

.PHONY: all clean cia 3dsx-lite cia-lite

#---------------------------------------------------------------------------------
# Export RomFS files to ensure they can be used as dependencies in the recursive make
export ALL_ROMFS_OUT := $(ROMFS_T3XFILES) $(ROMFS_XMLFILES) $(ROMFS_JSONFILES) $(ROMFS_OGGFILES) $(ROMFS_ADPFILES) $(ROMFS_TTFFILES) $(ROMFS_LUAFILES) $(ROMFS_TXTFILES) $(ROMFS_SNAKYFILES) $(ROMFS_RAWTEXFILES)

all: $(BUILD) $(ROMFS_TARGET) $(ALL_ROMFS_OUT)
	@mkdir -p $(CURDIR)/export
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

3dsx-lite:
	@$(MAKE) LITE=1

clean:
	@echo "Cleaning build and romfs assets..."
	@rm -rf $(BUILD) $(ROMFS_TARGET) $(CURDIR)/export/*.elf $(CURDIR)/export/*.3dsx $(CURDIR)/export/*.cia $(CURDIR)/export/*.smdh


cia: all
	@echo "Construyendo archivo .cia con RomFS (Estructura Psych)..."
	@echo $(BANNER_MSG)
	@$(BANNER_CMD)
	@makerom -f cia -o "$(CURDIR)/export/$(TARGET).cia" -elf "$(CURDIR)/export/$(TARGET).elf" -rsf "$(CURDIR)/$(RSF)" -icon "$(CURDIR)/export/$(TARGET).smdh" -banner "$(CURDIR)/build/banner.bin" -exefslogo -target t -ver 1 -major 1 -minor 0 -micro 0 -desc app:7

cia-lite:
	@$(MAKE) cia LITE=1

$(BUILD):
	@mkdir -p $@

$(ROMFS_TARGET):
	@mkdir -p $@

# --- RULES ---

$(ROMFS_TARGET)/%.bcfnt: $(ROMFS_SOURCE)/%.ttf
	@echo "Converting Font $< -> $@"
	@mkdir -p $(dir $@)
	@mkbcfnt -s 24 $< -o $@

$(ROMFS_TARGET)/%.t3x: $(ROMFS_SOURCE)/%.t3s $(ROMFS_SOURCE)/%.png
	@echo "Converting $< -> $@"
	@mkdir -p $(dir $@)
	@tex3ds --atlas -i $< -H $(BUILD)/$(notdir $*.h) -d $(DEPSDIR)/$(notdir $*.d) -o $@

$(ROMFS_TARGET)/%: $(ROMFS_SOURCE)/%
	@echo "Copying $< -> $@"
	@mkdir -p $(dir $@)
	@cp $< $@

# Re-flatten the headers for the build folder to simplify source includes
$(BUILD)/%.h: $(ROMFS_SOURCE)/%.t3s $(ROMFS_SOURCE)/%.png
	@echo "Static Header $< -> $@"
	@tex3ds --atlas -i $< -H $@ -d $(DEPSDIR)/$(notdir $*.d) -o $(ROMFS_TARGET)/$*.t3x

#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(_3DSXDEPS) $(ALL_ROMFS_OUT)

$(OUTPUT).smdh	:	$(APP_ICON)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
.PRECIOUS	:	%.t3x %.shbin
#---------------------------------------------------------------------------------
%.t3x.o	%_t3x.h :	%.t3x
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

#---------------------------------------------------------------------------------
%.shbin.o %_shbin.h : %.shbin
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------