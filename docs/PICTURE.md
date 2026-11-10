# The picture

## Widescreen

The fighting games can be shown and played in **16:9**: the picture is wider at the same height, with everything
the same size, so more of the stage is in view. The extra columns are the game's own stage art, which the board
already holds and never showed; nothing is stretched. It's a switch on the launcher's first page under "Full screen",
and on the in-game menu's Display page. It's **off unless you switch it on**, because it changes how the game plays:
the players can go to the wide picture's edges.

![Widescreen in Street Fighter Alpha 2: Ryu and Ken at Sagat's stage, more of it on each side](screenshots/sfa2/widescreen.png)

**Invisible walls**, on the Display page, keeps the 16:9 picture and plays the game exactly as the board does: the
players are stopped where the board's picture ends, by walls that aren't seen.

## Display

The Display page (in the launcher and the in-game menu) says how the picture fits the window or screen:

| Row | Choices |
| --- | --- |
| Scaling | Sharp (each pixel a crisp block), Whole pixels (every pixel the same whole number of the screen's pixels tall, a border round it), Smooth |
| Picture shape | 4:3 (as the arcade's monitor showed it), Square pixels, Fill |
| Shader | Off, or one of RetroArch's shaders (below) |
| Window size | 3x the board's 224 lines, or 2x to 6x |

## Shaders

The picture through one of RetroArch's shaders, by
[librashader](https://github.com/SnowflakePowered/librashader): a curved CRT, CRT Lottes, CRT Royale, CRT Guest or
a light CRT; scanlines light, strong, as an OSSC draws them, or dark; or xBRZ and ScaleFX to smooth the pixels'
edges (the Display page's **Shader**). **Your own** takes any RetroArch slang preset (`.slangp`) you put in a
`shaders` folder beside the game's settings, with the files it names (a few need a newer OpenGL than macOS has; Linux and Windows have it).
Under Shader on the launcher's Features page, **In the game's menu** → **Choose** keeps the in-game menu's list to
your favourites. Zoomed in, the camera closes in on the shaded picture, scanlines and all, as on a monitor. The menu is drawn over the picture without the shader. A shader that cannot be made leaves the
picture as it was.

| | | |
| --- | --- | --- |
| ![No shader](screenshots/shaders/off.png)<br>Off | ![CRT (crt-geom-mini)](screenshots/shaders/crt-geom-mini.png)<br>CRT | ![CRT Royale](screenshots/shaders/crt-royale.png)<br>CRT Royale |
| ![CRT Lottes](screenshots/shaders/crt-lottes.png)<br>CRT Lottes | ![Scanlines, light](screenshots/shaders/scanline-fract.png)<br>Scanlines, light | ![xBRZ](screenshots/shaders/xbrz.png)<br>xBRZ |

## The camera's zoom

In the fighting games, hold the left stick's click and press the left shoulder to zoom in on player 1's fighter,
the right shoulder for player 2's (keyboard: hold Left Shift and press Q or E). Each press goes a size up, 1.25x to
2x, then back to the whole picture. With the click held, the d-pad moves the camera; the right stick's click
(keyboard: Left Shift and W) keeps **both fighters** in the picture instead, as close as shows both whole. The zoom is
the window's alone: the game plays the same.
