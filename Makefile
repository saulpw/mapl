test: mapl
	echo '[ 2 3 [ 2 1 +' | ./mapl
	echo '[ 5 iota [ 5 iota +' | ./mapl
	echo '[ 5 DUP . iota [ 5 iota +' | ./mapl

mapl: main.c
	gcc -ggdb -o $@ $<

clean:
	rm mapl
