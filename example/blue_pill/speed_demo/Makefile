-include ../Makefile.template


run: $(TARGET).elf image.png.dat
	$(GDB) --command=run.gdb -nh -q --batch $(TARGET).elf


image.png.dat: image.png
	python convert_image.py image.png
