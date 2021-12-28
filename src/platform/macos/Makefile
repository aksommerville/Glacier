# Makefile for "macos" platform.

_CCDEF:=-DMA_PIXELSIZE=8 -DMA_APP_NAME=\"$(PROJECT_NAME)\" -DMA_NON_TINY=1
_CCINC:=-Isrc/platform/macos -Isrc/common
_CCERR:=-Werror -Wimplicit -Wno-comment -Wno-parentheses -Wno-deprecated-declarations
# -Wno-deprecated-declarations eg "NSClosableWindowMask"

macos_CC:=gcc -c -MMD -O2 $(_CCINC) $(_CCERR) $(_CCDEF)
macos_OBJC:=gcc -c -MMD -O2 -xobjective-c $(_CCINC) $(_CCERR) $(_CCDEF)
macos_LD:=gcc
macos_LDPOST:=-framework IOKit -framework AudioUnit -framework Cocoa -framework OpenGL
#TODO Can we eliminate OpenGL?

macos_MIDDIR:=mid/platform/macos
macos_OUTDIR:=out/macos

macos_CFILES:=$(shell find src/platform/macos -name '*.[cm]') $(CFILES_COMMON) $(CFILES_MAIN)
macos_OFILES:=$(patsubst src/%,$(macos_MIDDIR)/%.o,$(basename $(macos_CFILES)))
macos_OFILES+=$(patsubst mid/data/%.c,$(macos_MIDDIR)/data/%.o,$(DATA_MIDFILES_EMBED))
-include $(macos_OFILES:.o=.d)

$(macos_MIDDIR)/%.o:src/%.c;$(PRECMD) $(macos_CC) -o $@ $<
$(macos_MIDDIR)/data/%.o:mid/data/%.c;$(PRECMD) $(macos_CC) -o $@ $<
$(macos_MIDDIR)/%.o:src/%.m;$(PRECMD) $(macos_OBJC) -o $@ $<
$(macos_MIDDIR)/data/%.o:mid/data/%.m;$(PRECMD) $(macos_OBJC) -o $@ $<

macos_BUNDLE_MAIN:=$(macos_OUTDIR)/GlacierRacer.app
macos_PLIST_MAIN:=$(macos_BUNDLE_MAIN)/Contents/Info.plist
macos_NIB_MAIN:=$(macos_BUNDLE_MAIN)/Contents/Resources/Main.nib
macos_EXE:=$(macos_BUNDLE_MAIN)/Contents/MacOS/GlacierRacer
macos_DATA_ARCHIVE:=$(macos_BUNDLE_MAIN)/Contents/Resources/data
macos_ICON_MAIN:=$(macos_BUNDLE_MAIN)/Contents/Resources/appicon.icns

macos_DATA_FILES:=$(patsubst $(DATA_OUTDIR)/%,$(macos_DATA_ARCHIVE)/%,$(DATA_INCLUDE_FILES))
$(macos_DATA_ARCHIVE)/%:$(DATA_OUTDIR)/%;$(PRECMD) cp $< $@

$(macos_EXE):$(macos_PLIST_MAIN) $(macos_NIB_MAIN) $(macos_ICON_MAIN) $(macos_OFILES) $(macos_DATA_FILES);$(PRECMD) $(macos_LD) -o $@ $(macos_OFILES) $(macos_LDPOST)
RUNCMD=open -W $(macos_BUNDLE_MAIN) --args --reopen-tty=$$(tty) --chdir=$$(pwd) --files=out/data

$(macos_PLIST_MAIN):src/platform/macos/Info.plist;$(PRECMD) cp $< $@
$(macos_NIB_MAIN):src/platform/macos/Main.xib;$(PRECMD) ibtool --compile $@ $<

clean:remove-macos-preferences
remove-macos-preferences:;rm -rf ~/Library/Preferences/com.aksommerville.glacierracer

INPUT_ICONS:=$(wildcard src/platform/macos/appicon.iconset/*)
$(macos_ICON_MAIN):$(INPUT_ICONS);$(PRECMD) iconutil -c icns -o $@ src/platform/macos/appicon.iconset

