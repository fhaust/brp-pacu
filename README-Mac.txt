BRP-PACU for Mac OS X (intel architecture only)

Web site
_______________________________________
http://sourceforge.net/projects/brp-pacu

Dependencies
_______________________________________
You should first download and install:

Jack-OSX from
http://www.jackosx.com/

The GTK-OSX Framework from:
http://www.gtk-osx.org/

Instructions
_______________________________________
Open JackPilot from the Application folder. You will find it at /Applications/Jack/JackPilot.app

You should first read the Jack-OSX manual. It will be found under the Help menu of the JackPilot or at
/Applications/Jack/JackPilot.app/Contents/Resources/jackdoc.pdf

Configure JACK according to the Special Intel Mac instructions on page 12 - 13. If you don't do this, BRP-PAC will crash with the message "no physical capture ports".

Now you can start the BRP_PACU application by double-clicking on its icon.
The console output can be seen by opening /Applications/Utilities/Console.app

Never before asked questions
-------------------------------------------
What is BRP-PACU?
- It is an analysis tool to configure any sound system with an equalizer.  It compares the
   output of the system to the input of the system and allows you to use this data to 
   perform final equalization by matching the results with actual DSP/EQ.

How do you do this?
- There are many tutorials on doing this, but it takes practice, experience, and a good 
   ear.  You could try: http://www.sounddevices.com/notes/usbpre/usbpre-with-sia-smaart-software/
   its a brief introduction to the concept, although the USBPre is not supported by linux as of writing this,
   so you couldn't use it with this software.  Try the M-Audio usb preamp, its what I use.  You
   also need a decent omnidirectional calibrated mic (reasonably flat 20Hz-20kHz).

Isn't this software basically like what XYZ company sells for $1100?
- Yes

Why are you giving it away for free?
- There currently is no Linux port for any of these programs and I needed one.  Also I believe 
   anyone who knows how to perform the transfer function properly on an audio system should be 
   able to without selling your soul with some ridiculous EULA and forking over $1100.  The 
   barrier to entry in any market should be knowledge and skill, not overpriced tools.  There are many
   markets already headed in this direction.

What if I try it and it doesn't work?
- Email me and I can probably figure it out.  My email is lm317t@gmail.com.  Also try the mailing list, 
   it should be working.

Why does it segfault?
- Make Sure the gui.glade file is in /usr/local/share/BRP-PACU/ or /usr/share/BRP-PACU/ 

Why is there no documentation?
- Channel 1 is the output of the system we are observing (measured), channel 2 is the input (reference).  
   You also need a decent calibrated omnidrectional microphone (reasonably flat 20Hz-20kHz) and either a
   USB-Preamp like the 2-channel M-Audio, or a mixer with a 30dB pad or so going into the laptop mic input.  No Pad is
   neccessary if you are lucky enough to have a laptop or PC with a line input.  The headphone out will have the pink noise.

How do you handle a delay between measured and referenced signals?
- There is an automatic delay finding button.  You can also insert a custom delay if you want.

Is there a way to find the impulse response?
- It is in the menu under action

Why do some menu items do nothing?
- I have not implemented New, Save, Save As, View, or Open yet.  If you would like to help you
   can write some of them.  Please email me.  Otherwise please wait.

How do I change the FFT window size?
- This is still done at compile time in testfft.h as "N" I believe.  This will be a menu item like other
   options at a later time.  Any help on this is appreciated.