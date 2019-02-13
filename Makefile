.PHONY: all

# FIXME: only do host here.  Other targets need to use the $(TARGET)
# mechianism.  See exo/target/Makefile for an example.

ALL := $(patsubst %,build/%/temperv14.elf,host nexx)
all: $(ALL)


.PHONY: clean
clean:
	rm -rf build flycheck.env

# Native build with gcc in path.
build/host/%.elf: %.c
	mkdir -p $$(dirname $@)
	gcc -Os -o $@ $< -lusb -Wall -Werror
	strip $@

# Abstract cross-compilation.  Top level Makefile will set TARGET and
# TARGET_BUILD to provide a build mechanism.  We call that with
# proper C flags.
build/$(TARGET)/%.elf: %.c
	[ ! -z "$(TARGET_BUILD)" ]
	mkdir -p $$(dirname $@)
	$(TARGET_BUILD) gcc -Os -o $@ $< -lusb -Wall -Werror
	$(TARGET_BUILD) strip $@





# Interfaces to exo build system.
# FIXME: make this minimal.

# 1. Continuous syntax checking using flycheck.

# For emacs flycheck setup (FIXME: decouple this through .push_change)
flycheck.env: Makefile
	echo "FLYCHECK_GCC=\"gcc $(CFLAGS) -I.\"" >$@


# 2. Incremental updates.  This needs some work.

# Create a list of build targets to push.  As a side effect, this
# should rebuild the files if necessary.
push.list: $(ALL)
	echo $(ALL) >$@
# Perform individual file deployment
%.push: %
	[ ! -z "$$DST" ]
	../../apps/exo/priv/ssh/exo_ssh.sh $$DST update bin/$$(basename $*) <$<

