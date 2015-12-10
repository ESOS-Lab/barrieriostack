#
# Makefile for nfc devices
#

obj-$(CONFIG_NFC_PN544)		+= pn544/
obj-$(CONFIG_NFC_MICROREAD)	+= microread/
obj-$(CONFIG_SEC_NFC_SENN3AB)	+= secnfc/
obj-$(CONFIG_NFC_PN533)		+= pn533.o
obj-$(CONFIG_NFC_WILINK)	+= nfcwilink.o
obj-$(CONFIG_NFC_MEI_PHY)	+= mei_phy.o
obj-$(CONFIG_SEC_NFC)		+= sec_nfc.o
obj-$(CONFIG_NFC_PN547)		+= pn547.o
obj-$(CONFIG_NFC_P61)		+= p61.o
obj-$(CONFIG_ESE_P3_LSI)		+= ese_p3.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
