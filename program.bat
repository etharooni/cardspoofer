avr-g++ -g -Os -mmcu=atmega328 -c cardspoofer.c
avr-gcc -g -mmcu=atmega328 -o cardspoofer.elf cardspoofer.o
avr-objcopy -O ihex cardspoofer.elf cardspoofer.hex