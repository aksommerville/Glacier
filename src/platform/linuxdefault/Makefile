# Makefile for "linuxdefault" platform.

_CCDEF:=-DMA_PIXELSIZE=8 -DMA_APP_NAME=\"$(PROJECT_NAME)\" -DMA_NON_TINY=1
_CCINC:=-Isrc/platform/linuxdefault -Isrc/common
_CCERR:=-Werror -Wimplicit

linuxdefault_CC:=gcc -c -MMD -O2 $(_CCINC) $(_CCERR) $(_CCDEF)
linuxdefault_LD:=gcc
linuxdefault_LDPOST:=-lX11 -lpulse-simple -lpthread

linuxdefault_MIDDIR:=mid/platform/linuxdefault
linuxdefault_OUTDIR:=out/linuxdefault

linuxdefault_EXE:=$(linuxdefault_OUTDIR)/$(PROJECT_NAME)

linuxdefault_CFILES:=$(shell find src/platform/linuxdefault -name '*.c') $(CFILES_COMMON) $(CFILES_MAIN)
linuxdefault_OFILES:=$(patsubst src/%.c,$(linuxdefault_MIDDIR)/%.o,$(linuxdefault_CFILES))
linuxdefault_OFILES+=$(patsubst mid/data/%.c,$(linuxdefault_MIDDIR)/data/%.o,$(DATA_MIDFILES_EMBED))
-include $(linuxdefault_OFILES:.o=.d)

$(linuxdefault_MIDDIR)/%.o:src/%.c;$(PRECMD) $(linuxdefault_CC) -o $@ $<
$(linuxdefault_MIDDIR)/data/%.o:mid/data/%.c;$(PRECMD) $(linuxdefault_CC) -o $@ $<

$(linuxdefault_EXE):$(linuxdefault_OFILES) data;$(PRECMD) $(linuxdefault_LD) -o $@ $(linuxdefault_OFILES) $(linuxdefault_LDPOST)
RUNCMD=$< --files=out/data
