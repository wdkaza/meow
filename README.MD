# meow
A very lightweight and minimal X window manager made in C, insipired by i3, Fluorite, dwm and ragnarWM.<br>
Minimal, no desktop bloat, no unnecessary features. Just clean.<br><br><br>
![previewNord2](screenshots/flashbang.png)<br>
![preview2](screenshots/darkflashbang.png)<br>
(more screenshots in screenshots folder)
<br>
## Features<br>

- [x] Fullscreen  
- [x] Multiple desktops  
- [x] Multiple tiling layouts  
- [x] Configurable gaps between windows in layouts  
- [x] Easy config  
- [x] Bars support  
- [x] layout IPC module for polybar
- [x] Some EWMH support  
- [x] Xrecourses (recompiling)
- [x] 4 Corner resizing, and overall better feel for floating layout
- [ ] Scratchpads
- [x] Hover with mouse to select windows as an option 
- [x] Swap master stack with slave stack
- [ ] Configuring each slave's height/width individually
- [ ] Multiple monitor support

# Instalation

## 1. Dependencies
 
Arch linux:  
``` sh
sudo pacman -S xorg xdotool libxft libxcomposite libxcursor libxrandr  
```
 
## 2. Building and installing
 
Modify the config-(config.h) to your liking, and after doing that run (with sudo if necessary)  
 
```sh
make install  
```
 
## 3. .xinitrc example, for autostart
 
```sh
[[ -f ~/.Xresources ]] && xrdb -merge -I$HOME ~/.Xresources
setxkbmap -layout en
picom &
exec meow
```

## 4. .Xresources example

```sh
wm.borderWidth: 2
wm.borderFocused: 0xADD8E6
wm.borderUnfocused: 0x1e1e1e
wm.windowGap: 20
```

## Polybar IPC
```sh
[module/meow_layout]
type = custom/ipc
hook-0 = "echo Master"
hook-1 = "echo Cascade"
hook-2 = "echo Horizontal"
hook-3 = "echo Floating"
initial = 1;
```
## General information

If you will encounter any bug or will have any questions feel free to add me at discord 'wdd.'  
Default keybinds are afwul, i would heavily recommend changing them to your own liking   
You can find more details about the confing inside config.h   
 
There is a small issue with steam, you will need to turn off GPU accelerated rendering for it to work properly
