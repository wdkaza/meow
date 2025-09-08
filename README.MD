# meow
A very lightweight and minimal X window manager made in C, insipired by i3, dwm and ragnarWM.<br>
Minimal, no desktop bloat, no unnecessary features. Just clean.<br><br><br>
![preview1](screenshots/2025-09-02-193404_1920x1080_scrot.png)<br>
![preview3](screenshots/2025-08-12-014816_1920x1080_scrot.png)<br>
![preview4](screenshots/2025-08-13-164111_1920x1080_scrot.png)<br>
![preview5](screenshots/2025-08-06-214317_1920x1080_scrot.png)<br>
<br><br><br>
## Features<br>
So far meow has support for:<br>
- [x] Fullscreen<br>
- [x] Tiling layouts(just one currently)<br>
- [x] Window borders<br>
- [x] Gaps between windows in a layout<br>
- [x] Better config
- [ ] Support for other bars(example polybar, etc), (HIGHEST PRIORITY)
- [ ] Cleaner readme
- [ ] EWMH support
- [ ] Hover to select windows<br>
- [ ] Multiple monitors<br><br><br><br>

built in bar was scraped due to complexity being the same as window manager to make a good one
also it was coded very very poorly lol

## Instalation
<br>

⚠️It still has alot of bugs i would like to fix them before providing a way to install it⚠️<br>
For example if you close all windows on any desktop that is not "0" you will just lose control of the WM<br>
But if you'd like to see this project get finished someday give it a star and i will try my best to finish it⭐

I want to mention github.com/cococry/ragnar as ragnar's old code helped me understand alot of things,<br>
Make sure to check it out aswell!<br><br>
(but if you are really curious, you can run it with "startx ./meow", will provide a detailed guide a bit later when im sure meow is stable)
## Bugs

- [x] Launching firefox(maybe any window) has a small chance to spawn a 0,0 ignoring the wm(temp fix : force quit with ctrl+q and it only happends on startup for me sometimes)
- [ ] Picom doesnt detect fullscreen, moving up/down layout
- [ ] Firefox permission popup will sometimes spawn behind the window and firefox window needs to be focused to close it
- [x] Opening a sub window? maybe thats what its called of a window, example in discord [add file] will not map it(thunar) correctly for some reason
- [ ] Fullscreening should probably unmap other windows, looks weird with transparancy
- [ ] Bar is ofset sligthly to the left by a few pixels
- [ ] Weird glitch with tiling layout top offset after removing bar(may be fixed, only happend once)
- [ ] TODO. Will be adding more bugs that i encounter later, should have started keeping track of that earlier
