gifplay: *.c
	gcc *.c `pkg-config --cflags --libs mmm` -o mmm-gifplay -O3
clean:
	rm -f mmm-gifplay
install: mmm-gifplay
	cp mmm-gifplay /usr/local/bin
