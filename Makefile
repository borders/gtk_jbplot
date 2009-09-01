all: jbplot

jbplot: jbplot.c jbplot.h main.c jbplot-marshallers.c jbplot-marshallers.h
	gcc -g -o jbplot jbplot.c main.c jbplot-marshallers.c \
		`pkg-config --libs --cflags gtk+-2.0`

jbplot-marshallers.c: jbplot-marshallers.list
	glib-genmarshal --prefix _plot_marshal --body $< > $@

jbplot-marshallers.h: jbplot-marshallers.list
	glib-genmarshal --prefix _plot_marshal --header $< > $@

clean:
	rm -f jbplot-marshallers.h jbplot-marshallers.c
	rm -f jbplot
