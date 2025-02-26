memtest: memtest.c dlmalloc.c dlplus_malloc.c
	gcc -o memtest memtest.c dlmalloc.c dlplus_malloc.c
#-g -fsanitize=address 

clean:
	rm -f memtest
