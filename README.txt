rFactor2 Delta Best plugin (v9 "Imola")
=======================================

Author:      Cosimo <cosimo@streppone.it>
Last update: 2014/05/08
URL:         http://isiforums.net/f/showthread.php/19517-Delta-Best-plugin-for-rFactor-2
Status:      Works!

== What is this? ==

This is a proof of concept plugin for a Delta Best or Delta Split, or
whatever you want to call it.

The idea is not new at all. At least iRacing has something like this,
and I haven't seen it done yet for rF2.
Maybe TrackMap plugin has this, but I don't use TrackMap,
so I don't know.

== Proof! ==

Screenshot or it didn't happen:

  http://i.imgur.com/G3WvRjf.jpg

Even video:

  http://www.youtube.com/watch?v=FKzknrxRU8s

The idea is that you are shown in realtime the difference (split)
between the current lap and your best lap of the current session.

== Installation ==

Copy the content of the zip file into your rFactor 2
plugins folder, most likely something like this:

  "C:\Program files (x86)\rFactor2\Plugins"

You can omit this README.txt.

For the INI file to be used, copy the provided example
called "DeltaBest.example.ini" into "DeltaBest.ini"
and modify at will.

If you don't rename it, it won't be used.

== Status ==

Currently it works. It is quite accurate, but sometimes the
delta time reading goes off for an instant. That has to be fixed.
There's also a dynamic colored bar that shows exactly where
you are losing (red) or gaining time (green), regardless of your
actual difference to best lap.

That is quite an effective way to improve your lap times,
and get to learn a track.

== Customizing plugin position and appearance ==

Version 9 is completely configurable via an ini file.

You can customise how the time appears on screen by creating
this INI file. The INI file must be called "DeltaBest.ini"
and must be placed in the same Plugins folder as the "DeltaBest.dll"
file itself.

An example of INI file that you can change at will is bundled
in this zip file as "DeltaBest.example.ini". If you want to customise
the plugin, copy that file in "DeltaBest.ini" and change at will.

Examples of things you can do:

- customize the font face and size of the delta time text
- switch off the dynamic colored bar and leave just the time text
- switch off the delta time and leave just the bar
- modify the default shortcut key to toggle the display of
  the plugin (default is <CTRL> + d)
- modify position, width and height of either the bar or the text

== For feedback ==

Feedback on bugs, feature requests, generous donations :)
etc... are appreciated:

  http://isiforums.net/f/showthread.php/19517-Delta-Best-plugin-for-rFactor-2

Have fun,

-- 
Cosimo
