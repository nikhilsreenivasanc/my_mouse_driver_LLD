savedcmd_/home/xeedo/LLD_N/My_mouse/my_mouse.mod := printf '%s\n'   my_mouse.o | awk '!x[$$0]++ { print("/home/xeedo/LLD_N/My_mouse/"$$0) }' > /home/xeedo/LLD_N/My_mouse/my_mouse.mod
