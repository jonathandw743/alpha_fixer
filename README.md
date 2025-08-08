# transparency_recolorizer

Recolorizes transparent pixels in textures so that interpolation between opaque and transparent pixels produces the correct result.

See `Makefile` and `--help` for instructions.

## Examples

### Original texture

![Original texture](assets/Enhancers.png)

### Texture with all the transparent pixels set to opaque (before)

![Texture with all the transparent pixels set to opaque (before)](assets/Enhancers_check.png)

### Texture with all the transparent pixels recolorized and set to opaque (after)

![Texture with all the transparent pixels set recolorized and set to opaque (after)](assets/Enhancers_test.png)

### Texture with all the transparent pixels recolorized and set to transparent (should look the same as the original)

![Texture with all the transparent pixels set recolorized and set to transparent (should look the same as the riginal)](assets/Enhancers_final.png)
