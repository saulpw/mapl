test: mapl
	echo '[ 2 3 [ 2 1 + .' | ./mapl

mapl: main.c
	gcc -o $@ $<
