/**
* Dec, 30, 2015
*
* Dam Quang Tuan
*/
/**********************************************************************
* This file is to define the API for fbarrier() and fdatabarrier() rountines
* 
*
*/

int fbarrier(int fd) {
	return syscall(314, fd);
}

int fdatabarrier(int fd) {
	return syscall(315, fd);
}
