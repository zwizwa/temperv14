.PHONY: all
ALL := $(patsubst %,build/%/temperv14.elf,host nexx)
all: $(ALL)


.PHONY: clean
clean:
	rm -rf build flycheck.env


build/host/%.elf: %.c
	mkdir -p $$(dirname $@)
	gcc -Os -o $@ $< -lusb -Wall -Werror
	strip $@




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


# 3. Cross compilation

# Delegate to caller-specified build tools.  We just supply compiler flags here.
# See ~/exo/target/Makefile, rule for armdeb
build/$(TARGET)/%.elf: %.c
	[ ! -z "$(TARGET_BUILD)" ]
	mkdir -p $$(dirname $@)
	$(TARGET_BUILD) gcc -Os -o $@ $< -lusb -Wall -Werror
	$(TARGET_BUILD) strip $@

# FIXME: use the abstraction above to do just this.
# OpenWRT SDK
CROSS=/i/cross
build/nexx/%.elf: %.c Makefile
	[ -d "$(CROSS)" ]
	mkdir -p $$(dirname $@)
	. $(CROSS)/env-nexx.sh ; mipsel-openwrt-linux-gcc -Os -o $@ $< -I../include -DMAIN=main -Wall -Werror -lusb -Wall -Werror
	file $@


