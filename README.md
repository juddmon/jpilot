### What J-Pilot is
J-Pilot is a PalmOS device desktop for Linux/Unix.  The latest version is 1.8.2
### J-Pilot plugins
J-Pilot has a few plugins written for it.  They allow jpilot to have an interface and sync with Palm apps.  
[Manana page](http://bill.sexton.tripod.com/download.htm)  
[KeyRing page](http://gnukeyring.sourceforge.net)

### Ubuntu dialout user
In ubuntu you must be a member of the dialout group in order to sync over USB.  Users will not be in this group by default.  If you are using serial ports see the README file.
```shell
sudo usermod -a -G dialout $USER
```
### Environment variables
J-Pilot uses the JPILOT_HOME environment variable to make it easy to allow multiple pilots to be synced under the same unix user.  Just set JPILOT_HOME to the directory you want jpilot to use.  For example, I have 2 palm pilots.  I can sync the one I use all the time into /home/judd.  The other one I can sync into /home/judd/palm2 by using this script:
```shell
#!/bin/bash
JPILOT_HOME=/home/judd/palm2
jpilot
```
### Syncing
Most users find it easiest to sync by pressing the sync button on the PalmOS device and then within a second or a few pressing the sync button on jpilot.

### BACKUP and SYNC
Just a warning that a sync DOES NOT backup your palm.  
The Sync button will sync the 4 applications with the palm pilot and any plugins that are installed.  
  
The Backup button will backup every program and database from the palm pilot, except for AvantGo files (these are usually big and change daily).  
  
If you get an error saying that you have a NULL user ID, then you need to run install-user from the pilot-link suite.  
```shell
install-user /dev/pilot Judd 1234
```
Of course replace "Judd" and "1234" with you favorite name and number.
