# Makefile for Multiarcade compile-time tools.

TOOL_CC:=gcc -c -MMD -O2 -Isrc/tool -Werror -Wimplicit -Wno-parentheses -Wno-comment
TOOL_LD:=gcc
TOOL_LDPOST:=-lz -lm
TOOL_EXESFX:=

TOOL_MIDDIR:=mid/tool
TOOL_OUTDIR:=out/tool

TOOL_CFILES:=$(shell find src/tool -name '*.c')
TOOL_OFILES:=$(patsubst src/tool/%.c,$(TOOL_MIDDIR)/%.o,$(TOOL_CFILES))
TOOL_OFILES_COMMON:=$(filter $(TOOL_MIDDIR)/common/%,$(TOOL_OFILES))
-include $(TOOL_OFILES:.o=.d)
$(TOOL_MIDDIR)/%.o:src/tool/%.c;$(PRECMD) $(TOOL_CC) -o $@ $<

# Each directory immediately under src/tool is an executable for us to build.
# Except "common", and this Makefile.
TOOLS:=$(filter-out Makefile common,$(notdir $(wildcard src/tool/*)))
TOOL_EXES:=

define TOOL_RULES
  EXE_$1:=$(TOOL_OUTDIR)/$1$(TOOL_EXESFX)
  TOOL_EXES+=$$(EXE_$1)
  tools:$$(EXE_$1)
  OFILES_$1:=$(filter $(TOOL_MIDDIR)/$1/%,$(TOOL_OFILES)) $(TOOL_OFILES_COMMON)
  $$(EXE_$1):$$(OFILES_$1);$$(PRECMD) $(TOOL_LD) -o$$@ $$^ $(TOOL_LDPOST)
endef
$(foreach T,$(TOOLS),$(eval $(call TOOL_RULES,$T)))

