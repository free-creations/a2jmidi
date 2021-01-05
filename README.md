# a2jmidi

A one-way static bridge, connecting ALSA-MIDI to JACK-MIDI.

Mostly useful for those needing a stable, time-accurate link that connects JACK-based software to 
MIDI Hardware or to ALSA-MIDI-based software. 

Incoming MIDI events will be detected within less than a millisecond 
and will be aligned into the JACK-buffer 
with sample precision. 

## Usage
To create a new bridge, open a terminal-window and do:

```console
$ a2jmidi  [options] [name]
```
Where `name` is a user chosen name for the bridge.
The given name will be used to label the _client_ and also the _port_. If there is already a
_client_ with the given name, the bridges name will be made unique by appending a number.
A name containing spaces must be enclosed into quotation marks.

Allowed options are:

- __`-h [ --help ]`__ display help and exit
- __`-v [ --version ]`__ display version information and exit
- __`-s [ --startjack ]`__ try to start the JACK server if not already running
- __`-c [ --connect ] source-identifier`__ watch for a specified ALSA-sequencer-port  
and connect to it as soon as it becomes available
  
The `source-identifier` can be specified as the combination of _client-number_ and _port-number_
such as `28:0` or the label of a port such as `"USB-MIDI MIDI 1"`.

To stop the bridge, shutdown the JACK server or do `ctrl-c`.

## Example 1 
Start the JACK-server with [QjackCtl](https://qjackctl.sourceforge.io/),
then open a terminal and do: 

```console
$ a2jmidi "My Midi port"
```
In the MIDI-window of _QjackCtl_ we will see a new JACK-Midi client called `My Midi port`.
this is the __JACK side__ of the bridge.

![new JACK-Midi client](doc/img/screenshot03.png "new JACK-Midi client")   
 
The following screenshot shows the ALSA-MIDI connections.
We see a new ALSA-Midi-client called `My Midi port`, this is the 
__ALSA side__ of the bridge.
 
- Note: the panel labeled "ALSA" appears in the connections-window 
of _QjackCtl_, only when we have 
activated the _ALSA Sequencer support_ in the _Misc_ Tab of the _QjackCtl_-Setup.

![new ALSA-Midi client](doc/img/screenshot02.png "new ALSA-Midi client")

The ALSA- and the JACK- ports remain available as long as the  JACK server 
runs. Even after a short break of system-hibernation the bridge should restart
to work normally.

Now we have a permanent ALSA to JACK bridge which
can be connected by applications that produce ALSA-MIDI events. Like
for example [Frescobaldi](https://www.frescobaldi.org/). Frescobaldi is a 
tool to create professional looking sheet music. The music composition can be proof-read 
(proof-listened?) by listened the generated MIDI.
Below, we see how to connect Frescobaldi's midi output to 
our `My Midi port` created above.

![Connecting to Midi-out of Frescobali](doc/img/frescobaldi.png "Connecting to Midi-out of Frescobali")

Finally, the JACK side port can be connected to a virtual MIDI synthesizer as in the example below:

![Connecting to Midi-In of Synthesizer](doc/img/screenshot05.png "Connecting to Midi-In of Synthesizer")

Now, we can hear our music compositions played on the virtual piano.

## Example 2 - Connecting USB Hardware

USB Midi hardware can be plugged on and off while the JACK server is running. 
_a2jmidi_ permits to setup connections to such USB Midi hardware independently
whether the hardware is connected or not.
 
_a2jmidi_ will watch the state of connections and automatically grasp a specific 
port as soon as it becomes available. To this end we need to know the port name 
under which the USB Hardware is known to ALSA. To find out, we can use
the command `aconnect -i` as in the example below:
```console
$ aconnect -i
client 0: 'System' [type=kernel]
    0 'Timer           '
    1 'Announce        '
client 14: 'Midi Through' [type=kernel]
    0 'Midi Through Port-0'
client 28: 'USB-MIDI' [type=kernel]
    0 'USB-MIDI MIDI 1 '
```
In this example we see, there is a device that
reports its MIDI output as _"USB-MIDI MIDI 1"_. 
This device is an USB piano keyboard that we want to 
include into our JACK Audio setup.

In order to have _a2jmidi_ to automatically 
connect to this device, we'll start it with the following command:
```console
$ a2jmidi "My Midi port" --connect "USB-MIDI MIDI 1"
```
In the screenshot below we see that the bridge called "My Midi port" 
is now connected to the "USB-MIDI MIDI 1" device.
![Automatically connecting to USB Hardware](doc/img/screenshot06.png "Automatically connecting to USB Hardware")

When the USB device is powered off, the bridge will stay alive. As soon as 
a MIDI device called "USB-MIDI MIDI 1" reappears, the bridge will reconnect to it.

Now we are able to set up complex audio-midi patches including our
USB piano keyboard, even 
before the keyboard is connected or is powered on.

# Example 3

In this example we will merge example 1 and example 2 into one
startup script.   

Create a file with the following content:
```shell script
#!/usr/bin/env bash

a2jmidi --name "Keyboard" --connect "USB-MIDI MIDI 1" & 
sleep 1
a2jmidi "Sequencer_A" & 
sleep 1 
a2jmidi "Sequencer_B" &  

```
Save this file as `.a2jmidi_setup` under your root and make it executable.

In _QjackCtl_ go to the Setup/Options panel. Activate `Execute Script after Startup`
and enter `$HOME/.a2jmidi_setup` as startup script.
![QjackCtl Setup](doc/img/screenshot08.png "QjackCtl Setup")

When we start the
[Carla-audio-plugin-host](https://github.com/falkTX/Carla)
we now see three midi plugs (the red ones) labeled "Keyboard", "Sequencer_A", "Sequencer_B".


![Carla with MIDI sources](doc/img/screenshot07.png "Carla with MIDI sources")

## Build and Install

Instructions on how to build and install can be found
in the document [INSTALL.md](INSTALL.md).

## Similar Tools

This is a remake of `a2jmidi_bridge` found in the official Ubuntu distribution.


