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


# OpenWRT SDK
CROSS=/i/cross
build/nexx/%.elf: %.c Makefile
	[ -d "$(CROSS)" ]
	mkdir -p $$(dirname $@)
	. $(CROSS)/env-nexx.sh ; mipsel-openwrt-linux-gcc -Os -o $@ $< -I../include -DMAIN=main -Wall -Werror -lusb -Wall -Werror
	file $@

# root@panda:~# apt-get install gcc-6-arm-linux-gnueabi
build/armdeb/%.elf: %.c Makefile
	mkdir -p $$(dirname $@)
	arm-linux-gnueabi-gcc-6 -Os -o $@ $< -I../include -DMAIN=main -Wall -Werror -lusb -Wall -Werror
	file $@



# Create a list of build targets to push.  As a side effect, this
# should rebuild the files if necessary.
push.list: $(ALL)
	echo $(ALL) >$@
# Perform individual file deployment
%.push: %
	[ ! -z "$$DST" ]
	../../apps/exo/priv/ssh/exo_ssh.sh $$DST update bin/$$(basename $*) <$<
