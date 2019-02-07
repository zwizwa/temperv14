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

# .PHONY: install
# install:
# 	chown root:root temperv14.elf
# 	for host in zoo.zoo lroom.zoo broom.zoo zoe.zoo; do rsync.clone temperv14.elf $$host:/usr/local/bin; done

# For emacs flycheck setup (FIXME: decouple this through .push_change)
flycheck.env: Makefile
	echo "FLYCHECK_GCC=\"gcc $(CFLAGS) -I.\"" >$@

# Create a list of build targets to deploy.  As a side effect, this
# should rebuild the files if necessary.
deploy.list: $(ALL)
	echo $(ALL) >$@
# Perform individual file deployment.
%.deploy: %
	[ ! -z "$$DST" ]
	../../ssh/exo_ssh.sh $$DST update bin/temperv14.elf <$<


# OpenWRT SDK
CROSS=/i/cross
build/nexx/%.elf: %.c Makefile
	[ -d "$(CROSS)" ]
	mkdir -p $$(dirname $@)
	. $(CROSS)/env-nexx.sh ; mipsel-openwrt-linux-gcc -Os -o $@ $< -I../include -DMAIN=main -Wall -Werror -lusb -Wall -Werror
	file $@
