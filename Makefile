.PHONY = all check test assets final clean release
all: check test assets final
main:
	gcc -Wall -Wpedantic -o main main.c -lm
# check the RGB values of transparent pixels in the input textures
check: main
	./main --alpha-threshold 4 --make-transparent-opaque textures/in textures/check
# test what the RGB values of transparent pixels will be in the output
test: main
	./main --alpha-threshold 4 --recolorize-transparent --make-transparent-opaque textures/in textures/test
# create the assets for the readme
assets: main
	./main --alpha-threshold 4 --make-transparent-opaque textures/in/1x/Enhancers.png assets/Enhancers_check.png
	./main --alpha-threshold 4 --recolorize-transparent --make-transparent-opaque textures/in/1x/Enhancers.png assets/Enhancers_test.png
	./main --alpha-threshold 4 --recolorize-transparent textures/in/1x/Enhancers.png assets/Enhancers_final.png
# produce the final output, correcting the RGB values of transparent pixels
final: main
	./main --alpha-threshold 4 --recolorize-transparent textures/in textures/final
release:
	gcc -Wall -Wpedantic -O3 -o trc main.c -lm
clean:
	rm main
