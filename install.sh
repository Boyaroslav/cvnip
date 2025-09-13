#!/bin/bash

cmake .
make
g++ cvdaemon.cpp -o cvdaemon -lX11
mkdir -p /home/$USER/.local/bin/cvnip
mkdir -p /home/$USER/.local/share/cvnip
chmod +x cvnip
chmod +x cvdaemon
chmod +x run_cvdaemon.sh

sed -e "s|@DISPLAY@|$DISPLAY|g" \
    -e "s|@XAUTHORITY@|$XAUTHORITY|g" \
    cvdaemon.service.in > /home/$USER/.config/systemd/user/cvdaemon.service

mv cvdaemon /home/$USER/.local/share/cvnip/
cp run_cvdaemon.sh /home/$USER/.local/share/cvnip/

touch /tmp/bufcvnip.sock
chmod 666 /tmp/bufcvnip.sock

systemctl --user enable cvdaemon
systemctl --user start cvdaemon

sudo mv cvnip /usr/bin/