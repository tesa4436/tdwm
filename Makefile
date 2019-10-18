all : tdwm
tdwm : tdwm.c
	gcc tdwm.c -o tdwm -lxcb-icccm -lxcb -Wall -Wextra -pedantic-errors -std=c99 -g
clean :
	rm tdwm
assembly:
	gcc tdwm.c -o tdwm.s -lxcb-icccm -lxcb -Wall -Wextra -pedantic-errors -std=c99 -S 
