# Makefile for Multiarcade tests.

TEST_CC:=gcc -c -MMD -O2 -Isrc/common -Isrc -Imid/test/int -Werror -Wimplicit
TEST_LD:=gcc
TEST_LDPOST:=

TEST_EXE_ITEST:=out/itest
TEST_DIR_UTEST:=out/utest
TEST_EXES:=$(TEST_EXE_ITEST)
TEST_TOC:=mid/test/int/ma_itest_toc.h

TEST_CFILES:=$(shell find src/test -name '*.c')
TEST_OFILES:=$(patsubst src/test/%.c,mid/test/%.o,$(TEST_CFILES))
-include $(TEST_OFILES:.o=.d)

TEST_OFILES_COMMON:=$(filter mid/test/common/%,$(TEST_OFILES))
TEST_OFILES_UNIT:=$(filter mid/test/unit/%,$(TEST_OFILES))
TEST_OFILES_INT:=$(filter mid/test/int/%,$(TEST_OFILES))

TEST_EXES_UTEST:=$(patsubst mid/test/unit/%.o,$(TEST_DIR_UTEST)/%,$(TEST_OFILES_UNIT))
TEST_EXES+=$(TEST_EXES_UTEST)

mid/test/%.o:src/test/%.c;$(PRECMD) $(TEST_CC) -o $@ $<
$(TEST_DIR_UTEST)/%:mid/test/unit/%.o $(TEST_OFILES_COMMON);$(PRECMD) $(TEST_LD) -o $@ $^ $(TEST_LDPOST)
$(TEST_EXE_ITEST):$(TEST_OFILES_INT) $(TEST_OFILES_COMMON);$(PRECMD) $(TEST_LD) -o $@ $^ $(TEST_LDPOST)

TEST_CFILES_ITEST:=$(filter src/test/int/%,$(TEST_CFILES))
mid/test/int/ma_itest_main.o:$(TEST_TOC)
$(TEST_TOC):$(TEST_CFILES_ITEST);$(PRECMD) etc/tool/mkitesttoc.sh $(TEST_CFILES_ITEST) $@
