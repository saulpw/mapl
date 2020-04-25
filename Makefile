test: mapl
	echo '2 3 + .' | ./mapl

mapl: main.c
	gcc -o $@ $<
