#!/usr/bin/python

import gi
import sys
import struct
import ctypes
import subprocess
import os
import time
import signal

gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Wnck, Gdk, GdkX11, GLib

from appspec import AppSpecs

BINARY_PATH="/home/mike/bin/switcher"

class WindowSwitcher(object):

    def __init__(self):
        self.screen = Wnck.Screen.get_default()
        self.screen.connect('active-window-changed', self.on_window_change)
        self.windows = []
        self.spec = AppSpecs()
        self.spec.build_wmclass_index()
        self.window_id_map = {}
        self.id_window_map = {}
        self.free_id = set([])
        self.listing = False

    def refresh_windows(self):
        if self.listing:
            return

        self.screen.force_update()
        self.windows = sorted(filter(lambda w: w.get_class_instance_name() and not w.get_class_instance_name().startswith('FvwmButtons'), self.screen.get_windows()), key=lambda w: w.get_sort_order())

        for i in range(len(self.windows)):
            if self.windows[i].is_active():
                w = self.windows[i]
                self.windows.remove(w)
                self.windows.insert(0, w)

        # assigning IDs
        window_set = set(self.windows)
        for w, i in self.window_id_map.items():
            if w in window_set:
                continue
            del self.window_id_map[w]
            del self.id_window_map[i]
            self.free_id.add(i)

        for w in self.windows:
            if w in self.window_id_map:
                continue

            i = len(self.window_id_map) + 1
            if len(self.free_id) > 0:
                i = self.free_id.pop()

            self.window_id_map[w] = i
            self.id_window_map[i] = w

    def flush_windows(self):
        if self.listing:
            return

        for i in range(len(self.windows)):
            self.windows[i].set_sort_order(i)

    def on_window_change(self, screen, prev_window):
        if self.listing:
            return

        self.refresh_windows()
        self.flush_windows()

    def list_windows(self):
        self.refresh_windows()

        if len(self.windows) > 1:
            t = self.windows[0]
            self.windows[0] = self.windows[1]
            self.windows[1] = t

        for w in self.windows:
            name = w.get_name()
            icon = w.get_icon()
            # sys.stderr.write('%s\n' % (w.get_class_instance_name()))
            # sys.stderr.write('%s\n' % (w.is_active()))
            sys.stderr.write('%s id %d\n' % (w.get_name(), self.window_id_map[w]))
            # buf = icon.save_to_bufferv('png', [], [])[1]
            buf = None
            e = self.spec.find_by_wmclass(w.get_class_instance_name())
            if e and e.icon_path:
                buf = file(e.icon_path).read()
            else:
                buf = icon.save_to_bufferv('png', [], [])[1]
            sys.stdout.write(struct.pack('i', self.window_id_map[w]))
            sys.stdout.write(struct.pack('i', len(name)))
            sys.stdout.write(struct.pack('i', len(buf)))
            sys.stdout.write(name)
            sys.stdout.write(buf)
        sys.stdout.write(struct.pack('i', 0))
        sys.stdout.flush()

        self.listing = True

    def select_windows(self, channel):
        sel = int(channel.readline())
        sys.stderr.write('selecting %d' % (sel - 1,))
        if sel == 0:
            return

        w = self.id_window_map[sel]
        w.activate(0)
        self.listing = False
        return

def dispatch_io(channel, condition, sw):
    line = channel.readline()
    sys.stderr.write('line: ' + line)
    if line == 'list\n':
        sw.list_windows()
    elif line == 'select\n':
        sw.select_windows(channel)
    else:
        sys.stderr.write('Unknown command: %s' % line)
    return True

if __name__ == '__main__':
    signal.signal(signal.SIGPIPE, signal.SIG_IGN)
    Gtk.init()
    chn = GLib.IOChannel(0)
    sw = WindowSwitcher()
    GLib.io_add_watch(chn, 0, GLib.IO_IN, dispatch_io, sw)
    Gtk.main()
