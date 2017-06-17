#!/usr/bin/python

import sys
import re
import os
from appspec import AppSpecs
from switcher import run_switcher, write_item, SwitcherItem

class Launcher(object):
    def __init__(self):
        self.specs = AppSpecs()
        self.apps = None

    def on_list(self):
        with self.specs.lock:
            self.apps = self.specs.apps
        i = 1
        items = []
        for app in self.apps:
            if app.icon_path:
                sys.stderr.write('%s\n' % app.getName().encode('utf-8'))
                items.append(SwitcherItem(id=i, \
                                          name=app.getName().encode('utf-8'), \
                                          png_buf=file(app.icon_path).read()))
            i += 1
        items = sorted(items, key=lambda x: x.name)
        for item in items:
            write_item(item)
        write_item(None)

    def on_select(self, channel):
        sel_id = int(channel.readline())
        cmd = re.sub('\%[a-zA-Z]', r'', self.apps[sel_id - 1].getExec())
        os.system('nohup %s > /dev/null 2> /dev/null &' % cmd)

if __name__ == '__main__':
    run_switcher(Launcher())
