# Makefile for "raspi" platform.

# Pixelsize 8 or 16, both kosher.
_CCDEF:=-DMA_PIXELSIZE=16 -DMA_APP_NAME=\"$(PROJECT_NAME)\"
_CCINC:=-Isrc/platform/raspi -Isrc/common -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux
_CCERR:=-Werror -Wimplicit

raspi_CC:=gcc -c -MMD -O2 $(_CCINC) $(_CCERR) $(_CCDEF)
raspi_LD:=gcc -L/opt/vc/lib
raspi_LDPOST:=-lbcm_host -lasound -lpthread

raspi_MIDDIR:=mid/platform/raspi
raspi_OUTDIR:=out/raspi

raspi_EXE:=$(raspi_OUTDIR)/$(PROJECT_NAME)

raspi_CFILES:=$(shell find src/platform/raspi -name '*.c') $(CFILES_COMMON) $(CFILES_MAIN)
raspi_OFILES:=$(patsubst src/%.c,$(raspi_MIDDIR)/%.o,$(raspi_CFILES))
raspi_OFILES+=$(patsubst mid/data/%.c,$(raspi_MIDDIR)/data/%.o,$(DATA_MIDFILES_EMBED))
-include $(raspi_OFILES:.o=.d)

$(raspi_MIDDIR)/%.o:src/%.c;$(PRECMD) $(raspi_CC) -o $@ $<
$(raspi_MIDDIR)/data/%.o:mid/data/%.c;$(PRECMD) $(raspi_CC) -o $@ $<

$(raspi_EXE):$(raspi_OFILES);$(PRECMD) $(raspi_LD) -o $@ $^ $(raspi_LDPOST)

