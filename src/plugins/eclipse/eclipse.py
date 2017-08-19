1#
# Entangle: Tethered Camera Control & Capture
#
# Copyright (C) 2014 Daniel P. Berrange
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import time

import gi

from gi.repository import GObject
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GLib
from gi.repository import Gio
from gi.repository import Peas
gi.require_version("PeasGtk", "1.0")
from gi.repository import PeasGtk
gi.require_version("Entangle", "0.1")
from gi.repository import Entangle


ISO_APERTURE_TO_INDEX_MAP = [
    ["25",   ["1.4", "2",   "2.8", "4",   "5.6", "8",  "11", "16",  "22"]],
    ["50",   ["2",   "2.8", "4",   "5.6", "8",   "11", "16", "22",  "32"]],
    ["100",  ["2.8", "4",   "5.6", "8",   "11",  "16", "22", "32",  "44"]],
    ["200",  ["4",   "5.6", "8",   "11",  "16",  "22", "32", "44",  "64"]],
    ["400",  ["5.6", "8",   "11",  "16",  "22",  "32", "44", "64",  "88"]],
    ["800",  ["8",   "11",  "16",  "22",  "32",  "44", "64", "88",  "128"]],
    ["1600", ["11",  "16",  "22",  "32",  "44",  "64", "88", "128", "176"]],
]

INDEX_TO_SPEED_MAP = [
    [
        "1/2000",
        "1/500",
        "1/125",
        "1/30",
        "1/15",
        "1/8",
        "1/2",
    ],
    [
        "1/1000",
        "1/250",
        "1/60",
        "1/15",
        "1/8",
        "1/4",
        "1",
    ],
    [
        "1/500",
        "1/125",
        "1/30",
        "1/8",
        "1/4",
        "1/2",
        "2",
    ],

    [
        "1/250",
        "1/60",
        "1/15",
        "1/4",
        "1/2",
        "1",
        "4",
    ],
    [
        "1/125",
        "1/30",
        "1/8",
        "1/2",
        "1",
        "2",
        "8",
    ],
    [
        "1/60",
        "1/15",
        "1/4",
        "1",
        "2",
        "4",
        "15",
    ],

    [
        "1/30",
        "1/8",
        "1/2",
        "2",
        "4",
        "8",
        "30",
    ],
    [
        "1/15",
        "1/4",
        "1",
        "4",
        "8",
        "15",
        "60",
    ],
    [
        "1/8",
        "1/2",
        "2",
        "8",
        "15",
        "30",
        "120",
    ]
]


class EclipsePluginWidget(Gtk.Grid):
    '''The form for controlling parameters of
    the batch mode shooting session and the progress'''

    def do_iso_changed(self, data):
        self.do_aperture_populate()
        self.config.set_iso(self.iso.get_active_text())

    def do_aperture_changed(self, data):
        self.config.set_aperture(self.aperture.get_active_text())

    def do_aperture_populate(self):
        isoidx = self.iso.get_active()
        if isoidx == -1:
            self.aperture.set_sensitive(False)
        else:
            self.aperture.set_sensitive(True)

            self.aperture.remove_all()
            defset = False
            for apertureidx in range(len(ISO_APERTURE_TO_INDEX_MAP[isoidx][1])):
                aperture = ISO_APERTURE_TO_INDEX_MAP[isoidx][1][apertureidx]
                self.aperture.append(aperture, aperture)
                if aperture == self.config.get_aperture():
                    self.aperture.set_active(apertureidx)
                    defset = True
            if not defset:
                self.aperture.set_active(0)

    def __init__(self, config):
        super(EclipsePluginWidget, self).__init__()

        self.config = config

        self.set_properties(column_spacing=6,
                            row_spacing=6,
                            expand=False)

        self.attach(Gtk.Label("ISO:"), 0, 0, 1, 1)
        self.iso = Gtk.ComboBoxText()

        for isoidx in range(len(ISO_APERTURE_TO_INDEX_MAP)):
            iso = ISO_APERTURE_TO_INDEX_MAP[isoidx][0]
            self.iso.append(iso, iso)
            if iso == config.get_iso():
                self.iso.set_active(isoidx)
        self.iso.set_properties(expand=True)
        self.iso.connect("changed", self.do_iso_changed)
        self.attach(self.iso, 1, 0, 1, 1)

        self.attach(Gtk.Label("Aperture:"), 0, 1, 1, 1)
        self.aperture = Gtk.ComboBoxText()
        self.do_aperture_populate()
        self.aperture.connect("changed", self.do_aperture_changed)
        self.attach(self.aperture, 1, 1, 1, 1)

        self.show_all()

