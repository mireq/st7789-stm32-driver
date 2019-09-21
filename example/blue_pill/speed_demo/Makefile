-include ../Makefile.template


run: $(TARGET).elf image.png.dat
	$(GDB) --command=run.gdb -nh -q --batch $(TARGET).elf


example/image.png.dat: image.png
	python example/convert_image.py example/image.png
