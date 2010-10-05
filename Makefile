
all: dump-fans smc

smc: smc.c smc.h
	gcc -Wall -o smc smc.c -framework IOKit

dump-fans: dump-fans.c
	gcc -std=c99 -Wall -o dump-fans dump-fans.c -framework IOKit


silent:
	# python -c "print hex(4250 << 2)" -> 0x4268
	sudo ./smc -k "FS! " -w 0003
	sudo ./smc -k "F0Tg" -w 4268
	sudo ./smc -k "F1Tg" -w 4268

#test:
#	sudo ./smc -k "F0Tg" -w 4268
#	sudo ./smc -k "F1Tg" -w 4268

noisy:
	sudo ./smc -k "FS! " -w 0000
