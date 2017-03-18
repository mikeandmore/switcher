#!/usr/bin/python

import gi
import sys
import time

from gi.repository import Wnck

from appspec import AppSpecs
from switcher import SwitcherItem, run_switcher, write_item

class WindowSwitcher(object):

    def __init__(self):
        self.screen = Wnck.Screen.get_default()
        self.screen.connect('active-window-changed', self.on_window_change)
        self.windows = []
        self.spec = AppSpecs()
        self.window_id_map = {}
        self.id_window_map = {}
        self.free_id = set([])
        self.screen.force_update()

    def refresh_windows(self):
        sys.stderr.write('Event: refreshing\n')
        self.windows = sorted(filter(lambda w: w.get_class_instance_name() and not w.get_class_instance_name().startswith('FvwmButtons'), self.screen.get_windows()), key=lambda w: w.get_sort_order())

        for i in range(len(self.windows)):
            if self.windows[i].is_active():
                w = self.windows[i]
                sys.stderr.write('%s is active\n' % w.get_name())
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
        sys.stderr.write('Event: flushing\n')

        if len(self.windows) > 1 and not self.windows[0].is_active():
            t = self.windows[0]
            del self.windows[0]
            self.windows.append(t)
            self.windows[0].activate(time.time())

        for i in range(len(self.windows)):
            self.windows[i].set_sort_order(i)

    def on_window_change(self, screen, prev_window):
        sys.stderr.write('Event: window change\n')
        self.refresh_windows()
        self.flush_windows()

    def on_list(self):
        self.refresh_windows()

        if len(self.windows) > 1:
            t = self.windows[0]
            self.windows[0] = self.windows[1]
            self.windows[1] = t

        with self.spec.lock:
            for w in self.windows:
                name = w.get_name()
                icon = w.get_icon()
                # sys.stderr.write('%s\n' % (w.get_class_instance_name()))
                # sys.stderr.write('%s\n' % (w.is_active()))
                sys.stderr.write('>>> %s id %d\n' % (w.get_name(), self.window_id_map[w]))
                # buf = icon.save_to_bufferv('png', [], [])[1]
                buf = None
                e = self.spec.find_by_wmclass(w.get_class_instance_name())
                if e and e.icon_path:
                    buf = file(e.icon_path).read()
                else:
                    buf = icon.save_to_bufferv('png', [], [])[1]
                write_item(SwitcherItem(id=self.window_id_map[w], name=name, png_buf=buf))
            write_item(None)

    def on_select(self, channel):
        sel = int(channel.readline())
        sys.stderr.write('Event: selecting %d' % (sel - 1,))
        if sel == 0:
            return

        w = self.id_window_map[sel]
        sys.stderr.write('activating %s\n' % w.get_name())
        w.activate(time.time())

if __name__ == '__main__':
    run_switcher(WindowSwitcher())
