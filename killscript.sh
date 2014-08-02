
remoteuser=axt122130

remotecomputer1=net41.utdallas.edu
remotecomputer2=net42.utdallas.edu
remotecomputer3=net43.utdallas.edu
remotecomputer4=net44.utdallas.edu

ssh -l "$remoteuser" "$remotecomputer1" "killall out" &
ssh -l "$remoteuser" "$remotecomputer2" "killall out" &
ssh -l "$remoteuser" "$remotecomputer3" "killall out" &
ssh -l "$remoteuser" "$remotecomputer4" "killall out" &

