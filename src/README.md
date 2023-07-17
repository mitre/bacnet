# BACnet Plugin Code

Where possible, the CALDERA for OT plugins leverage open-source libraries and payloads, unifying their exposure through the Caldera Adversary Emulation framework.

* The BACnet plugin leverages the open-source protocol stack [BACnet Stack](https://github.com/bacnet-stack/bacnet-stack/tree/bacnet-stack-1.0) - version 1.0.

* The BACnet stack is licensed with the [GPL2 License](https://github.com/bacnet-stack/bacnet-stack/blob/master/license/gpl-2.txt)

* The command-line protocol service binaries produced by building BACnet Stack are used as their respective ability payloads.

## Specific Code Modifications

The Caldera for OT BACnet Plugin primarily uses unchanged default payloads from the BACnet stack. However, two payloads were modified from the BACnet stack and rebuilt specifically for this plugin. All modifications made to the original source are detailed below.

**Modified files:** <br>
* [bacnet-stack/apps/readprop/main.c](https://github.com/bacnet-stack/bacnet-stack/tree/bacnet-stack-1.0/apps/readprop)
* [bacnet-stack/apps/writeprop/main.c](https://github.com/bacnet-stack/bacnet-stack/tree/bacnet-stack-1.0/apps/writeprop)

### **Read Prop Changes:**
* Change 1: Reformatted code – unfolded it to make editing easier
* Change 2: [L506](https://gitlab.mitre.org/caldera/ot/plugins/bacnet-caldera/-/blob/code-review/src/bacnet-stack/apps/readprop/main.c#L506)
* Using IP[:Port] or MAC address in-place of the Device-ID will
    1.  Create a fake entry that maps Device-ID 1337 to the provided IP/MAC
    2. Set the Target_Device_Object_Instance to 1337
    * 1+2  allows skipping the WhoIs/IAm resolution process.
        * bacnet-stack may be able to take an input file that stores prior resolutions – but that was avoided to reduce files on the target system.
* Change 3: [L541](https://gitlab.mitre.org/caldera/ot/plugins/bacnet-caldera/-/blob/code-review/src/bacnet-stack/apps/readprop/main.c#L541)
*	Passing index=-2 will cause
    1. The length of the object property (list type) to be read
    2. All indices of the object property (list type) to be read 1-by-1 in ReadProp requests
    * 1+2 allows reading arbitrarily long lists
        * Without requiring the device to support segmentation (seg occurs when packet exceeds MAX_APDU)
* Disabled optional args --mac, --dnet, --dadr and tied the destination network to local connection when mac is specified.

### **Write Prop Changes:**
* Change 1: Reformatted code – unfolded it to make editing easier
* Change 2: [L391](https://gitlab.mitre.org/caldera/ot/plugins/bacnet-caldera/-/blob/code-review/src/bacnet-stack/apps/writeprop/main.c#L391)
*	Using IP[:Port] or MAC address in-place of the Device-ID will
	1. Create a fake entry that maps Device-ID 1337 to the provided IP/MAC
	2. Set the Target_Device_Object_Instance to 1337
	* 1+2 allows skipping the WhoIs/IAm resolution process.
        * bacnet-stack may be able to take an input file that stores prior resolutions – but that was avoided to reduce files on the target system.

## **Reproducing Builds**

How to compile the binaries present in the plugin’s payloads folder.

1.	Download BACnet Stack [1.0](https://github.com/bacnet-stack/bacnet-stack/tree/bacnet-stack-1.0)
2.	Copy readprop and writeprop main.c files to their respective destinations under /apps/
3.	Compile binaries

As noted in ["Demo BACnet Applications"](https://bacnet.sourceforge.net/), to build the applications with MinGW tools on Windows, you may need to "copy the `mingw-make.exe` to `make.exe` to enable the compile."