class EclipsePluginData(GObject.Object):

    __gtype_name__ = "EclipsePluginData"

    def _get_speeds(self, iso, aperture):
        for isoidx in range(len(ISO_APERTURE_TO_INDEX_MAP)):
            thisiso = ISO_APERTURE_TO_INDEX_MAP[isoidx][0]
            if thisiso != iso:
                continue

            for apertureidx in range(len(ISO_APERTURE_TO_INDEX_MAP[isoidx][1])):
                thisaperture = ISO_APERTURE_TO_INDEX_MAP[isoidx][1][apertureidx]
                if thisaperture != aperture:
                    continue

                return INDEX_TO_SPEED_MAP[apertureidx]

            return []

        return []

    def __init__(self, iso, aperture):
        super(EclipsePluginData, self).__init__()

        self.speeds = self._get_speeds(iso, aperture)
        self.speedIndex = 0
        self.automata = None

    def speed(self):
        speed = self.speeds[self.speedIndex]
        self.speedIndex = self.speedIndex + 1
        return speed

    def finished(self):
        return self.speedIndex == len(self.speeds)

    def reset(self):
        self.speedIndex = 0


class EclipsePluginScript(Entangle.ScriptSimple):
    '''The script for controlling the camera'''

    def __init__(self, config):
        super(EclipsePluginScript, self).__init__(
            title="Eclipse Totality"
        )

        self.config = config
        self.widget = EclipsePluginWidget(config)

    def do_get_config_widget(self):
        return self.widget

    def do_capture_callback(self, automata, result, script_result):
        try:
            automata.capture_finish(result)
        except GLib.Error as e:
            self.return_task_error(script_result, str(e));
            return

        if script_result.return_error_if_cancelled():
            return

        cancel = script_result.get_cancellable()
        self.do_next_step(cancel, script_result)

    def do_init_task_data(self):
        return EclipsePluginData(self.config.get_iso(),
                                 self.config.get_aperture())

    def do_next_step(self, cancel, script_result):
        data = self.get_task_data(script_result)
        if data.finished():
            script_result.return_boolean(True)
            return

        automata = script_result.automata
        camera = automata.get_camera()
        controls = camera.get_controls()
        widget = controls.get_by_path("/main/capturesettings/shutterspeed2")
        speed = data.speed()
        widget.set_property("value", speed)
        camera.save_controls_async(cancel, self.do_save_callback, script_result)

    def do_save_callback(self, camera, result, script_result):
        try:
            camera.save_controls_finish(result)
        except GLib.Error as e:
            self.return_task_error(script_result, str(e));
            return

        if script_result.return_error_if_cancelled():
            return

        automata = script_result.automata
        automata.capture_async(script_result.get_cancellable(),
                               self.do_capture_callback,
                               script_result)

    def do_execute(self, automata, cancel, result):
        result.automata = automata

        self.do_next_step(cancel, result)


class EclipsePluginConfig():
    '''Provides integration with GSettings to read/write
    configuration parameters'''

    def __init__(self, plugin_info):
        settingsdir = plugin_info.get_data_dir() + "/schemas"
        sssdef = Gio.SettingsSchemaSource.get_default()
        sss = Gio.SettingsSchemaSource.new_from_directory(settingsdir, sssdef, False)
        schema = sss.lookup("org.entangle-photo.plugins.eclipse", False)
        self.settings = Gio.Settings.new_full(schema, None, None)

    def get_iso(self):
        return self.settings.get_string("iso")

    def set_iso(self, iso):
        self.settings.set_string("iso", iso)

    def get_aperture(self):
        return self.settings.get_string("aperture")

    def set_aperture(self, fnum):
        self.settings.set_string("aperture", fnum)


class EclipsePlugin(GObject.Object, Peas.Activatable):
    '''Handles the plugin activate/deactivation and
    tracking of camera manager windows. When a window
    appears, it enables the eclipse functionality on
    that window'''
    __gtype_name__ = "EclipsePlugin"

    object = GObject.property(type=GObject.Object)

    def __init__(self):
        GObject.Object.__init__(self)
        self.wins = []
        self.winsigadd = None
        self.winsigrem = None
        self.config = None
        self.script = None

    def do_activate_window(self, win):
        if not isinstance(win, Entangle.CameraManager):
            return

        win.add_script(self.script)
        self.wins.append(win)

    def do_deactivate_window(self, win):
        if not isinstance(win, Entangle.CameraManager):
            return

        win.remove_script(self.script)
        oldwins = self.wins
        self.wins = []
        for w in oldwins:
            if w != win:
                self.wins.append(w)

    def do_activate(self):
        if self.config is None:
            self.config = EclipsePluginConfig(self.plugin_info)
            self.script = EclipsePluginScript(self.config)

        # Windows can be dynamically added/removed so we
        # must track this
        self.winsigadd = self.object.connect(
            "window-added",
            lambda app, win: self.do_activate_window(win))
        self.winsigrem = self.object.connect(
            "window-removed",
            lambda app, win: self.do_deactivate_window(win))

        for win in self.object.get_windows():
            self.do_activate_window(win)

    def do_deactivate(self):
        self.object.disconnect(self.winsigadd)
        self.object.disconnect(self.winsigrem)
        for win in self.object.get_windows():
            self.do_deactivate_window(win)
        self.config = None
        self.script = None
