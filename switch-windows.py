import gi
import sys
import struct
import ctypes

gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Wnck


if __name__ == '__main__':
    Gtk.init()
    screen = Wnck.Screen.get_default()
    screen.force_update();
    i = 0
    for w in screen.get_windows():
        i += 1
        sys.stdout.write(struct.pack('i', i))
        name = w.get_name()
        icon = w.get_icon()
        sys.stderr.write("%s\n" % (w.get_class_instance_name()))
        buf = icon.save_to_bufferv('png', [], [])

        sys.stdout.write(struct.pack('i', len(name)))
        sys.stdout.write(struct.pack('i', len(buf[1])))
        sys.stdout.write(name)
        sys.stdout.write(buf[1])
    sys.stdout.write(struct.pack('i', 0))
