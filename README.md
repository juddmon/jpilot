### What J-Pilot is
J-Pilot is a PalmOS device desktop for Linux/Unix.  The latest version is 2.1.0
### J-Pilot plugins
J-Pilot has a few plugins written for it.  They allow jpilot to have an interface and sync with Palm apps.
- [Pcs&Videos](https://github.com/danbodoh/picsnvideos-jpilot)
- [Manana page](http://bill.sexton.tripod.com/download.htm)  
- [KeyRing page](http://gnukeyring.sourceforge.net)

### Ubuntu install for 20.04 (focal) and 22.04 (jammy)
This will install the package and create an apt source so that it will be updated when new code is released.
```shell
curl -s https://packagecloud.io/install/repositories/judd/jpilot/script.deb.sh | sudo bash
sudo apt install jpilot jpilot-plugins
```

### Communication Permissions

#### The short version for Debian based distros and many others

Make sure your user is in the plugdev group.  Note: This will require you to logout and back in for the changes to take effect.
```shell
sudo usermod -a -G plugdev $USER
```

Create a file like `/etc/udev/rules.d/60-palm.rules` if one does not exist, containing:
```
# Give the Palm's USB device node a group the user is a member of.
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{idVendor}=="0830", MODE="0660", GROUP="plugdev"
```
Then to ask udev to read the rules and re-apply them:
```shell
sudo udevadm control --reload-rules
sudo udevadm trigger
```
Use the port 'usb:' for pilot-link and jpilot to communicate over.

#### A more detailed explanation

##### Choose to use libusb or the visor module
There are two different ways for pilot-link to reach a USB Palm, and they
use different sync ports:

- **libusb — the modern way, and what you should use.**  A pilot-link built
  against libusb talks to the USB device directly, with no serial device
  node involved.  The sync port is the literal string `usb:`, which is
  J-Pilot's default.
- **The `visor` kernel module — deprecated long ago.**  This is the old
  usb-serial approach: the module presents the Palm as a serial port and
  the sync port is a device node such as `/dev/ttyUSB0`, `/dev/ttyUSB1`,
  and so on.  It is only worth using on old systems where libusb is not
  an option.

The sync port is set in J-Pilot under File -> Preferences -> Settings, in
the "Serial Port" field.

##### Giving your user access to the Palm with a udev rule (libusb)
This is the part that matters when you sync with `usb:`.  Because libusb
talks to the raw USB device instead of a serial port, being in the
`dialout` group does not help: `dialout` only covers `/dev/tty*` nodes.
The raw USB device node that libusb opens (under `/dev/bus/usb/`) is owned
by `root` by default, so a normal user cannot open it and the sync fails
with a permission error.  A udev rule fixes this by giving that node a
group you belong to every time the Palm connects.  The rule goes in
`/etc/udev/rules.d/60-palm.rules`.

First find out which groups you are actually a member of, so you can pick
one that already applies to you:
```shell
id
```
This prints something like:
```
uid=1000(judd) gid=1000(judd) groups=1000(judd),20(dialout),46(plugdev)
```
The names in `groups=` are the ones you may use.  In this example either
`dialout` or `plugdev` would work and plugdev is the preferred one.  Remember this group.

If you need to add yourself to a group you can do so with the below command.  Note: This will require you to logout and back in for the changes to take effect.
```shell
sudo usermod -a -G plugdev $USER
```

Find your device's vendor id by running `lsusb` while the Palm is
connected and syncing; Palm, Inc. devices use `0830`, but Handspring, Sony
and other makers each have their own id.  Then create
`/etc/udev/rules.d/60-palm.rules` containing:
```
# Give the Palm's USB device node a group the user is a member of.
SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device", ATTR{idVendor}=="0830", MODE="0660", GROUP="plugdev"
```
Replace `GROUP="plugdev"` with whichever group you remembered from the `id` command above.  Replace the `ATTR{idVendor}` value with your device's id if it is not a Palm.

Then to ask udev to read the rules and re-apply them:
```shell
sudo udevadm control --reload-rules
sudo udevadm trigger
```
The Palm only appears on the USB bus while a HotSync is actually running,
so press HotSync on the device and then check the node's ownership.  Use
`lsusb` to get the bus and device numbers, then list that node:
```shell
lsusb
ls -l /dev/bus/usb/001/007
```
It should now be group-owned by the group you chose.

##### If you use the visor module instead
The `visor` module is deprecated and is not needed for a libusb build of
pilot-link.  If you do use it (check whether it is
loaded with `lsmod | grep visor`), the Palm is presented as one or more
serial ports, `/dev/ttyUSB0`, `/dev/ttyUSB1` and so on, and you set the
sync port to one of those rather than to `usb:`.  Which node carries the
HotSync connection can vary, so if the first does not connect, try the
next one.  These are ordinary tty nodes, so it is membership in the
`dialout` group that grants access to them, not `plugdev` — the USB rule in
the previous section does not apply.  Add yourself to it with (again, you
must logout and back in for this to take effect):
```shell
sudo usermod -a -G dialout $USER
```

Because the number can change from sync to sync, the classic pilot-link
convention is a `/dev/pilot` symlink pointing at whichever node the Palm
gets, which you can create with a udev rule on the tty:
```
SUBSYSTEM=="tty", ATTRS{idVendor}=="0830", GROUP="dialout", MODE="0660", SYMLINK+="pilot"
```
`/dev/pilot` is not in J-Pilot's port drop-down, so select "other" and
type it in.  Command-line pilot-link tools accept it the same way:
```shell
pilot-xfer -p /dev/pilot -l
```

### Environment variables
J-Pilot uses the JPILOT_HOME environment variable to make it easy to allow multiple pilots to be synced under the same unix user.  Just set JPILOT_HOME to the directory you want jpilot to use.  For example, I have 2 palm pilots.  I can sync the one I use all the time into /home/judd.  The other one I can sync into /home/judd/palm2 by using this script:
```shell
#!/bin/bash
JPILOT_HOME=/home/judd/palm2
jpilot
```
### Syncing
Some users find that timing matters for getting the Palm to start syncing.  It seems to succedd more often by pressing the sync button on the PalmOS device and then within a second or two pressing the sync button in jpilot.

### BACKUP and SYNC
Just a warning that a sync DOES NOT backup your palm.  
The Sync button will sync the 4 applications with the palm pilot and any plugins that are installed.  
  
The Backup button will backup every program and database from the palm pilot, except for AvantGo files (these are usually big and change daily).  
  
If you get an error saying that you have a NULL user ID, then you need to run install-user from the pilot-link suite.  On Debian and Ubuntu it is installed as `pilot-install-user`.
```shell
pilot-install-user -p usb: -u "Judd" -i 12345
```
Of course replace "Judd" and "12345" with you favorite name and number.  PalmOS requires the user ID to be a 5-digit number.  Use the same port here that you use for syncing, so `/dev/ttyUSB1` (or whichever node) instead of `usb:` if you are using the visor module.

### Building a Debian (or ubuntu) package
From the repo root, with the tree already configured (e.g. after ./configure):

1. Ensure debian/changelog has an entry for the version in configure.in (e.g. 2.1.0-1).
2. Run:
   ./build-debian.sh
3. Collect the built artifacts in the current directory:
	jpilot-1.8.2.tar.gz
	jpilot-1.8.2.tar.gz.asc
	jpilot-1.8.2.tar.gz.md5sum
	jpilot-2.1.0.tar.gz
	jpilot-dbgsym_2.1.0-1_amd64.ddeb
	jpilot-plugins-dbgsym_2.1.0-1_amd64.ddeb
	jpilot-plugins_2.1.0-1_amd64.deb
	jpilot_2.1.0-1.debian.tar.xz
	jpilot_2.1.0-1.dsc
	jpilot_2.1.0-1_amd64.buildinfo
	jpilot_2.1.0-1_amd64.changes
	jpilot_2.1.0-1_amd64.deb
	jpilot_2.1.0.orig.tar.gz

#### For a new upstream version
1. update the jpilot version in configure.ac in the AC_INIT function.
2. Add a change in debian/changelog
3. run ./build-debian.sh.
