XEPHYR=$(whereis -b Xephyr | cut -f2 -d' ')

xinit ./.xinitrc -- \
  "$XEPHYR" \
  :100 \
  -ac \
  -screen 1920x1080 \
  -host-cursor

