#!/usr/bin/env python
import gi
import sys
import struct
import ctypes
import subprocess
import os

gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Wnck, Gdk, GdkX11, GLib

BINARY_PATH="/home/mike/bin/switcher"

class WindowSwitcher(object):

    def __init__(self):
        self.screen = Wnck.Screen.get_default()
        self.windows = []

    def list_windows(self):
        self.screen.force_update();
        self.windows = sorted(filter(lambda w: w.get_class_instance_name() and not w.get_class_instance_name().startswith('FvwmButtons'), self.screen.get_windows()), key=lambda w: w.get_sort_order())

        if len(self.windows) > 2:
            t = self.windows[0]
            self.windows[0] = self.windows[1]
            self.windows[1] = t
    
        i = 0
        for w in self.windows:
            i += 1
            sys.stdout.write(struct.pack('i', i))
            name = w.get_name()
            icon = w.get_icon()
            # sys.stderr.write('%s\n' % (w.get_class_instance_name()))
            # sys.stderr.write('%s\n' % (w.is_active()))
            buf = icon.save_to_bufferv('png', [], [])

            sys.stdout.write(struct.pack('i', len(name)))
            sys.stdout.write(struct.pack('i', len(buf[1])))
            sys.stdout.write(name)
            sys.stdout.write(buf[1])
        sys.stdout.write(struct.pack('i', 0))
        sys.stdout.flush()

    def select_windows(self, channel):
        sel = int(channel.readline())
        sys.stderr.write('selecting %d' % (sel - 1,))
        if sel != 0:
            w = self.windows[sel - 1]
            self.windows.remove(w)
            self.windows.insert(0, w)
            w.activate(0)
            for i in range(len(self.windows)):
                self.windows[i].set_sort_order(i)

def dispatch_io(channel, condition, sw):
    line = channel.readline()
    sys.stderr.write(line)
    if line == 'list\n':
        sw.list_windows()
    elif line == 'select\n':
        sw.select_windows(channel)
    else:
        sys.stderr.write('Unknown command: %s' % line)
    return True
        
if __name__ == '__main__':
    Gtk.init()
    chn = GLib.IOChannel(0)
    sw = WindowSwitcher()    
    GLib.io_add_watch(chn, 0, GLib.IO_IN, dispatch_io, sw)
    Gtk.main()
