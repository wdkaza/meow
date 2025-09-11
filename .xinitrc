eval "$(dbus-launch --sh-syntax)"
setxkbmap us &
nitrogen --restore
kitty &
export _JAVA_AWT_WM_NONREPARENTING=1
exec ./meow
