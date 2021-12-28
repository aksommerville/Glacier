# Makefile for "tinyarcade" platform -- this is not like other platforms.

# If you're looking for MA_PIXELSIZE, it's not here.
# (TODO) I don't know how to pass -D options thru arduino-builder.
# So for now, change the default in multiarcade.h if you want to change for TinyArcade.

# Hard-coded defaults are for my dev box. Season to taste.
ifeq (,greyskull)
ifndef MA_PORT
  MA_PORT:=ttyACM0
endif
ifndef MA_IDEROOT
  MA_IDEROOT:=/opt/arduino-1.8.16
endif
ifndef MA_IDEVERSION
  MA_IDEVERSION:=10816
endif
ifndef MA_BUILDER_OPTS
  MA_BUILDER_OPTS:=
  #MA_BUILDER_OPTS:=-verbose
endif
else # MacBook Air
  ifndef MA_PORT
    MA_PORT:=tty.usbmodemFA131
  endif
  ifndef MA_IDEROOT
    MA_IDEROOT:=/Applications/Arduino.app/Contents/Java
  endif
  ifndef MA_IDEVERSION
    MA_IDEVERSION:=10819
  endif
  ifndef MA_BUILDER_OPTS
    MA_BUILDER_OPTS:=
  endif
endif

# I think these will be constant, but allowing you might override.
ifndef MA_BUILDER
  MA_BUILDER:=$(MA_IDEROOT)/arduino-builder
endif
ifndef MA_PKGROOT
  #MA_PKGROOT:=$(wildcard ~/.arduino15/packages) # Linux
  MA_PKGROOT:=$(wildcard ~/Library/Arduino15/packages)
endif
ifndef MA_LIBSROOT
  #MA_LIBSROOT:=$(wildcard ~/Arduino/libraries)
  MA_LIBSROOT:=$(wildcard ~/Documents/Arduino/libraries)
endif

#---------------------------------------------------------------------------
# The remainder should not require any adjustment.
# (ha ha ha yeah right)

TINY_TMPDIR_SOLO:=mid/tinyarcade/build-solo
TINY_TMPDIR_HOSTED:=mid/tinyarcade/build-hosted
TINY_CACHEDIR_SOLO:=mid/tinyarcade/cache-solo
TINY_CACHEDIR_HOSTED:=mid/tinyarcade/cache-hosted
$(TINY_TMPDIR_SOLO) $(TINY_TMPDIR_HOSTED) $(TINY_CACHEDIR_SOLO) $(TINY_CACHEDIR_HOSTED):;mkdir -p $@

TINY_BIN_SOLO:=out/$(PROJECT_NAME)-solo.bin
TINY_BIN_HOSTED:=out/$(PROJECT_NAME)-hosted.bin
TINY_PACKAGE:=out/$(PROJECT_NAME).zip

TINY_SRCFILES:= \
  $(CFILES_COMMON) $(CFILES_MAIN) $(HFILES_MAIN) $(DATA_MIDFILES_EMBED) \
  src/common/multiarcade.h \
  src/common/bba.h src/common/bb_midi.h \
  $(shell find src/platform/tinyarcade -name '*.[ch]') \
  src/platform/tinyarcade/sercom_glue.cpp \
  src/platform/tinyarcade/ma_sd.cpp \
  src/platform/tinyarcade/usb_glue.cpp

# All the C files get copied here to simplify our request to arduino-builder.
TINY_SCRATCHDIR:=mid/tinyarcade/scratch
TINY_SCRATCHFILES:=
define TINY_SCRATCHRULE
  TINY_SCRATCHFILES+=mid/tinyarcade/scratch/$(notdir $1)
  mid/tinyarcade/scratch/$(notdir $1):$1;$$(PRECMD) cp $$< $$@
endef
$(foreach F,$(TINY_SRCFILES),$(eval $(call TINY_SCRATCHRULE,$F)))
TINY_SCRATCHFILES+=mid/tinyarcade/scratch/dummy.cpp
mid/tinyarcade/scratch/dummy.cpp:src/platform/tinyarcade/dummy.cpp;$(PRECMD) cp $< $@

