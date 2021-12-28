# Makefile for "generic" platform.
# This is a good starting point for making a new platform.

generic_CC:=gcc -c -MMD -O2 -Isrc/platform/generic -Isrc/common -Werror -Wimplicit -DMA_PIXELSIZE=8
generic_LD:=gcc
generic_LDPOST:=

generic_MIDDIR:=mid/platform/generic
generic_OUTDIR:=out/generic

generic_EXE:=$(generic_OUTDIR)/$(PROJECT_NAME)

generic_CFILES:=$(shell find src/platform/generic -name '*.c') $(CFILES_COMMON) $(CFILES_MAIN)
generic_OFILES:=$(patsubst src/%.c,$(generic_MIDDIR)/%.o,$(generic_CFILES))
generic_OFILES+=$(patsubst mid/data/%.c,$(generic_MIDDIR)/data/%.o,$(DATA_MIDFILES_EMBED))
-include $(generic_OFILES:.o=.d)

$(generic_MIDDIR)/%.o:src/%.c;$(PRECMD) $(generic_CC) -o $@ $<
$(generic_MIDDIR)/data/%.o:mid/data/%.c;$(PRECMD) $(generic_CC) -o $@ $<

$(generic_EXE):$(generic_OFILES);$(PRECMD) $(generic_LD) -o $@ $^ $(generic_LDPOST)

RUNCMD=trap '' INT ; $< ; clear
