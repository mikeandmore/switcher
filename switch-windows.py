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
        self.spec = AppSpecs()
        self.window_id_map = {}
        self.id_window_map = {}
        self.free_id = set([])
        self.screen.force_update()
        self.windows = sorted(self.screen.get_windows(), key=lambda w: w.get_sort_order())
        self.adjust_active_order()
        self.last_action = None

        for w, i in zip(self.windows, range(len(self.windows))):
            self.window_id_map[w] = i + 1
            self.id_window_map[i + 1] = w

        self.screen.connect('active-window-changed', self.on_window_change)
        self.screen.connect('window-opened', self.on_window_open)
        self.screen.connect('window-closed', self.on_window_close)
        
    def adjust_active_order(self):
        for i in range(len(self.windows)):
            if self.windows[i].is_active():
                w = self.windows[i]
                self.windows.remove(w)
                self.windows.insert(0, w)

        for i in range(len(self.windows)):
            sys.stderr.write('Flushing: >>> %s\n' % self.windows[i].get_name())
            self.windows[i].set_sort_order(i)

    def try_first_window(self, exclude=[]):
        if not any(map(lambda w: w.is_active(), self.windows)):
            for w in self.windows:
                if w in exclude:
                    continue
                if not w.is_minimized():
                    return w
        return None

    def on_window_change(self, screen, prev_window):
        if not prev_window:
            self.adjust_active_order()
            return
        if screen.get_active_workspace() != prev_window.get_workspace():
            return

        sys.stderr.write('Prev %s\n' % prev_window.get_name())
        w = self.try_first_window(exclude=[prev_window])
        if w != None:
            w.activate(time.time())
        self.adjust_active_order()

    def on_window_open(self, screen, window):
        self.windows.insert(0, window)
        new_id = len(self.windows) + 1
        if len(self.free_id) > 0:
            new_id = self.free_id.pop()
        self.id_window_map[new_id] = window
        self.window_id_map[window] = new_id

    def on_window_close(self, screen, window):
        old_id = self.window_id_map[window]
        self.windows.remove(window)
        del self.id_window_map[old_id]
        del self.window_id_map[window]
        self.free_id.add(old_id)

    def on_list(self):
        candidates = filter(lambda w: not w.get_workspace() or w.get_workspace() == self.screen.get_active_workspace(), self.windows)
        if len(candidates) > 1:
            t = candidates[0]
            candidates[0] = candidates[1]
            candidates[1] = t

        with self.spec.lock:
            for w in candidates:
                name = w.get_name()
                icon = w.get_icon()
                # sys.stderr.write('%s\n' % (w.get_class_instance_name()))
                # sys.stderr.write('%s\n' % (w.is_active()))
                sys.stderr.write('>>> %s id %d %s\n' % (w.get_name(), self.window_id_map[w], w.get_workspace()))
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
        if sel == 0:
            return

        w = self.id_window_map[sel]
        w.activate(time.time())

if __name__ == '__main__':
    run_switcher(WindowSwitcher())