define BUILD # 1=goal, 2=tmpdir, 3=cachedir, 4=BuildOption
$1:$2 $3 $(TINY_SCRATCHFILES); \
  $(MA_BUILDER) \
  -compile \
  -logger=machine \
  -hardware $(MA_IDEROOT)/hardware \
  $(if $(MA_PKGROOT),-hardware $(MA_PKGROOT)) \
  -tools $(MA_IDEROOT)/tools-builder \
  -tools $(MA_IDEROOT)/hardware/tools/avr \
  $(if $(MA_PKGROOT),-tools $(MA_PKGROOT)) \
  -built-in-libraries $(MA_IDEROOT)/libraries \
  $(if $(MA_LIBSROOT),-libraries $(MA_LIBSROOT)) \
  -fqbn=TinyCircuits:samd:tinyarcade:BuildOption=$4 \
  -ide-version=$(MA_IDEVERSION) \
  -build-path $2 \
  -warnings=none \
  -build-cache $3 \
  -prefs=build.warn_data_percentage=75 \
  $(MA_BUILDER_OPTS) \
  mid/tinyarcade/scratch/dummy.cpp $(TINY_SCRATCHFILES) \
  2>&1 | etc/tool/reportstatus.py
endef

# For inclusion in a TinyArcade SD card.
TINY_PRODUCT_HOSTED:=$(TINY_TMPDIR_HOSTED)/dummy.cpp.bin
$(TINY_BIN_HOSTED):build-hosted;$(PRECMD) cp $(TINY_PRODUCT_HOSTED) $@
$(eval $(call BUILD,build-hosted,$(TINY_TMPDIR_HOSTED),$(TINY_CACHEDIR_HOSTED),TAgame))

# For upload.
TINY_PRODUCT_SOLO:=$(TINY_TMPDIR_SOLO)/dummy.cpp.bin
$(TINY_BIN_SOLO):build-solo;$(PRECMD) cp $(TINY_PRODUCT_SOLO) $@
$(eval $(call BUILD,build-solo,$(TINY_TMPDIR_SOLO),$(TINY_CACHEDIR_SOLO),TAmenu))
  
launch:$(TINY_BIN_SOLO); \
  $(MA_PKGROOT)/arduino/tools/bossac/1.7.0-arduino3/bossac -i -d --port=$(MA_PORT) -U true -i -e -w $(TINY_BIN_SOLO) -R
# Doesn't work on MacBook Air, taking my chances without it:
#  stty -F /dev/$(MA_PORT) 1200 ; \
#  sleep 2 ; \

#/Users/andy/Library/Arduino15/packages/arduino/tools/bossac/1.7.0-arduino3/bossac -i -d --port=cu.usbmodemFD121 -U true -i -e -w -v /var/folders/gk/4p0c8qzn34s2lw51vxz2f0km0000gn/T/arduino_build_568358/TinyScreenBasicExample.ino.bin -R 

  
# I guess a README would be nice, too.
$(TINY_PACKAGE):$(TINY_BIN_HOSTED) $(DATA_INCLUDE_FILES);$(PRECMD) \
  rm -rf out/$(PROJECT_NAME) ; \
  mkdir out/$(PROJECT_NAME) || exit 1 ; \
  cp -r out/data/* out/$(PROJECT_NAME) || exit 1 ; \
  cp $(TINY_BIN_HOSTED) out/$(PROJECT_NAME)/$(PROJECT_NAME).bin || exit 1 ; \
  rm -f out/$(PROJECT_NAME)/hiscore ; \
  cd out ; \
  zip -r $(PROJECT_NAME).zip $(PROJECT_NAME) >/dev/null || exit 1 ; \
  rm -r $(PROJECT_NAME)
  
sdcard:$(TINY_PACKAGE);etc/tool/sdcard.sh $(TINY_PACKAGE)
