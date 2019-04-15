# Pete-Software-Render-DX9
A port of Pete Op.S. Software Render from the old DirectDraw interface to the more modern and capable DirectX9.

## Why was this ported to DirectX9?
If you loved this plugin back in the day, you would know what it is capable of and how it is perfect for 2D games or for general simulation of a CRT television. With advancements in GPU technology, the underlying DirectDraw interface used for this became unstable and resorted to emulation for the most, causing incompatibilities, weird behaviour, and loss of hardware features such as filtered framebuffer. With DirectX9 most of these issues simply go away and it's much easier to maintain the code compatible on several cards with a more generic approach.

## What was improved from the original?
This new edition of the Pete Op.S. Software Driver adds a few features of its own that make visual rendintion a bit better in general.
* Stretch mode would stretch the image by maintaining aspect ratio was fundamentally broken for games using a hi-res non-interlaced mode, resulting in the image not filling a portion of screen that it was intended to take care of. This was fixed by adding a forced 4:3 aspect ratio whenever the uses chooses to do so. The original ratio option is still there if one wanted to use it anyway.
* Rewritten the code that takes screenshots. Now it captures only what's on screen at native resolution, with no overlay elements, and in PNG for glorious compression.

## What was dropped in the process?
* All the old CPU-based upscale algorithms have been dropped entirely, not only because they are fundamentally slower, but because they can be done identically by using shaders. While these shaders haven't been implemented, one could use a more general approach like ReShader to achieve similar if not better effects, with way better performance stability too since these effects would be performed entirely on the GPU which is more capable than the CPU to process images.
* Fullscreen mode has been dropped replaced with a borderless window mode.

## Intended updates
* Add a mode to select screen display filtering.
* Drop the deprecated Video For Windows record module. There are better external alternatives such as OBS, which can also record audio and are generally faster.
