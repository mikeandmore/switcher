#!/usr/bin/python

from xdg import BaseDirectory, IconTheme
from xdg.DesktopEntry import DesktopEntry

import subprocess
import os
import re

class AppSpecs(object):
    def __init__(self):
        self.apps = []
        for data_dir in BaseDirectory.xdg_data_dirs:
            app_dir = data_dir + '/applications/'
            for root, dirs, files in os.walk(app_dir):
                for filename in files:
                    if not filename.endswith('.desktop'):
                        continue
                    pathname = app_dir + filename
                    e = DesktopEntry(pathname)
                    e.icon_path = None
                    if e.getIcon():
                        e.icon_path = IconTheme.getIconPath(e.getIcon(), extensions=['png'])
                    self.apps.append(e)

    def build_wmclass_index(self):
        self.wmclass_index = {}
        for e in self.apps:
            # Some special cases
            if e.getName() == 'Google Chrome':
                self.wmclass_index['google-chrome'] = e
                continue

            if e.getStartupWMClass() == '' or e.getIcon() == '':
                continue
            self.wmclass_index[e.getStartupWMClass()] = e

    def find_by_wmclass(self, klass):
        return self.wmclass_index.get(klass)

if __name__ == '__main__':
    spec = AppSpecs()
    spec.build_wmclass_index()

    nameidx = {}
    for app in spec.apps:
        nameidx[app.getName()] = app.getExec()

    proc = subprocess.Popen(['dmenu', '-i'], stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    for k, v in nameidx.items():
        proc.stdin.write('%s\n' % k.encode('utf-8'))
    proc.stdin.close()

    name = proc.stdout.read()[:-1]
    cmd = re.sub(r'\%[a-zA-Z]', r'', nameidx[name])
    os.system('nohup %s > /dev/null 2> /dev/null &' % cmd)
