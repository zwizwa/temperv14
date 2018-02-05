all: temperv14.elf
%.elf: %.c
	gcc -Os -o $@ $< -lusb -Wall -Werror
	strip $@

install:
	chown root:root temperv14.elf
	for host in zoo.zoo lroom.zoo broom.zoo zoe.zoo; do rsync.clone temperv14.elf $$host:/usr/local/bin; done



