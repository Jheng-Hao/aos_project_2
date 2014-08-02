rm -f logFile.txt

remoteuser=axt122130
remotecomputer1=net41.utdallas.edu
remotecomputer2=net42.utdallas.edu
remotecomputer3=net43.utdallas.edu
remotecomputer4=net44.utdallas.edu
remotecomputer5=net34.utdallas.edu
remotecomputer6=net35.utdallas.edu
remotecomputer7=net36.utdallas.edu
remotecomputer8=net37.utdallas.edu
remotecomputer9=net38.utdallas.edu
remotecomputer10=net39.utdallas.edu

ssh -l "$remoteuser" "$remotecomputer1" "cd $HOME/proj_temp_aos/;./out" &
ssh -l "$remoteuser" "$remotecomputer2" "cd $HOME/proj_temp_aos/;./out" &
ssh -l "$remoteuser" "$remotecomputer3" "cd $HOME/proj_temp_aos/;./out" &
ssh -l "$remoteuser" "$remotecomputer4" "cd $HOME/proj_temp_aos/;./out" &
#ssh -l "$remoteuser" "$remotecomputer5" "cd $HOME/aos/;./node 0 35 36 37 38 39" &
#ssh -l "$remoteuser" "$remotecomputer6" "cd $HOME/aos/;./node 0 36 37 38 39" &
#ssh -l "$remoteuser" "$remotecomputer7" "cd $HOME/aos/;./node 0 37 38 39" &
#ssh -l "$remoteuser" "$remotecomputer8" "cd $HOME/aos/;./node 0 38 39" &
#ssh -l "$remoteuser" "$remotecomputer9" "cd $HOME/aos/;./node 0 39" &
#ssh -l "$remoteuser" "$remotecomputer10" "cd $HOME/aos/;./node 0 34" &
