CFLAGS=-ggdb -fno-inline

mapl: main.c mpl.h
	gcc $(CFLAGS) -o $@ $<

test: out-test.log

out-test.log: mapl tests.mapl
	./mapl kernel.mpl tests.mapl | tee $@
	./mapl kernel.mpl euler-01.mpl

clean:
	rm -f mapl

.PHONY: test clean
