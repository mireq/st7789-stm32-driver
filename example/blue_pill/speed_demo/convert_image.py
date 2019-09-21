# -*- coding: utf-8 -*-
import sys
import struct
from PIL import Image, ImagePalette
import numpy as np


OUTPUT_SIZE = (240, 240)


def dither_565(im):
	bands = im.split()

	r_palette = np.array([[(c << 3) + 3 for c in range(32)]])
	g_palette = np.array([[(c << 2) + 1 for c in range(64)]])
	b_palette = np.array([[(c << 3) + 3 for c in range(32)]])
	r_palette = Image.fromarray(r_palette, mode='L').convert('RGB').convert('P', palette=Image.ADAPTIVE, colors=32)
	g_palette = Image.fromarray(g_palette, mode='L').convert('RGB').convert('P', palette=Image.ADAPTIVE, colors=64)
	b_palette = Image.fromarray(b_palette, mode='L').convert('RGB').convert('P', palette=Image.ADAPTIVE, colors=32)
	print(r_palette.palette.colors)

	bands = [bands[0].convert('RGB').convert('P', palette=r_palette.palette), bands[1].convert('RGB').convert('P', palette=r_palette.palette), bands[2].convert('RGB').convert('P', palette=r_palette.palette)]
	bands = [bands[0].convert('L'), bands[1].convert('L'), bands[2].convert('L')]

	return Image.merge('RGB', bands)


def main():
	with open(sys.argv[1], 'rb') as image_fp:
		im = Image.open(image_fp)
		im = im.convert('RGB')

	im.thumbnail(OUTPUT_SIZE, Image.ANTIALIAS)

	with open(sys.argv[1] + '.dat', 'wb') as image_fp:
		packed_pixel = struct.Struct("<H")
		pixels = list(im.getdata())
		for pixel in pixels:
			r, g, b = pixel
			r = r >> 3
			g = g >> 2
			b = b >> 3
			pixel = (r << 11) + (g << 5) + (b)
			image_fp.write(packed_pixel.pack(pixel))



if __name__ == "__main__":
	main()
