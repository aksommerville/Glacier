# Makefile for Multiarcade data files.

DATA_MIDDIR:=mid/data
DATA_OUTDIR:=out/data

# Under src/data there are two directories:
#  - embed/ for things we bake into the application.
#  - include/ for things we ship with it on the SD card.
# Embedded files must be known to us, they always require conversion.
# Unknown include files are fine; we'll copy them verbatim.
DATA_SRCFILES:=$(shell find src/data -type f)
DATA_SRCFILES_EMBED:=$(filter src/data/embed/%,$(DATA_SRCFILES))
DATA_SRCFILES_INCLUDE:=$(filter src/data/include/%,$(DATA_SRCFILES))

DATA_MIDFILES_EMBED:=$(filter %.c, \
  $(patsubst src/data/embed/image/%.png,$(DATA_MIDDIR)/image/%.c, \
  $(patsubst src/data/embed/font/%.png,$(DATA_MIDDIR)/font/%.c, \
  $(DATA_SRCFILES_EMBED) \
)))

DATA_INCLUDE_FILES:=$(patsubst src/data/include/%,$(DATA_OUTDIR)/%, \
  $(patsubst %.png,%.tsv, \
  $(patsubst src/data/include/title.png,src/data/include/$(PROJECT_NAME).tsv, \
  $(patsubst %.mid,%.bba, \
  $(DATA_SRCFILES_INCLUDE) \
))))

$(DATA_MIDDIR)/image/%.c:src/data/embed/image/%.png $(EXE_mktex);$(PRECMD) $(EXE_mktex) -o$@ $<
$(DATA_MIDDIR)/font/%.c:src/data/embed/font/%.png $(EXE_mkfont);$(PRECMD) $(EXE_mkfont) -o$@ $<

$(DATA_OUTDIR)/$(PROJECT_NAME).tsv:src/data/include/title.png $(EXE_mktsv);$(PRECMD) $(EXE_mktsv) -o$@ $< -s16
$(DATA_OUTDIR)/%.tsv:src/data/include/%.png $(EXE_mktsv);$(PRECMD) $(EXE_mktsv) -o$@ $< -s8
$(DATA_OUTDIR)/%:src/data/include/%;$(PRECMD) cp $< $@

#TODO fix mkbba
#$(DATA_OUTDIR)/%.bba:src/data/include/%.mid $(EXE_mkbba);$(PRECMD) $(EXE_mkbba) -o$@ $<
$(DATA_OUTDIR)/%.bba:src/data/include/%.mid;$(PRECMD) ../bb/out/macos-default/beepbot mid2bba -o$@ $<
