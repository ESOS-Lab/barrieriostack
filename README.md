Barrier-Enabled IO Stack for Flash Storage
-----
* Maintainer : Joonetaek Oh(na94ojt), Gyeongyeol Choi(chl4651)
* Contributor : Jaemin Jung, Hankeun Son, Seongbae Son, Youjip Won

Barrier-Enabled IO Stack is an IO Stack that uses the new command for flash storage, the write barrier command.
The write barrier command allows the host to control the persist order without the expensive command like flush.
We overhaul three of moduless in linux IO stack: the filesystem, the IO scheduler, and the dispatch modules.

Publication
-----
* Youjip Won, Jaemin Jung, Gyeongyeol Choi, Joontaek Oh, Seongbae Son, Jooyoung Hwang, Sangyeun Cho “Barrier Enabled IO Stack for Flash Storage”, USENIX FAST 2018
