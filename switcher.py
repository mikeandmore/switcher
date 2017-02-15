import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Wnck, Gdk, GdkX11, GLib
import signal
import sys
import struct
from collections import namedtuple

SwitcherItem = namedtuple('SwitcherItem', ['id', 'name', 'png_buf'])

def write_item(sw_item):
    if sw_item == None:
        sys.stdout.write(struct.pack('i', 0))
        sys.stdout.flush()
        return
    sys.stdout.write(struct.pack('i', sw_item.id))
    sys.stdout.write(struct.pack('i', len(sw_item.name)))
    sys.stdout.write(struct.pack('i', len(sw_item.png_buf)))
    sys.stdout.write(sw_item.name)
    sys.stdout.write(sw_item.png_buf)

def run_switcher(sw):
    def dispatch_io(channel, condition, sw):
        line = channel.readline()
        sys.stderr.write('line: ' + line)
        if line == 'list\n':
            sw.on_list()
        elif line == 'select\n':
            sw.on_select(channel)
        else:
            sys.stderr.write('Unknown command: %s' % line)
        return True

    signal.signal(signal.SIGPIPE, signal.SIG_IGN)
    Gtk.init()
    chn = GLib.IOChannel(0)
    GLib.io_add_watch(chn, 0, GLib.IO_IN, dispatch_io, sw)
    Gtk.main()
