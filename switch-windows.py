#!/usr/bin/env python
import gi
import sys
import struct
import ctypes
import subprocess
import os

gi.require_version('Gtk', '3.0')
gi.require_version('Keybinder', '3.0')
from gi.repository import Gtk, Wnck, Gdk, GdkX11, GLib

BINARY_PATH="/home/mike/bin/switcher"

def switch_windows():
    Keybinder.unbind("<Alt>Tab")
    sw = subprocess.Popen(BINARY_PATH, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    
    screen = Wnck.Screen.get_default()
    screen.force_update();
    windows = sorted(filter(lambda w: w.get_class_instance_name() and not w.get_class_instance_name().startswith('FvwmButtons'), screen.get_windows()), key=lambda w: w.get_sort_order())

    if len(windows) > 2:
        t = windows[0]
        windows[0] = windows[1]
        windows[1] = t
        
    # for w in windows:
    #     print('%s %d' % (w.get_name(), w.get_sort_order()))
    
    i = 0
    for w in windows:
        i += 1
        sw.stdin.write(struct.pack('i', i))
        name = w.get_name()
        icon = w.get_icon()
        # sys.stderr.write('%s\n' % (w.get_class_instance_name()))
        # sys.stderr.write('%s\n' % (w.is_active()))
        buf = icon.save_to_bufferv('png', [], [])

        sw.stdin.write(struct.pack('i', len(name)))
        sw.stdin.write(struct.pack('i', len(buf[1])))
        sw.stdin.write(name)
        sw.stdin.write(buf[1])
    sw.stdin.write(struct.pack('i', 0))
    sw.stdin.flush()

    sel = int(sw.stdout.readline())
    if sel != 0:
        w = windows[sel - 1]
        windows.remove(w)
        windows.insert(0, w)
        w.activate(0)
        for i in range(len(windows)):
            windows[i].set_sort_order(i)

if __name__ == '__main__':
    Gtk.init()
    Gtk.main()
