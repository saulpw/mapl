test-golden.txt: mapl tests.mapl
	cat tests.mapl | ./mapl | tee $@
	git diff tests-golden.txt

mapl: main.c
	gcc -ggdb -o $@ $<

clean:
	rm -f mapl
