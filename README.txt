rFactor2 Delta Best plugin (v24 "Nola")
=============================================

Author:      Cosimo <cosimo@streppone.it>
Last update: 2017/02/25
URL:         https://forum.studio-397.com/index.php?threads/delta-best-plugin-for-rfactor-2.43399/
Status:      Works!

== What is this? ==

This is a plugin for rFactor2 to add a Delta to Best Lap or Delta Split,
or whatever you want to call it to rF2.

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

== Changelog ==

2017/02/25 - v24/Nola

    Fixed a cosmetic bug in how the best lap time is displayed
    when a session is loaded or when a new best lap is saved.
    A laptime of "2:02.058" would be displayed as "2:2.058".

2016/09/25 - v23/Anderstorp

    (v21 and v22 were just test versions with limited distribution)

    Fixed a few logic mistakes in how the best lap is saved. Now the
    best lap time should be saved more accurately.

    Added a chat message when a best lap is restored from file, so
    you will know what is your best time in the same car/track
    combination.

    Changed how the Delta info is shown. Now it's never shown when
    the player is in the pits, and should be there from the first
    timed lap whenever you already have a saved lap from before.

2015/10/24 - v20/RoadAmerica

    Fixed the activation logic for the plugin. Since v19 sometimes the
    plugin would start to show in monitor mode as well.

2015/10/23 - v19/Nords

    Plugin will now work after a race restart.
    Possibly also when doing driver swaps, but I haven't been able to
    fully test that case yet.

2015/08/19 - v18/Targa
2015/08/02 - v17/Indy (never released publicly)

    Best laps now persist to disk, so when you start a new
    session but you already had a best lap, this is automatically restored
    from disk, and you can get the delta best timing straight away from
    the first lap.

    The Delta Best lap files will be saved in the rFactor2 user folder
    into the "rFactor2\Userdata\player\Settings\DeltaBest" path.
    If that path doesn't exist, it will be created.

== Installation ==

Copy the content of the zip file (all files, maybe except
the README.txt) into your rFactor2 plugins folder,
most likely a directory like this:

  "D:\Steam\SteamApps\common\rFactor 2\Bin64\Plugins"

You can omit this README.txt.

For the INI file to be used, copy the provided example
called "DeltaBest.example.ini" into "DeltaBest.ini"
and modify at will.

If you don't rename it, it won't be used.

== Note for 32-bit builds ==

32-bit builds are not supported anymore. 64-bit builds are the only
practical choice nowadays.

== Status ==

Currently it works. It is quite accurate, but sometimes the
delta time reading goes off for an instant.
There's also a dynamic colored bar that shows exactly where
you are losing (red) or gaining time (green), regardless of your
actual difference to best lap. Both the bar and the time can
be disabled via the ini file.

That is quite an effective way to improve your lap times,
and get to learn a track.

== Customizing plugin position and appearance ==

From version 9 the plugin is completely configurable via an ini file.

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

  https://forum.studio-397.com/index.php?threads/delta-best-plugin-for-rfactor-2.43399/

Have fun,

-- 
Cosimo
