eval "$(dbus-launch --sh-syntax)"
setxkbmap us &
nitrogen --restore &
exec ./meow
