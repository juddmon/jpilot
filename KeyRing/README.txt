
This is a J-Pilot plugin program for GNU KeyRing.  KeyRing is a palm
application that stores records with DES3 encryption.  KeyRing may change
its name because it isn't allowed to use GNU in the name.  Just a warning.

KeyRing can be found at:
http://gnukeyring.sourceforge.net

I used OpenSSL for encryption.  You will have to get it and install it on
your own.  Since the U.S.A. exports on encryption have been weakened it
should be freely available and included in new distributions.

There is no autoconf (configure) detection of OpenSSL, so you may have to
edit the Makefile appropriately if its not installed in a standard place.

BUGS:
 There is one major bug that I know of.  When you change passwords on the
 palm KeyRing program it will re-encrypt all the passwords.  If you have
 unsynced records in J-Pilot, they will not get re-encrypted and will be
 garbage.  I could fix this, but its too much work.  Just sync before
 changing your password.

 Sort order isn't the same as on the palm.
 
TODO:
 Update Configure script to detect OpenSSL.
 This requires a newer version of autoconf for me so I didn't do it right
 away.

Judd Montgomery <judd@jpilot.org>
