eval "$(dbus-launch --sh-syntax)"
setxkbmap us &
kitty &
nitrogen --restore &
exec ./meow
